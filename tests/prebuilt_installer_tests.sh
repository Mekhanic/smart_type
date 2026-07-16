#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
PAYLOAD=${1:-}
[[ -d $PAYLOAD ]] || { echo "Usage: tests/prebuilt_installer_tests.sh PAYLOAD" >&2; exit 2; }

TEST_HOME=$(mktemp -d)
trap 'rm -rf "$TEST_HOME"' EXIT
MOCK_BIN="$TEST_HOME/mock-bin"
mkdir -p "$MOCK_BIN"
for command in fcitx5 fcitx5-remote systemctl pkill killall update-desktop-database \
    gtk-update-icon-cache kbuildsycoca6 kwriteconfig6 busctl; do
    ln -s /usr/bin/true "$MOCK_BIN/$command"
done
printf '%s\n' '#!/bin/sh' \
    'if [ "$1" = get ]; then' \
    "  echo \"['existing@example.org']\"" \
    'fi' > "$MOCK_BIN/gsettings"
chmod +x "$MOCK_BIN/gsettings"

mkdir -p "$TEST_HOME/.local/lib64/fcitx5"
printf stale > "$TEST_HOME/.local/lib64/fcitx5/smarttype.so"
printf stale > "$TEST_HOME/.local/lib64/fcitx5/smarttypeui.so"
mkdir -p "$TEST_HOME/.config/environment.d"
printf '%s\n' \
    'FCITX_ADDON_DIRS=/home/preview/.local/lib64/fcitx5:/usr/lib64/fcitx5' \
    > "$TEST_HOME/.config/environment.d/fcitx5-smarttype.conf"

env -u USER \
HOME="$TEST_HOME" \
XDG_CONFIG_HOME="$TEST_HOME/.config" \
SMARTTYPE_PREFIX="$TEST_HOME/.local" \
PATH="$MOCK_BIN:$PATH" \
    "$ROOT/scripts/install-user.sh" --prebuilt-dir "$PAYLOAD"

test -x "$TEST_HOME/.local/bin/smarttypectl"
test -f "$TEST_HOME/.local/lib/fcitx5/smarttype.so"
test ! -e "$TEST_HOME/.local/lib64/fcitx5/smarttype.so"
test ! -e "$TEST_HOME/.local/lib64/fcitx5/smarttypeui.so"
test ! -e "$TEST_HOME/.config/environment.d/fcitx5-smarttype.conf"
grep -Fq "FCITX_ADDON_DIRS=$TEST_HOME/.local/lib/fcitx5" \
    "$TEST_HOME/.config/environment.d/90-smarttype.conf"

SYMLINK_HOME=$(mktemp -d)
mkdir -p "$SYMLINK_HOME/.local/lib64/fcitx5" "$SYMLINK_HOME/.local/lib"
printf stale > "$SYMLINK_HOME/.local/lib64/fcitx5/smarttype.so"
printf keep > "$SYMLINK_HOME/.local/lib64/fcitx5/other-addon.so"
ln -s ../lib64/fcitx5 "$SYMLINK_HOME/.local/lib/fcitx5"
HOME="$SYMLINK_HOME" \
XDG_CONFIG_HOME="$SYMLINK_HOME/.config" \
SMARTTYPE_PREFIX="$SYMLINK_HOME/.local" \
PATH="$MOCK_BIN:$PATH" \
    "$ROOT/scripts/install-user.sh" --prebuilt-dir "$PAYLOAD"
test -f "$SYMLINK_HOME/.local/lib/fcitx5/smarttype.so"
test ! -L "$SYMLINK_HOME/.local/lib/fcitx5"
test ! -e "$SYMLINK_HOME/.local/lib64/fcitx5/smarttype.so"
test -f "$SYMLINK_HOME/.local/lib64/fcitx5/other-addon.so"
rm -rf "$SYMLINK_HOME"
test -f "$TEST_HOME/.local/share/icons/hicolor/512x512/apps/smarttype-idle.png"
grep -Fxq "ExecStart=$TEST_HOME/.local/bin/smarttype-tray" \
    "$TEST_HOME/.config/systemd/user/smarttype-tray.service"
grep -Fq 'Name=smarttype-us' "$TEST_HOME/.config/fcitx5/profile"
grep -Fq 'Name=smarttype' "$TEST_HOME/.config/fcitx5/profile"

GNOME_PAYLOAD=$(mktemp -d)
cp -a "$PAYLOAD/." "$GNOME_PAYLOAD/"
GNOME_SOURCE="$GNOME_PAYLOAD/share/smarttype/gnome/kimpanel@kde.org"
if [[ ! -f $GNOME_SOURCE/metadata.json ]]; then
    mkdir -p "$GNOME_SOURCE"
    printf '%s\n' '{"uuid":"kimpanel@kde.org"}' > "$GNOME_SOURCE/metadata.json"
    cat > "$GNOME_SOURCE/panel.js" <<'EOF'
            if (label[i].length == 0)
                lookupTable[i].ignore_focus = true;
            else
                lookupTable[i].ignore_focus = false;
EOF
    printf '%s\n' '.kimpanel-candidate-item { cursor: pointer; }' \
        > "$GNOME_SOURCE/stylesheet.css"
