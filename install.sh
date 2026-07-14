#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
MODE=auto
INSTALL_DEPS=1
ASSUME_YES=0
BUILD_FROM_SOURCE=0
PREBUILT_DIR=

usage() {
    cat <<'EOF'
Usage: ./install.sh [OPTIONS]

By default SmartType downloads a checksum-verified prebuilt release. No
compiler is needed on the user's computer.

Options:
  --mode auto|kde-wayland|x11  Override graphical-session detection
  --skip-deps                  Do not install distribution runtime packages
  --build-from-source          Compile and test locally (developer fallback)
  --prebuilt-dir DIR           Install an already extracted release payload
  -y, --yes                    Pass non-interactive confirmation to dnf/apt
  -h, --help                   Show this help

Verified release environments:
  Fedora 44 KDE Plasma / Wayland (x86_64)
  Ubuntu 26.04 KDE Plasma / Wayland (x86_64)
  Kali Rolling Xfce / X11 (x86_64)
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
        --build-from-source) BUILD_FROM_SOURCE=1 ;;
        --prebuilt-dir)
            [[ $# -ge 2 ]] || { echo "--prebuilt-dir requires a value" >&2; exit 2; }
            PREBUILT_DIR=$(cd "$2" && pwd)
            shift
            ;;
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

detect_target() {
    case "${ID:-}" in
        fedora)
            [[ ${VERSION_ID:-} == 44 ]] || return 1
            echo fedora44
            ;;
        ubuntu)
            [[ ${VERSION_ID:-} == 26.04 ]] || return 1
            echo ubuntu2604
            ;;
        kali) echo kali-rolling ;;
        *) return 1 ;;
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

TARGET=$(detect_target) || {
    echo "Unsupported release target: ${PRETTY_NAME:-${ID:-unknown}}" >&2
    echo "See docs/INSTALL.md for the exact support matrix." >&2
    exit 1
}

if [[ $MODE == auto ]]; then MODE=$(detect_mode); fi
if [[ $MODE == unknown ]]; then
    echo "Cannot detect a supported graphical session." >&2
    echo "Run this inside KDE Wayland or X11, or pass --mode explicitly." >&2
    exit 1
fi

case "$TARGET:$MODE" in
    fedora44:kde-wayland|ubuntu2604:kde-wayland|kali-rolling:x11) ;;
    *)
        echo "${PRETTY_NAME:-$ID} with mode '$MODE' is not a verified release target." >&2
        exit 1
        ;;
esac

if [[ -z $PREBUILT_DIR && $BUILD_FROM_SOURCE -eq 0 ]]; then
    args=(--mode "$MODE")
    (( INSTALL_DEPS )) || args+=(--skip-deps)
    (( ASSUME_YES )) && args+=(--yes)
    exec "$ROOT/scripts/install-release.sh" "${args[@]}"
fi

install_runtime_dependencies() {
    case "$TARGET" in
        fedora44)
            command=(sudo dnf --setopt=install_weak_deps=False install)
            (( ASSUME_YES )) && command+=(-y)
            command+=(fcitx5 fcitx5-qt fcitx5-gtk fcitx5-configtool hunspell hunspell-ru glibc-gconv-extra
                qt6-qtbase qt6-qtdeclarative cairo pango gdk-pixbuf2 glib2 wayland-libs
                libxcb xcb-util xcb-util-wm xcb-util-keysyms)
            "${command[@]}"
            ;;
        ubuntu2604|kali-rolling)
            sudo apt-get update
            command=(sudo apt-get install)
            (( ASSUME_YES )) && command+=(-y)
            command+=(fcitx5 fcitx5-frontend-qt6 fcitx5-frontend-gtk3 fcitx5-config-qt
                hunspell hunspell-ru hunspell-en-us libqt6core6 libqt6gui6 libqt6widgets6 libqt6dbus6
                libqt6qml6 libcairo2 libpango-1.0-0 libgdk-pixbuf-2.0-0 libglib2.0-0
                libwayland-client0 libxcb1 libxcb-util1 libxcb-icccm4 libxcb-xinerama0
                libxcb-randr0 libxcb-ewmh2 libxcb-keysyms1)
            "${command[@]}"
            ;;
    esac
}

install_build_dependencies() {
    case "$TARGET" in
        fedora44)
            command=(sudo dnf --setopt=install_weak_deps=False install)
            (( ASSUME_YES )) && command+=(-y)
            command+=(fcitx5 fcitx5-devel fcitx5-qt fcitx5-gtk fcitx5-configtool glibc-gconv-extra
                hunspell hunspell-ru gcc-c++ cmake ninja-build sqlite-devel
                pkgconf-pkg-config cairo-devel pango-devel gdk-pixbuf2-devel glib2-devel
                wayland-devel wayland-protocols-devel qt6-qtbase-devel
                qt6-qtdeclarative-devel libxcb-devel xcb-util-devel xcb-util-wm-devel
                xcb-util-keysyms-devel)
            "${command[@]}"
            ;;
        ubuntu2604|kali-rolling)
            sudo apt-get update
            command=(sudo apt-get install)
            (( ASSUME_YES )) && command+=(-y)
            command+=(build-essential cmake ninja-build pkg-config libsqlite3-dev fcitx5
                libfcitx5core-dev libfcitx5utils-dev libfcitx5config-dev fcitx5-modules-dev
                fcitx5-frontend-qt6 fcitx5-frontend-gtk3 fcitx5-config-qt hunspell hunspell-ru hunspell-en-us
                qt6-base-dev qt6-declarative-dev libcairo2-dev libpango1.0-dev
                libgdk-pixbuf-2.0-dev libglib2.0-dev libwayland-dev wayland-protocols
                libxcb1-dev libxcb-util-dev libxcb-icccm4-dev libxcb-xinerama0-dev
                libxcb-randr0-dev libxcb-ewmh-dev libxcb-keysyms1-dev)
            "${command[@]}"
            ;;
    esac
}

if (( INSTALL_DEPS )); then
    echo "==> Installing SmartType dependencies for ${PRETTY_NAME:-$ID}"
    if (( BUILD_FROM_SOURCE )); then install_build_dependencies; else install_runtime_dependencies; fi
fi

install_args=()
[[ -n $PREBUILT_DIR ]] && install_args+=(--prebuilt-dir "$PREBUILT_DIR")
case "$MODE" in
    kde-wayland) install_args+=(--enable-kde-wayland-ime --enable-kde-layout-sync --disable-kimpanel) ;;
    x11) install_args+=(--enable-x11-layout-sync --disable-kimpanel) ;;
esac

echo "==> Configuring SmartType for $MODE"
"$ROOT/scripts/install-user.sh" "${install_args[@]}"

echo
echo "SmartType installation completed successfully."
echo "Log out and back in once; SmartType English will already be selected in Fcitx."
echo "After login, run: $HOME/.local/share/smarttype/doctor.sh"
