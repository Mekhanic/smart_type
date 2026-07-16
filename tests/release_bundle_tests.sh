#!/usr/bin/env bash
set -euo pipefail

BUNDLE=${1:-}
[[ -d $BUNDLE && -x $BUNDLE/install.sh && -d $BUNDLE/payload ]] || {
    echo "Usage: tests/release_bundle_tests.sh BUNDLE" >&2
    exit 2
}

for required in install-dependencies.sh patch-kimpanel.py install-user.sh \
    prepare-gnome-kimpanel.sh; do
    [[ -x $BUNDLE/scripts/$required ]] || {
        echo "Release bundle misses executable scripts/$required" >&2
        exit 1
    }
done
bash -n "$BUNDLE/install.sh" "$BUNDLE/scripts/"*.sh

target=$(<"$BUNDLE/TARGET")
case "$target" in
    fedora44|ubuntu2604) mode=kde-wayland ;;
    kali-rolling) mode=x11 ;;
    *)
        echo "Release bundle has an unsupported TARGET: $target" >&2
        exit 1
        ;;
esac

TEST_HOME=$(mktemp -d)
trap 'rm -rf "$TEST_HOME"' EXIT
MOCK_BIN="$TEST_HOME/mock-bin"
mkdir -p "$MOCK_BIN" "$TEST_HOME/.config/environment.d"
printf '%s\n' '[Layout]' 'LayoutList=us' 'VariantList=' 'Use=true' \
    > "$TEST_HOME/.config/kxkbrc"
cp "$TEST_HOME/.config/kxkbrc" "$TEST_HOME/kxkbrc.expected"
for command in fcitx5 fcitx5-remote systemctl pkill killall \
    update-desktop-database gtk-update-icon-cache kbuildsycoca6 kwriteconfig6 \
    busctl; do
    ln -s /usr/bin/true "$MOCK_BIN/$command"
done
printf '%s\n' \
    'FCITX_ADDON_DIRS=/home/preview/.local/lib64/fcitx5:/usr/lib64/fcitx5' \
    > "$TEST_HOME/.config/environment.d/fcitx5-smarttype.conf"

env -u USER \
HOME="$TEST_HOME" \
XDG_CONFIG_HOME="$TEST_HOME/.config" \
SMARTTYPE_PREFIX="$TEST_HOME/.local" \
    PATH="$MOCK_BIN:$PATH" \
    "$BUNDLE/install.sh" --prebuilt-dir "$BUNDLE/payload" \
    --mode "$mode" --skip-deps

test -x "$TEST_HOME/.local/bin/smarttypectl"
test -f "$TEST_HOME/.local/lib/fcitx5/smarttype.so"
test -f "$TEST_HOME/.local/lib/fcitx5/smarttypeui.so"
test ! -e "$TEST_HOME/.config/environment.d/fcitx5-smarttype.conf"
grep -Fq "FCITX_ADDON_DIRS=$TEST_HOME/.local/lib/fcitx5" \
    "$TEST_HOME/.config/environment.d/90-smarttype.conf"
systemctl_unit="$TEST_HOME/.config/systemd/user/fcitx5-layout-sync.service"
test -f "$systemctl_unit"
python3 -c 'import pathlib, sys; assert pathlib.Path(sys.argv[1]).read_bytes() == pathlib.Path(sys.argv[2]).read_bytes()' \
    "$TEST_HOME/kxkbrc.expected" "$TEST_HOME/.config/kxkbrc"
fcitx_config="$TEST_HOME/.config/fcitx5/config"
grep -qx 'ActiveByDefault=True' "$fcitx_config"
grep -qx 'ShareInputState=All' "$fcitx_config"
grep -qx 'EnumerateSkipFirst=True' "$fcitx_config"
grep -qx '0=Alt+Shift_L' "$fcitx_config"
grep -qx '1=Shift+Alt_L' "$fcitx_config"

echo "Extracted release bundle install smoke passed"