fi
GNOME_HOME=$(mktemp -d)
    mkdir -p "$GNOME_HOME/.config/autostart"
    printf '%s\n' '[Desktop Entry]' 'Type=Application' 'Name=Existing override' \
        > "$GNOME_HOME/.config/autostart/imsettings-start.desktop"
    IMSETTINGS_SOURCE="$GNOME_HOME/system-imsettings-start.desktop"
    printf '%s\n' '[Desktop Entry]' 'Type=Application' 'Exec=imsettings-boot.sh' \
        > "$IMSETTINGS_SOURCE"
    IMSETTINGS_FCITX_PROFILE="$GNOME_HOME/system-fcitx5.conf"
    printf '%s\n' 'XIM=fcitx5' 'XMODIFIERS=@im=fcitx5' \
        > "$IMSETTINGS_FCITX_PROFILE"
    mkdir -p "$GNOME_HOME/.config/imsettings"
    printf '%s\n' 'XIM=custom' > "$GNOME_HOME/.config/imsettings/xinputrc"
    HOME="$GNOME_HOME" \
    XDG_CONFIG_HOME="$GNOME_HOME/.config" \
    SMARTTYPE_PREFIX="$GNOME_HOME/.local" \
    SMARTTYPE_IMSETTINGS_AUTOSTART_SOURCE="$IMSETTINGS_SOURCE" \
    SMARTTYPE_IMSETTINGS_FCITX_PROFILE="$IMSETTINGS_FCITX_PROFILE" \
    PATH="$MOCK_BIN:$PATH" \
        "$ROOT/scripts/install-user.sh" --prebuilt-dir "$GNOME_PAYLOAD" \
        --enable-gnome-wayland --appindicator-uuid test-appindicator@example.org

    grep -A5 -Fx '[Behavior/EnabledAddons]' "$GNOME_HOME/.config/fcitx5/config" |
        grep -Fxq '0=kimpanel'
    grep -A5 -Fx '[Behavior/EnabledAddons]' "$GNOME_HOME/.config/fcitx5/config" |
        grep -Fxq '1=ibusfrontend'
    grep -A5 -Fx '[Behavior/DisabledAddons]' "$GNOME_HOME/.config/fcitx5/config" |
        grep -Fxq '0=smarttypeui'
    grep -Fxq 'GTK_IM_MODULE=fcitx' "$GNOME_HOME/.config/environment.d/90-smarttype.conf"
    grep -Fxq 'QT_IM_MODULE=fcitx' "$GNOME_HOME/.config/environment.d/90-smarttype.conf"
    grep -Fxq 'QT_IM_MODULES=wayland;fcitx' "$GNOME_HOME/.config/environment.d/90-smarttype.conf"
    grep -Fxq 'Exec=fcitx5 -d --replace' \
        "$GNOME_HOME/.config/autostart/org.fcitx.Fcitx5.desktop"
    grep -Fxq 'Hidden=true' \
        "$GNOME_HOME/.config/autostart/imsettings-start.desktop"
    grep -Fxq 'X-SmartType-Managed=true' \
        "$GNOME_HOME/.config/autostart/imsettings-start.desktop"
    grep -Fxq 'Name=Existing override' \
        "$GNOME_HOME/.config/autostart/imsettings-start.desktop.before-smarttype"
    test "$(readlink -f "$GNOME_HOME/.config/imsettings/xinputrc")" = \
        "$(readlink -f "$IMSETTINGS_FCITX_PROFILE")"
    grep -Fxq 'XIM=custom' \
        "$GNOME_HOME/.config/imsettings/xinputrc.before-smarttype"
    grep -Fxq "$IMSETTINGS_FCITX_PROFILE" \
        "$GNOME_HOME/.config/imsettings/xinputrc.smarttype-managed"
    test -f "$GNOME_HOME/.local/share/gnome-shell/extensions/kimpanel@kde.org/.smarttype-managed"
    grep -Fxq 'kimpanel_was_enabled=0' \
        "$GNOME_HOME/.local/share/gnome-shell/extensions/kimpanel@kde.org/.smarttype-managed"
    grep -Fxq 'appindicator_uuid=test-appindicator@example.org' \
        "$GNOME_HOME/.local/share/gnome-shell/extensions/kimpanel@kde.org/.smarttype-managed"
    grep -Fxq 'appindicator_was_enabled=0' \
        "$GNOME_HOME/.local/share/gnome-shell/extensions/kimpanel@kde.org/.smarttype-managed"
    ! grep -q 'label\[i\].length == 0' \
        "$GNOME_HOME/.local/share/gnome-shell/extensions/kimpanel@kde.org/panel.js"
    grep -Fq 'rgba(128, 128, 128, 0.22)' \
        "$GNOME_HOME/.local/share/gnome-shell/extensions/kimpanel@kde.org/stylesheet.css"
    HOME="$GNOME_HOME" \
    XDG_CONFIG_HOME="$GNOME_HOME/.config" \
    PREFIX="$GNOME_HOME/.local" \
    PATH="$MOCK_BIN:$PATH" \
        "$ROOT/scripts/uninstall-user.sh" >/dev/null
    grep -Fxq 'Name=Existing override' \
        "$GNOME_HOME/.config/autostart/imsettings-start.desktop"
    test ! -e "$GNOME_HOME/.config/autostart/imsettings-start.desktop.before-smarttype"
    grep -Fxq 'XIM=custom' "$GNOME_HOME/.config/imsettings/xinputrc"
    test ! -e "$GNOME_HOME/.config/imsettings/xinputrc.before-smarttype"
    test ! -e "$GNOME_HOME/.config/imsettings/xinputrc.smarttype-managed"
rm -rf "$GNOME_HOME" "$GNOME_PAYLOAD"

broken_payload="$TEST_HOME/broken-payload"
cp -a "$PAYLOAD" "$broken_payload"
rm "$broken_payload/lib/fcitx5/smarttype.so"
if HOME="$TEST_HOME" SMARTTYPE_PREFIX="$TEST_HOME/broken-prefix" PATH="$MOCK_BIN:$PATH" \
    "$ROOT/scripts/install-user.sh" --prebuilt-dir "$broken_payload" >/dev/null 2>&1; then
    echo "Invalid prebuilt payload was accepted" >&2
    exit 1
fi

echo "Prebuilt installer smoke test passed"
