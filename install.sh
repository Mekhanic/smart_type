#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
MODE=auto
INSTALL_DEPS=1
ASSUME_YES=0

usage() {
    cat <<'EOF'
Usage: ./install.sh [--mode auto|kde-wayland|x11] [--skip-deps] [--yes]

Supported release environments:
  Fedora KDE Plasma / Wayland
  Ubuntu 26.04 KDE Plasma / Wayland
  Kali Linux Xfce / X11

The installer installs build/runtime dependencies, builds and tests SmartType,
installs it into ~/.local, configures Fcitx, and enables the tray services.
Log out and back in once after the first installation.
EOF
}

while (($#)); do
    case "$1" in
        --mode)
            [[ $# -ge 2 ]] || { echo "--mode requires a value" >&2; exit 2; }
            MODE=$2
            shift
            ;;
        --skip-deps) INSTALL_DEPS=0 ;;
        -y|--yes) ASSUME_YES=1 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

case "$MODE" in auto|kde-wayland|x11) ;; *) echo "Unsupported mode: $MODE" >&2; exit 2 ;; esac

[[ -r /etc/os-release ]] || { echo "Cannot identify this Linux distribution." >&2; exit 1; }
# shellcheck source=/dev/null
. /etc/os-release

install_dependencies() {
    case "${ID:-}" in
        fedora)
            command=(sudo dnf install)
            (( ASSUME_YES )) && command+=(-y)
            command+=(
                fcitx5 fcitx5-devel fcitx5-qt fcitx5-gtk fcitx5-configtool
                hunspell hunspell-ru gcc-c++ cmake ninja-build sqlite-devel
                pkgconf-pkg-config cairo-devel pango-devel gdk-pixbuf2-devel
                glib2-devel wayland-devel wayland-protocols-devel
                qt6-qtbase-devel qt6-qtdeclarative-devel libxcb-devel
                xcb-util-devel xcb-util-wm-devel xcb-util-keysyms-devel
            )
            "${command[@]}"
            ;;
        ubuntu|kali)
            if [[ ${ID:-} == ubuntu && ${VERSION_ID:-} != 26.04 ]]; then
                echo "Ubuntu ${VERSION_ID:-unknown} is not a live-verified release target." >&2
                echo "Use Ubuntu 26.04, or pass --skip-deps only for development testing." >&2
                exit 1
            fi
            sudo apt-get update
            command=(sudo apt-get install)
            (( ASSUME_YES )) && command+=(-y)
            command+=(
                build-essential cmake ninja-build pkg-config libsqlite3-dev
                fcitx5 libfcitx5core-dev libfcitx5utils-dev libfcitx5config-dev
                fcitx5-modules-dev fcitx5-frontend-qt6 fcitx5-frontend-gtk3
                fcitx5-config-qt hunspell hunspell-ru qt6-base-dev
                qt6-declarative-dev libcairo2-dev libpango1.0-dev
                libgdk-pixbuf-2.0-dev libglib2.0-dev libwayland-dev
                wayland-protocols libxcb1-dev libxcb-util-dev
                libxcb-icccm4-dev libxcb-xinerama0-dev libxcb-randr0-dev
                libxcb-ewmh-dev libxcb-keysyms1-dev
            )
            "${command[@]}"
            ;;
        *)
            echo "Unsupported distribution: ${PRETTY_NAME:-${ID:-unknown}}" >&2
            echo "See docs/INSTALL.md for the current support boundary." >&2
            exit 1
            ;;
    esac
}

detect_mode() {
    local session=${XDG_SESSION_TYPE:-}
    local desktop=${XDG_CURRENT_DESKTOP:-${XDG_SESSION_DESKTOP:-}}
    session=${session,,}
    desktop=${desktop,,}
    if [[ $session == wayland && $desktop == *kde* ]]; then
        echo kde-wayland
    elif [[ $session == x11 ]]; then
        echo x11
    else
        echo unknown
    fi
}

if [[ $MODE == auto ]]; then
    MODE=$(detect_mode)
fi
if [[ $MODE == unknown ]]; then
    echo "Cannot detect a supported graphical session." >&2
    echo "Run this from a terminal inside KDE Wayland or X11, or pass --mode explicitly." >&2
    exit 1
fi

case "${ID:-}:$MODE" in
    fedora:kde-wayland) ;;
    ubuntu:kde-wayland)
        if [[ ${VERSION_ID:-} != 26.04 ]]; then
            echo "Ubuntu ${VERSION_ID:-unknown} is not a live-verified release target." >&2
            exit 1
        fi
        ;;
    kali:x11) ;;
    *)
        echo "${PRETTY_NAME:-$ID} with mode '$MODE' is not a verified release target." >&2
        echo "See docs/INSTALL.md for the exact support matrix." >&2
        exit 1
        ;;
esac

if (( INSTALL_DEPS )); then
    echo "==> Installing SmartType dependencies for ${PRETTY_NAME:-$ID}"
    install_dependencies
fi

echo "==> Configuring SmartType for $MODE"
case "$MODE" in
    kde-wayland)
        "$ROOT/scripts/install-user.sh" \
            --enable-kde-wayland-ime --enable-kde-layout-sync --disable-kimpanel
        ;;
    x11)
        "$ROOT/scripts/install-user.sh" \
            --enable-x11-layout-sync --disable-kimpanel
        ;;
esac

echo
echo "SmartType installation completed successfully."
echo "Log out and back in once; SmartType English will already be selected in Fcitx."
echo "After login, run: $ROOT/scripts/doctor.sh"
