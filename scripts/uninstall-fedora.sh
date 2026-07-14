#!/usr/bin/env bash
set -euo pipefail

echo "Останавливаю службы..."
systemctl --user stop fcitx5-layout-sync.service 2>/dev/null || true
systemctl --user disable fcitx5-layout-sync.service 2>/dev/null || true
systemctl --user disable --now smarttype-tray.service 2>/dev/null || true
killall -9 smarttype-tray smarttype-ui fcitx5-layout-sync.py 2>/dev/null || true

echo "Удаляю глобально установленные файлы SmartType..."
sudo rm -f /usr/lib64/fcitx5/smarttype.so \
            /usr/lib64/fcitx5/smarttypeui.so \
            /usr/lib/fcitx5/smarttype.so \
            /usr/lib/fcitx5/smarttypeui.so \
            /usr/share/fcitx5/addon/smarttype.conf \
            /usr/share/fcitx5/addon/smarttypeui.conf \
            /usr/share/fcitx5/inputmethod/smarttype.conf \
            /usr/share/fcitx5/inputmethod/smarttype-us.conf \
            /usr/bin/smarttypectl \
            /usr/bin/smarttype-eval \
            /usr/bin/smarttype-tray \
            /usr/bin/smarttype-ui \
            /usr/bin/fcitx5-layout-sync.py \
            /usr/share/applications/smarttype-tray.desktop \
            /usr/share/autostart/smarttype-tray.desktop \
            /usr/share/systemd/user/fcitx5-layout-sync.service \
            /usr/share/systemd/user/smarttype-tray.service

sudo rm -rf /usr/share/fcitx5/themes/smarttype-liquid-glass

sudo rm -f /usr/share/icons/hicolor/scalable/apps/smarttype.svg \
            /usr/share/icons/hicolor/scalable/apps/smarttype-disabled.svg \
            /usr/share/icons/hicolor/scalable/apps/smarttype-paused.svg \
            /usr/share/icons/hicolor/1024x1024/apps/smarttype-idle.png \
            /usr/share/icons/hicolor/1024x1024/apps/smarttype-pause.png \
            /usr/share/icons/hicolor/512x512/apps/smarttype-idle.png \
            /usr/share/icons/hicolor/512x512/apps/smarttype-pause.png

# Refresh desktop database and KDE launcher cache
sudo update-desktop-database /usr/share/applications 2>/dev/null || true
sudo gtk-update-icon-cache --force --ignore-theme-index /usr/share/icons/hicolor 2>/dev/null || true
systemctl --user daemon-reload

echo "SmartType удалён. Личная база ~/.local/share/smarttype сохранена."
