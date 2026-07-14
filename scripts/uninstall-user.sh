#!/usr/bin/env bash
set -uo pipefail

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
      "$HOME/.local/bin/fcitx5-layout-sync.py"

rm -f "$HOME/.local/lib64/fcitx5/smarttype.so" \
      "$HOME/.local/lib64/fcitx5/smarttypeui.so" \
      "$HOME/.local/lib/fcitx5/smarttype.so" \
      "$HOME/.local/lib/fcitx5/smarttypeui.so"

rm -f "$HOME/.local/share/fcitx5/addon/smarttype.conf" \
      "$HOME/.local/share/fcitx5/addon/smarttypeui.conf" \
      "$HOME/.local/share/fcitx5/inputmethod/smarttype.conf" \
      "$HOME/.local/share/fcitx5/inputmethod/smarttype-us.conf"

rm -rf "$HOME/.local/share/fcitx5/themes/smarttype-liquid-glass"

PREFIX="${PREFIX:-$HOME/.local}"
AUTOSTART_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/autostart"

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
