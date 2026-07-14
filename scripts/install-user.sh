#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BUILD="${SMARTTYPE_BUILD_DIR:-$ROOT/build}"
PREFIX="${SMARTTYPE_PREFIX:-$HOME/.local}"
BUILD_JOBS="${SMARTTYPE_BUILD_JOBS:-2}"
ENABLE_KDE_LAYOUT_SYNC=0
ENABLE_KDE_WAYLAND_IME=0
ENABLE_X11_LAYOUT_SYNC=0
DISABLE_KIMPANEL=0
CONFIGURE_PROFILE=1
PREBUILT_DIR=

usage() {
    cat <<'EOF'
Usage: scripts/install-user.sh [--enable-kde-layout-sync] [--enable-kde-wayland-ime]
                               [--enable-x11-layout-sync] [--disable-kimpanel]
                               [--no-configure-profile] [--prebuilt-dir DIR]

Builds and installs SmartType into $HOME/.local by default. It does not install
distribution packages or alter desktop settings unless an explicit option is
given. See docs/INSTALL.md for Fedora, Ubuntu and Kali dependencies.
By default it adds both SmartType methods to the current Fcitx group while
preserving all existing methods. Use --no-configure-profile to opt out.
EOF
}

while (($#)); do
    case "$1" in
        --enable-kde-layout-sync) ENABLE_KDE_LAYOUT_SYNC=1 ;;
        --enable-kde-wayland-ime) ENABLE_KDE_WAYLAND_IME=1 ;;
        --enable-x11-layout-sync) ENABLE_X11_LAYOUT_SYNC=1 ;;
        --disable-kimpanel) DISABLE_KIMPANEL=1 ;;
        --no-configure-profile) CONFIGURE_PROFILE=0 ;;
        --prebuilt-dir)
            [[ $# -ge 2 ]] || { echo "--prebuilt-dir requires a value" >&2; exit 2; }
            PREBUILT_DIR=$(cd "$2" && pwd)
            shift
            ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

if (( ENABLE_KDE_LAYOUT_SYNC && ENABLE_X11_LAYOUT_SYNC )); then
    echo "Choose only one layout owner: KDE Wayland or X11/Fcitx." >&2
    exit 2
fi

command -v fcitx5 >/dev/null || {
    echo "Fcitx 5 runtime is required. Install it with your distribution package manager; see docs/INSTALL.md." >&2
    exit 1
}

if [[ -n $PREBUILT_DIR ]]; then
    required=(
        bin/smarttypectl
        bin/smarttype-tray
        bin/configure-fcitx-profile.py
        bin/configure-fcitx-x11.py
        bin/fcitx5-layout-sync.py
        lib/fcitx5/smarttype.so
        lib/fcitx5/smarttypeui.so
        share/fcitx5/addon/smarttype.conf
        share/fcitx5/addon/smarttypeui.conf
        share/fcitx5/inputmethod/smarttype.conf
        share/fcitx5/inputmethod/smarttype-us.conf
        share/systemd/user/smarttype-tray.service
    )
    for relative_path in "${required[@]}"; do
        [[ -f "$PREBUILT_DIR/$relative_path" ]] || {
            echo "Invalid SmartType release bundle: missing $relative_path" >&2
            exit 1
        }
    done
    echo "==> Installing verified prebuilt SmartType files into $PREFIX"
    mkdir -p "$PREFIX"
    cp -a "$PREBUILT_DIR/." "$PREFIX/"
else
    cmake_args=(-S "$ROOT" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release
                -DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_INSTALL_LIBDIR=lib
                -DSMARTTYPE_BUILD_UI_DEMO=OFF)
    if [[ -f "$ROOT/.deps/sysroot/usr/include/Fcitx5/Core/fcitx/instance.h" ]]; then
        cmake_args+=("-DSMARTTYPE_FCITX_SDK_ROOT=$ROOT/.deps/sysroot")
    fi

    cmake "${cmake_args[@]}"
    cmake --build "$BUILD" --parallel "$BUILD_JOBS"
    ctest --test-dir "$BUILD" --output-on-failure
    echo "=== Хеши установленных файлов перед установкой ==="
    sha256sum "$PREFIX/bin/smarttypectl" "$PREFIX/lib/fcitx5/smarttype.so" 2>/dev/null || true
    cmake --install "$BUILD"
fi

# Retire every historical state-specific icon. The tray now embeds its two
# theme variants, while the desktop launcher uses only smarttype-idle.png.
rm -f \
    "$PREFIX/share/icons/hicolor/512x512/apps/smarttype-pause.png" \
    "$PREFIX/share/icons/hicolor/1024x1024/apps/smarttype-idle.png" \
    "$PREFIX/share/icons/hicolor/1024x1024/apps/smarttype-pause.png" \
    "$PREFIX/share/icons/hicolor/scalable/apps/smarttype.svg" \
    "$PREFIX/share/icons/hicolor/scalable/apps/smarttype-paused.svg" \
    "$PREFIX/share/icons/hicolor/scalable/apps/smarttype-disabled.svg"

if (( CONFIGURE_PROFILE )); then
    "$PREFIX/bin/configure-fcitx-profile.py" \
        "${XDG_CONFIG_HOME:-$HOME/.config}/fcitx5/profile"
    echo "OK  SmartType Русский и SmartType English добавлены в профиль Fcitx."
fi

echo "=== Хеши установленных файлов после установки ==="
sha256sum "$PREFIX/bin/smarttypectl" "$PREFIX/lib/fcitx5/smarttype.so" 2>/dev/null || true

killall -9 smarttype-ui 2>/dev/null || true

if [[ "${SMARTTYPE_EXTERNAL_UI:-0}" == "1" ]] &&
   [[ -x "$BUILD/src/ui/smarttype-ui" ]] && command -v kpackagetool6 >/dev/null; then
    kpackagetool6 --type=KWin/Script --upgrade "$ROOT/kwin/smarttype-position" \
        >/dev/null 2>&1 || \
        kpackagetool6 --type=KWin/Script --install "$ROOT/kwin/smarttype-position"
    kwriteconfig6 --file kwinrc --group Plugins \
        --key smarttype-positionEnabled true
    busctl --user call org.kde.KWin /KWin org.kde.KWin reconfigure \
        >/dev/null 2>&1 || true
elif command -v kwriteconfig6 >/dev/null; then
    kwriteconfig6 --file kwinrc --group Plugins \
        --key smarttype-positionEnabled false
fi

mkdir -p "$HOME/.config/environment.d"
addon_dirs=()
for dir in "$PREFIX/lib/fcitx5" "$PREFIX/lib64/fcitx5" /usr/lib/fcitx5 /usr/lib64/fcitx5 /usr/lib/*-linux-gnu/fcitx5; do
    [[ -d "$dir" ]] && addon_dirs+=("$dir")
done
addon_dirs_joined=$(IFS=:; echo "${addon_dirs[*]}")
cat > "$HOME/.config/environment.d/90-smarttype.conf" <<EOF
# Generated by SmartType. Keep the local addon directory ahead of the distro one.
FCITX_ADDON_DIRS=$addon_dirs_joined
XMODIFIERS=@im=fcitx
EOF

# KDE Wayland must be the sole launcher of Fcitx so KWin can pass the
# one-shot input-method socket. The normal XDG autostart and an early
# fcitx5-remote call would launch a second, visually alive but input-dead
# instance instead. This desktop integration is opt-in because it changes
# the user's KWin input-method selection.
if (( ENABLE_KDE_WAYLAND_IME )); then
    fcitx_desktop=/usr/share/applications/org.fcitx.Fcitx5.desktop
    if ! command -v kwriteconfig6 >/dev/null || [[ ! -f $fcitx_desktop ]]; then
        echo "FAIL  KDE Wayland Fcitx desktop integration is unavailable" >&2
        exit 1
    fi
    kwriteconfig6 --file kwinrc --group Wayland --key InputMethod "$fcitx_desktop"
    mkdir -p "${XDG_CONFIG_HOME:-$HOME/.config}/autostart"
    cat > "${XDG_CONFIG_HOME:-$HOME/.config}/autostart/org.fcitx.Fcitx5.desktop" <<'EOF'
[Desktop Entry]
Hidden=true
EOF
    echo "OK  KWin will own Fcitx startup on the next KDE Wayland login."
fi

# User install prefix (matches CMAKE_INSTALL_PREFIX above).
APPS_DESKTOP="$PREFIX/share/applications/smarttype-tray.desktop"
TRAY_BIN="$PREFIX/bin/smarttype-tray"

if [[ ! -f "$APPS_DESKTOP" ]]; then
    echo "Предупреждение: не найден $APPS_DESKTOP — autostart tray не настроен." >&2
else
    # Ensure desktop entry Exec points at the installed tray binary.
    if grep -q '^Exec=' "$APPS_DESKTOP"; then
        sed -i "s|^Exec=.*|Exec=$TRAY_BIN|" "$APPS_DESKTOP"
    else
        printf 'Exec=%s\n' "$TRAY_BIN" >> "$APPS_DESKTOP"
    fi
    sed -i "s|@HOME@|$HOME|g" "$APPS_DESKTOP"

fi

# A named user service owns the long-lived tray. Unlike a one-shot XDG
# autostart process it has an observable state and is restarted even after a
# clean/unexpected QApplication exit.
TRAY_UNIT_SRC="$PREFIX/share/systemd/user/smarttype-tray.service"
TRAY_UNIT_DST="${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user/smarttype-tray.service"
mkdir -p "$(dirname "$TRAY_UNIT_DST")"
sed "s|^ExecStart=.*|ExecStart=$TRAY_BIN|" "$TRAY_UNIT_SRC" > "$TRAY_UNIT_DST"

# Remove both historical XDG autostart locations so only systemd owns the
# process and a manual launcher invocation cannot replace its lifecycle.
rm -f "$PREFIX/share/autostart/smarttype-tray.desktop" \
      "${XDG_CONFIG_HOME:-$HOME/.config}/autostart/smarttype-tray.desktop"

systemctl --user daemon-reload
systemctl --user stop smarttype-tray.service 2>/dev/null || true
# Retire an old XDG-autostart/manual instance before starting the owned one.
pkill -x smarttype-tray 2>/dev/null || true
if systemctl --user reenable smarttype-tray.service >/dev/null &&
   systemctl --user start smarttype-tray.service; then
    echo "OK  smarttype-tray.service enabled and running."
else
    echo "FAIL  smarttype-tray.service could not be enabled" >&2
    exit 1
fi

# ── ST-020: KDE Alt+Shift ↔ SmartType Fcitx IM bridge (opt-in) ─────────────
# Without this service, Alt+Shift only flips the Plasma layout indicator while
# Fcitx stays on smarttype / smarttype-us — typing language freezes (Kate,
# Chrome, Telegram). Unit goes to ~/.config/systemd/user (always loaded).
LAYOUT_SYNC_BIN="$PREFIX/bin/fcitx5-layout-sync.py"
LAYOUT_SYNC_UNIT_SRC="$ROOT/config/fcitx5-layout-sync.service"
LAYOUT_SYNC_UNIT_DST="${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user/fcitx5-layout-sync.service"

if (( ENABLE_KDE_LAYOUT_SYNC )) && [[ ! -x "$LAYOUT_SYNC_BIN" && -f "$ROOT/scripts/fcitx5-layout-sync.py" ]]; then
    install -Dm755 "$ROOT/scripts/fcitx5-layout-sync.py" "$LAYOUT_SYNC_BIN"
fi

if (( ENABLE_KDE_LAYOUT_SYNC )) && [[ ! -x "$LAYOUT_SYNC_BIN" ]]; then
    echo "Предупреждение: нет $LAYOUT_SYNC_BIN — ручной Alt+Shift↔ST не будет работать." >&2
elif (( ENABLE_KDE_LAYOUT_SYNC )) && [[ ! -f "$LAYOUT_SYNC_UNIT_SRC" ]]; then
    echo "Предупреждение: нет $LAYOUT_SYNC_UNIT_SRC" >&2
elif (( ENABLE_KDE_LAYOUT_SYNC )); then
    mkdir -p "$(dirname "$LAYOUT_SYNC_UNIT_DST")"
    # Unit uses ExecStart=%h/.local/bin/... (portable). If PREFIX is not
    # $HOME/.local, rewrite ExecStart to the installed script path.
    if [[ "$LAYOUT_SYNC_BIN" == "$HOME/.local/bin/fcitx5-layout-sync.py" ]]; then
        cp "$LAYOUT_SYNC_UNIT_SRC" "$LAYOUT_SYNC_UNIT_DST"
    else
        sed "s|^ExecStart=.*|ExecStart=$LAYOUT_SYNC_BIN|" "$LAYOUT_SYNC_UNIT_SRC" \
            > "$LAYOUT_SYNC_UNIT_DST"
    fi
    systemctl --user daemon-reload
    if systemctl --user enable --now fcitx5-layout-sync.service; then
        echo "OK  fcitx5-layout-sync.service enabled and running (Alt+Shift ↔ SmartType)."
    else
        echo "WARN  не удалось enable --now fcitx5-layout-sync.service" >&2
        echo "      Запустите: systemctl --user enable --now fcitx5-layout-sync.service" >&2
    fi
fi

# ── Disable kimpanel only on explicit request ──────────────────────────────
# kimpanel (UIPriority=50) otherwise intercepts candidate rendering and shows
# the plain Plasma system popup instead of the native SmartType panel.
if (( DISABLE_KIMPANEL )); then
mkdir -p "$HOME/.config/fcitx5/conf"
if ! grep -q "Enabled=False" "$HOME/.config/fcitx5/conf/kimpanel.conf" 2>/dev/null; then
    printf '[Addon]\nEnabled=False\n' > "$HOME/.config/fcitx5/conf/kimpanel.conf"
    echo "OK  kimpanel disabled (smarttypeui will handle candidate rendering)."
fi
fi

# Refresh desktop database and KDE launcher cache
update-desktop-database "$PREFIX/share/applications" || true
command -v gtk-update-icon-cache >/dev/null && \
    gtk-update-icon-cache --force --ignore-theme-index "$PREFIX/share/icons/hicolor" || true
command -v kbuildsycoca6 >/dev/null && kbuildsycoca6 --noincremental || true

echo "SmartType установлен для пользователя $USER."
echo "Перезапустите графический сеанс, чтобы Fcitx получил новое окружение."
if (( CONFIGURE_PROFILE )); then
    echo "SmartType English и SmartType Русский уже добавлены в профиль Fcitx."
else
    echo "Добавьте SmartType English и SmartType Русский через fcitx5-configtool."
fi
if (( ! ENABLE_KDE_LAYOUT_SYNC && ! ENABLE_X11_LAYOUT_SYNC )); then
    echo "KDE Alt+Shift bridge was not enabled. On KDE, opt in with: scripts/install-user.sh --enable-kde-layout-sync"
fi
if (( ENABLE_X11_LAYOUT_SYNC )); then
    X11_CONFIGURATOR="$PREFIX/bin/configure-fcitx-x11.py"
    if [[ ! -x "$X11_CONFIGURATOR" ]]; then
        echo "FAIL  missing installed X11 configurator: $X11_CONFIGURATOR" >&2
        exit 1
    fi
    "$X11_CONFIGURATOR" \
        "${XDG_CONFIG_HOME:-$HOME/.config}/fcitx5/config" \
        "$HOME/.config/environment.d/90-smarttype.conf" \
        "$HOME/.xprofile"
    "$PREFIX/bin/smarttypectl" set-setting x11_normalize_layout true
    LAYOUT_SYNC_BIN="$PREFIX/bin/fcitx5-layout-sync.py"
    LAYOUT_SYNC_UNIT_SRC="$ROOT/config/fcitx5-layout-sync.service"
    LAYOUT_SYNC_UNIT_DST="${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user/fcitx5-layout-sync.service"
    mkdir -p "$(dirname "$LAYOUT_SYNC_UNIT_DST")"
    if [[ "$LAYOUT_SYNC_BIN" == "$HOME/.local/bin/fcitx5-layout-sync.py" ]]; then
        cp "$LAYOUT_SYNC_UNIT_SRC" "$LAYOUT_SYNC_UNIT_DST"
    else
        sed "s|^ExecStart=.*|ExecStart=$LAYOUT_SYNC_BIN|" "$LAYOUT_SYNC_UNIT_SRC" \
            > "$LAYOUT_SYNC_UNIT_DST"
    fi
    systemctl --user daemon-reload
    systemctl --user enable --now fcitx5-layout-sync.service
    if fcitx_has_owner=$(busctl --user call org.freedesktop.DBus \
        /org/freedesktop/DBus org.freedesktop.DBus NameHasOwner \
        s org.fcitx.Fcitx5 2>/dev/null) && [[ $fcitx_has_owner == 'b true' ]]; then
        fcitx5-remote -r || true
    fi
    echo "OK  X11 Fcitx owns Alt+Shift; GTK/Qt preedit stays in application fields."
fi
if (( CONFIGURE_PROFILE )) && command -v fcitx5-remote >/dev/null; then
    fcitx5-remote -r 2>/dev/null || true
    fcitx5-remote -o 2>/dev/null || true
    fcitx5-remote -s smarttype-us 2>/dev/null || true
fi
if (( ENABLE_KDE_WAYLAND_IME )); then
    echo "Log out and back in before testing; do not restart Fcitx from its tray on Wayland."
fi
