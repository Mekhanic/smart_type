#!/usr/bin/env bash
set -uo pipefail

PREFIX="${PREFIX:-$HOME/.local}"

echo "Останавливаю службы SmartType..."
systemctl --user stop fcitx5-layout-sync.service 2>/dev/null || true
systemctl --user disable fcitx5-layout-sync.service 2>/dev/null || true

# Kill running processes
systemctl --user disable --now smarttype-tray.service 2>/dev/null || true
killall -9 smarttype-tray smarttype-ui fcitx5-layout-sync.py 2>/dev/null || true

echo "Удаляю установленные файлы из ~/.local..."
rm -f "$HOME/.local/bin/smarttypectl" \
      "$HOME/.local/bin/smarttype-eval" \
      "$HOME/.local/bin/smarttype-tray" \
      "$HOME/.local/bin/smarttype-ui" \
      "$HOME/.local/bin/fcitx5-layout-sync.py" \
      "$HOME/.local/bin/configure-fcitx-profile.py" \
      "$HOME/.local/bin/configure-fcitx-gnome.py" \
      "$HOME/.local/bin/configure-fcitx-x11.py"

rm -f "$HOME/.local/lib64/fcitx5/smarttype.so" \
      "$HOME/.local/lib64/fcitx5/smarttypeui.so" \
      "$HOME/.local/lib/fcitx5/smarttype.so" \
      "$HOME/.local/lib/fcitx5/smarttypeui.so"

rm -f "$HOME/.local/share/fcitx5/addon/smarttype.conf" \
      "$HOME/.local/share/fcitx5/addon/smarttypeui.conf" \
      "$HOME/.local/share/fcitx5/inputmethod/smarttype.conf" \
      "$HOME/.local/share/fcitx5/inputmethod/smarttype-us.conf"

rm -rf "$HOME/.local/share/fcitx5/themes/smarttype-liquid-glass"

GNOME_EXTENSION_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/gnome-shell/extensions/kimpanel@kde.org"
GNOME_EXTENSION_BACKUP="${GNOME_EXTENSION_DIR}.before-smarttype"
if [[ -f $GNOME_EXTENSION_DIR/.smarttype-managed ]]; then
    GNOME_EXTENSION_STATE="$GNOME_EXTENSION_DIR/.smarttype-managed"
    kimpanel_was_enabled=0
    appindicator_was_enabled=0
    appindicator_uuid=$(sed -n 's/^appindicator_uuid=//p' "$GNOME_EXTENSION_STATE" | head -n1)
    grep -qx 'kimpanel_was_enabled=1' "$GNOME_EXTENSION_STATE" && kimpanel_was_enabled=1
    grep -qx 'appindicator_was_enabled=1' "$GNOME_EXTENSION_STATE" && appindicator_was_enabled=1
    if command -v gnome-extensions >/dev/null; then
        (( kimpanel_was_enabled )) || \
            gnome-extensions disable kimpanel@kde.org >/dev/null 2>&1 || true
        if [[ -n $appindicator_uuid ]] && (( ! appindicator_was_enabled )); then
            gnome-extensions disable "$appindicator_uuid" >/dev/null 2>&1 || true
        fi
    fi
    rm -rf "$GNOME_EXTENSION_DIR"
    if [[ -e $GNOME_EXTENSION_BACKUP ]]; then
        mv "$GNOME_EXTENSION_BACKUP" "$GNOME_EXTENSION_DIR"
    fi
fi
rm -rf "$PREFIX/share/smarttype/gnome"

for addon in kimpanel ibusfrontend smarttypeui; do
    addon_config="${XDG_CONFIG_HOME:-$HOME/.config}/fcitx5/conf/$addon.conf"
    addon_backup="$addon_config.before-smarttype-gnome"
    if [[ -f $addon_backup ]]; then
        mv "$addon_backup" "$addon_config"
    elif [[ -f $addon_config ]] && grep -q '^Enabled=' "$addon_config"; then
        rm -f "$addon_config"
    fi
done

AUTOSTART_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/autostart"

GNOME_FCITX_AUTOSTART="$AUTOSTART_DIR/org.fcitx.Fcitx5.desktop"
GNOME_FCITX_BACKUP="$GNOME_FCITX_AUTOSTART.before-smarttype-gnome"
if [[ -f $GNOME_FCITX_AUTOSTART ]] && grep -q '^X-SmartType-Managed=true$' "$GNOME_FCITX_AUTOSTART"; then
    rm -f "$GNOME_FCITX_AUTOSTART"
    [[ -f $GNOME_FCITX_BACKUP ]] && mv "$GNOME_FCITX_BACKUP" "$GNOME_FCITX_AUTOSTART"
fi

rm -f "$PREFIX/share/applications/smarttype-tray.desktop" \
      "$PREFIX/share/autostart/smarttype-tray.desktop" \
      "$AUTOSTART_DIR/smarttype-tray.desktop"

rm -f "${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user/smarttype-tray.service" \
      "$PREFIX/share/systemd/user/smarttype-tray.service"

rm -f "$HOME/.local/share/icons/hicolor/scalable/apps/smarttype.svg" \
      "$HOME/.local/share/icons/hicolor/scalable/apps/smarttype-disabled.svg" \
      "$HOME/.local/share/icons/hicolor/scalable/apps/smarttype-paused.svg" \
      "$HOME/.local/share/icons/hicolor/1024x1024/apps/smarttype-idle.png" \
      "$HOME/.local/share/icons/hicolor/1024x1024/apps/smarttype-pause.png" \
      "$PREFIX/share/icons/hicolor/512x512/apps/smarttype-idle.png" \
      "$PREFIX/share/icons/hicolor/512x512/apps/smarttype-pause.png"

rm -f "$HOME/.local/share/systemd/user/fcitx5-layout-sync.service"
rm -f "$HOME/.config/environment.d/90-smarttype.conf"
rm -f "$PREFIX/share/smarttype/doctor.sh" "$PREFIX/share/smarttype/uninstall-user.sh"
systemctl --user daemon-reload 2>/dev/null || true

if command -v kpackagetool6 >/dev/null; then
    echo "Удаляю KWin скрипты..."
    kpackagetool6 --type=KWin/Script --remove smarttype-position >/dev/null 2>&1 || true
    if command -v kwriteconfig6 >/dev/null; then
        kwriteconfig6 --file kwinrc --group Plugins --key smarttype-positionEnabled false
        busctl --user call org.kde.KWin /KWin org.kde.KWin reconfigure >/dev/null 2>&1 || true
    fi
fi

systemctl --user daemon-reload

# Refresh desktop database and KDE launcher cache
update-desktop-database "$HOME/.local/share/applications" 2>/dev/null || true
kbuildsycoca6 --noincremental >/dev/null 2>&1 || true

echo "Пользовательская копия SmartType удалена."
echo "Личная база данных в ~/.local/share/smarttype/ сохранена."
