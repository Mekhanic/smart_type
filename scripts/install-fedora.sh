#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
[[ -f /etc/fedora-release ]] || { echo "Поддерживается только Fedora." >&2; exit 1; }

echo "Устанавливаю зависимости (sudo запросит пароль)..."
sudo dnf install -y fcitx5 fcitx5-devel fcitx5-qt fcitx5-gtk \
    fcitx5-configtool hunspell hunspell-ru gcc-c++ cmake ninja-build sqlite-devel \
    pkgconf-pkg-config cairo-devel pango-devel gdk-pixbuf2-devel wayland-devel wayland-protocols-devel \
    qt6-qtbase-devel qt6-qtdeclarative-devel

cmake -S "$ROOT" -B "$ROOT/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
    -DSMARTTYPE_BUILD_UI_DEMO=OFF
cmake --build "$ROOT/build"
ctest --test-dir "$ROOT/build" --output-on-failure
sudo cmake --install "$ROOT/build"

# The CMake-generated unit contains the real /usr binary path. Keep one
# systemd-owned tray instance and retire historical XDG/manual instances.
rm -f "$HOME/.config/autostart/smarttype-tray.desktop"
systemctl --user daemon-reload
systemctl --user stop smarttype-tray.service 2>/dev/null || true
pkill -x smarttype-tray 2>/dev/null || true
systemctl --user enable --now smarttype-tray.service

sudo gtk-update-icon-cache --force --ignore-theme-index /usr/share/icons/hicolor 2>/dev/null || true
update-desktop-database /usr/share/applications 2>/dev/null || true
command -v kbuildsycoca6 >/dev/null && kbuildsycoca6 --noincremental || true

mkdir -p "$HOME/.config/environment.d"
if [[ ! -e "$HOME/.config/environment.d/90-smarttype.conf" ]]; then
    install -m 0644 "$ROOT/config/90-smarttype.conf" \
        "$HOME/.config/environment.d/90-smarttype.conf"
fi

echo
echo "SmartType установлен. Завершите настройку KDE:"
echo "  Tray: systemctl --user status smarttype-tray.service"
echo "  1. Параметры системы -> Клавиатура -> Виртуальная клавиатура -> Fcitx 5."
echo "  2. Выйдите из сеанса и войдите снова."
echo "  3. В fcitx5-configtool добавьте «SmartType Русский»."
echo
echo "Личное слово:   smarttypectl add-word Happ"
echo "Личное правило: smarttypectl add-rule вопщем 'в общем'"
