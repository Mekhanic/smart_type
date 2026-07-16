#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BUILD="${SMARTTYPE_BUILD_DIR:-$ROOT/build}"
PREFIX="${SMARTTYPE_PREFIX:-$HOME/.local}"
BUILD_JOBS="${SMARTTYPE_BUILD_JOBS:-2}"
ENABLE_KDE_LAYOUT_SYNC=0
ENABLE_KDE_WAYLAND_IME=0
ENABLE_X11_LAYOUT_SYNC=0
ENABLE_GNOME_WAYLAND=0
DISABLE_KIMPANEL=0
APPINDICATOR_UUID=
CONFIGURE_PROFILE=1
PREBUILT_DIR=

usage() {
    cat <<'EOF'
Usage: scripts/install-user.sh [--enable-kde-layout-sync] [--enable-kde-wayland-ime]
                               [--enable-gnome-wayland]
                               [--enable-x11-layout-sync] [--disable-kimpanel]
                               [--appindicator-uuid UUID]
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
        --enable-gnome-wayland) ENABLE_GNOME_WAYLAND=1 ;;
        --enable-x11-layout-sync) ENABLE_X11_LAYOUT_SYNC=1 ;;
        --disable-kimpanel) DISABLE_KIMPANEL=1 ;;
        --appindicator-uuid)
            [[ $# -ge 2 ]] || { echo "--appindicator-uuid requires a value" >&2; exit 2; }
            APPINDICATOR_UUID=$2
            shift
            ;;
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

integration_modes=$((ENABLE_KDE_WAYLAND_IME + ENABLE_X11_LAYOUT_SYNC + ENABLE_GNOME_WAYLAND))
if (( integration_modes > 1 )); then
    echo "Choose only one desktop integration: KDE Wayland, GNOME Wayland, or X11/Fcitx." >&2
    exit 2
fi

command -v fcitx5 >/dev/null || {
    echo "Fcitx 5 runtime is required. Install it with your distribution package manager; see docs/INSTALL.md." >&2
    exit 1
}

# Upgrades replace the tray executable in place. Linux rejects truncating a
# currently executing binary with ETXTBSY, so stop the service before copying
# a prebuilt payload. It is enabled and started again after the new unit is
# installed below.
systemctl --user stop smarttype-tray.service 2>/dev/null || true
pkill -x smarttype-tray 2>/dev/null || true

# Fedora preview builds used lib64/fcitx5 and sometimes left lib/fcitx5 as a
# symlink to that directory. Releases use a real lib/fcitx5 directory on every
# distribution. Detach only the symlink here; its target may contain unrelated
# user addons and must not be removed wholesale.
canonical_addon_dir="$PREFIX/lib/fcitx5"
legacy_addon_dir="$PREFIX/lib64/fcitx5"
if [[ -L $canonical_addon_dir ]]; then
    rm -f "$canonical_addon_dir"
    mkdir -p "$canonical_addon_dir"
fi

if [[ -n $PREBUILT_DIR ]]; then
    required=(
        bin/smarttypectl
        bin/smarttype-tray
        bin/configure-fcitx-profile.py
        bin/configure-fcitx-gnome.py
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
    # Replace existing executables and shared objects by inode instead of
    # truncating files that may still be mapped by a running tray/Fcitx
    # process. The old process keeps its old inode until the final reload.
    cp -a --remove-destination "$PREBUILT_DIR/." "$PREFIX/"
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

# Releases now use one canonical user addon directory. Old Fedora previews
# installed the same modules under lib64; leaving those files behind lets an
# already-running session keep loading a stale engine after an upgrade.
if [[ -f "$PREFIX/lib/fcitx5/smarttype.so" ]]; then
    rm -f "$legacy_addon_dir/smarttype.so" "$legacy_addon_dir/smarttypeui.so"
    rmdir "$legacy_addon_dir" 2>/dev/null || true
    rmdir "$PREFIX/lib64" 2>/dev/null || true
fi

KIMPANEL_SOURCE="$PREFIX/share/smarttype/gnome/kimpanel@kde.org"
if (( ENABLE_GNOME_WAYLAND )) && [[ ! -f $KIMPANEL_SOURCE/metadata.json ]]; then
    PREPARE_KIMPANEL="$ROOT/scripts/prepare-gnome-kimpanel.sh"
    [[ -x $PREPARE_KIMPANEL ]] || {
        echo "FAIL  missing GNOME Kimpanel preparation helper: $PREPARE_KIMPANEL" >&2
        exit 1
    }
    "$PREPARE_KIMPANEL" "$KIMPANEL_SOURCE"
fi
if (( ENABLE_GNOME_WAYLAND )); then
    KIMPANEL_PATCHER="$ROOT/scripts/patch-kimpanel.py"
    [[ -x $KIMPANEL_PATCHER ]] || {
        echo "FAIL  missing GNOME Kimpanel interaction patcher: $KIMPANEL_PATCHER" >&2
        exit 1
    }
    # Upgrade payloads made before the interaction fix may already contain a
    # pinned Kimpanel tree. Patch it idempotently before copying it into the
    # live GNOME extension directory; existence alone is not freshness.
    python3 "$KIMPANEL_PATCHER" "$KIMPANEL_SOURCE"
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
        "${XDG_CONFIG_HOME:-$HOME/.config}/fcitx5/profile" --apply-live
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
# Preview builds installed this later-sorting drop-in. Even a correct
# 90-smarttype.conf is ignored when the legacy file still assigns lib64 after
# it, leaving both SmartType input methods visible but unavailable. The file is
# SmartType-owned, so retire it during every upgrade.
rm -f "$HOME/.config/environment.d/fcitx5-smarttype.conf"
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

# Fedora's desktop autostarts imsettings after environment.d has been loaded.
# Its stock launcher exports XMODIFIERS=@im=none, which leaves the Fcitx tray
# visible but makes SmartType unusable after a clean login. Shadow only an
# installed/existing imsettings launcher, preserve a user override, and mark
# our replacement so uninstall can restore it exactly.
imsettings_source=${SMARTTYPE_IMSETTINGS_AUTOSTART_SOURCE:-/etc/xdg/autostart/imsettings-start.desktop}
imsettings_override="${XDG_CONFIG_HOME:-$HOME/.config}/autostart/imsettings-start.desktop"
imsettings_backup="$imsettings_override.before-smarttype"
if (( integration_modes == 1 )) &&
   [[ -e $imsettings_source || -e $imsettings_override ]]; then
    mkdir -p "$(dirname "$imsettings_override")"
    if [[ -e $imsettings_override ]] &&
       ! grep -qx 'X-SmartType-Managed=true' "$imsettings_override" 2>/dev/null &&
       [[ ! -e $imsettings_backup ]]; then
        cp -a "$imsettings_override" "$imsettings_backup"
    fi
    cat > "$imsettings_override" <<'EOF'
[Desktop Entry]
Type=Application
Name=Input Method Settings (disabled by SmartType)
Hidden=true
X-GNOME-Autostart-enabled=false
X-SmartType-Managed=true
EOF
    echo "OK  imsettings autostart disabled so it cannot replace Fcitx after login."
fi

# Fedora Plasma also sources the selected imsettings profile from its early
# plasma-workspace environment hook, before XDG autostart is considered. The
# modern per-user path is ~/.config/imsettings/xinputrc (not ~/.xinputrc).
# Selecting the distro's Fcitx 5 profile gives the whole login session
# XMODIFIERS=@im=fcitx5 while its Wayland guard leaves QT_IM_MODULE unset.
imsettings_fcitx_profile=${SMARTTYPE_IMSETTINGS_FCITX_PROFILE:-/etc/X11/xinit/xinput.d/fcitx5.conf}
imsettings_user_dir="${XDG_CONFIG_HOME:-$HOME/.config}/imsettings"
imsettings_xinputrc="$imsettings_user_dir/xinputrc"
imsettings_xinputrc_backup="$imsettings_user_dir/xinputrc.before-smarttype"
imsettings_xinputrc_marker="$imsettings_user_dir/xinputrc.smarttype-managed"
if (( integration_modes == 1 )) && [[ -r $imsettings_fcitx_profile ]]; then
    mkdir -p "$imsettings_user_dir"
    if [[ -e $imsettings_xinputrc || -L $imsettings_xinputrc ]]; then
        current_xinputrc=$(readlink -f "$imsettings_xinputrc" 2>/dev/null || true)
        expected_xinputrc=$(readlink -f "$imsettings_fcitx_profile" 2>/dev/null || true)
        if [[ $current_xinputrc != "$expected_xinputrc" &&
              ! -e $imsettings_xinputrc_backup ]]; then
            mv "$imsettings_xinputrc" "$imsettings_xinputrc_backup"
        fi
    fi
    ln -sfn "$imsettings_fcitx_profile" "$imsettings_xinputrc"
    printf '%s\n' "$imsettings_fcitx_profile" > "$imsettings_xinputrc_marker"
    echo "OK  Fedora imsettings profile set to Fcitx 5 for the next login."
fi

if (( ENABLE_GNOME_WAYLAND )); then
    GNOME_CONFIGURATOR="$PREFIX/bin/configure-fcitx-gnome.py"
    [[ -x $GNOME_CONFIGURATOR ]] || {
        echo "FAIL  missing installed GNOME configurator: $GNOME_CONFIGURATOR" >&2
        exit 1
    }
    [[ -f $KIMPANEL_SOURCE/metadata.json ]] || {
        echo "FAIL  release payload does not contain the pinned GNOME Kimpanel extension" >&2
        exit 1
    }

    GNOME_EXTENSION_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/gnome-shell/extensions/kimpanel@kde.org"
    GNOME_EXTENSION_BACKUP="${GNOME_EXTENSION_DIR}.before-smarttype"
    GNOME_EXTENSION_STATE="$GNOME_EXTENSION_DIR/.smarttype-managed"
    kimpanel_was_enabled=0
    appindicator_was_enabled=0
    if [[ -f $GNOME_EXTENSION_STATE ]]; then
        grep -qx 'kimpanel_was_enabled=1' "$GNOME_EXTENSION_STATE" && kimpanel_was_enabled=1
        grep -qx 'appindicator_was_enabled=1' "$GNOME_EXTENSION_STATE" && appindicator_was_enabled=1
    else
        enabled_extensions=$(gsettings get org.gnome.shell enabled-extensions 2>/dev/null || true)
        [[ $enabled_extensions == *kimpanel@kde.org* ]] && kimpanel_was_enabled=1
        [[ -n $APPINDICATOR_UUID && $enabled_extensions == *"$APPINDICATOR_UUID"* ]] && appindicator_was_enabled=1
    fi
    if [[ -e $GNOME_EXTENSION_DIR && ! -f $GNOME_EXTENSION_DIR/.smarttype-managed && ! -e $GNOME_EXTENSION_BACKUP ]]; then
        mv "$GNOME_EXTENSION_DIR" "$GNOME_EXTENSION_BACKUP"
    fi
    rm -rf "$GNOME_EXTENSION_DIR"
    mkdir -p "$(dirname "$GNOME_EXTENSION_DIR")"
    cp -a "$KIMPANEL_SOURCE" "$GNOME_EXTENSION_DIR"
    printf 'kimpanel_was_enabled=%s\nappindicator_uuid=%s\nappindicator_was_enabled=%s\n' \
        "$kimpanel_was_enabled" "$APPINDICATOR_UUID" "$appindicator_was_enabled" \
        > "$GNOME_EXTENSION_STATE"

    systemctl --user disable --now fcitx5-layout-sync.service >/dev/null 2>&1 || true
    # GNOME keeps one compositor XKB source while Fcitx cycles the two
    # SmartType methods. Normalize incoming Latin/Cyrillic keysyms to the
    # selected method so RU/EN changes immediately without a KDE layout bridge.
    "$PREFIX/bin/smarttypectl" set-setting x11_normalize_layout true
    "$GNOME_CONFIGURATOR" \
        "${XDG_CONFIG_HOME:-$HOME/.config}/fcitx5/config" \
        "${XDG_CONFIG_HOME:-$HOME/.config}/fcitx5/conf" \
        "$HOME/.config/environment.d/90-smarttype.conf" \
        "${XDG_CONFIG_HOME:-$HOME/.config}/autostart/org.fcitx.Fcitx5.desktop" \
        --enable-session-extensions --appindicator-uuid "$APPINDICATOR_UUID"
    echo "OK  GNOME will start Fcitx through its IBus frontend after the next login."
    echo "OK  Kimpanel and AppIndicator extensions are enabled without removing other extensions."
fi

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
    # Never rewrite the user's KDE layout list. On a one-layout or custom-order
    # profile the bridge yields ownership to Fcitx, and the engine maps physical
    # keysyms to the selected SmartType IM instead of trusting KDE index 0.
    # Configure Fcitx itself to own Alt+Shift and skip the technical
    # keyboard-us item. This is required when KDE has no RU layout index and
    # therefore cannot emit a useful layoutChanged signal.
    KDE_FCITX_CONFIGURATOR="$PREFIX/bin/configure-fcitx-x11.py"
    if [[ ! -x "$KDE_FCITX_CONFIGURATOR" ]]; then
        echo "FAIL  missing installed Fcitx configurator: $KDE_FCITX_CONFIGURATOR" >&2
        exit 1
    fi
    "$KDE_FCITX_CONFIGURATOR" \
        "${XDG_CONFIG_HOME:-$HOME/.config}/fcitx5/config"
    "$PREFIX/bin/smarttypectl" set-setting x11_normalize_layout true
    systemctl --user stop fcitx5-layout-sync.service 2>/dev/null || true
    rm -f "$PREFIX/bin/configure-kde-layouts.py" \
          "${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user/smarttype-kde-layouts.service" \
          "${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user/plasma-kwin_wayland.service.d/20-smarttype-layouts.conf" \
          "${XDG_CONFIG_HOME:-$HOME/.config}/plasma-workspace/env/smarttype-kde-layouts.sh"
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

echo "SmartType установлен для пользователя ${USER:-$(id -un)}."
echo "Перезапустите графический сеанс, чтобы Fcitx получил новое окружение."
if (( CONFIGURE_PROFILE )); then
    echo "SmartType English и SmartType Русский уже добавлены в профиль Fcitx."
else
    echo "Добавьте SmartType English и SmartType Русский через fcitx5-configtool."
fi
if (( ! ENABLE_KDE_LAYOUT_SYNC && ! ENABLE_X11_LAYOUT_SYNC && ! ENABLE_GNOME_WAYLAND )); then
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
if [[ $("$PREFIX/bin/smarttypectl" get-setting enabled 2>/dev/null) == "enabled: 1" ]]; then
    echo "OK  SmartType is enabled."
else
    echo "WARN  SmartType remains disabled by the existing user preference." >&2
    echo "      Enable it from the tray or run:" >&2
    echo "      $PREFIX/bin/smarttypectl set-setting enabled 1" >&2
fi
if (( ENABLE_KDE_WAYLAND_IME )); then
    echo "Log out and back in before testing; do not restart Fcitx from its tray on Wayland."
fi
if (( ENABLE_GNOME_WAYLAND )); then
    echo "Log out and back in before testing GNOME applications and the candidate panel."
fi
