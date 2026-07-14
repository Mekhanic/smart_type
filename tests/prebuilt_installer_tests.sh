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

HOME="$TEST_HOME" \
XDG_CONFIG_HOME="$TEST_HOME/.config" \
SMARTTYPE_PREFIX="$TEST_HOME/.local" \
PATH="$MOCK_BIN:$PATH" \
    "$ROOT/scripts/install-user.sh" --prebuilt-dir "$PAYLOAD"

test -x "$TEST_HOME/.local/bin/smarttypectl"
test -f "$TEST_HOME/.local/lib/fcitx5/smarttype.so"
test -f "$TEST_HOME/.local/share/icons/hicolor/512x512/apps/smarttype-idle.png"
grep -Fxq "ExecStart=$TEST_HOME/.local/bin/smarttype-tray" \
    "$TEST_HOME/.config/systemd/user/smarttype-tray.service"
grep -Fq 'Name=smarttype-us' "$TEST_HOME/.config/fcitx5/profile"
grep -Fq 'Name=smarttype' "$TEST_HOME/.config/fcitx5/profile"

broken_payload="$TEST_HOME/broken-payload"
cp -a "$PAYLOAD" "$broken_payload"
rm "$broken_payload/lib/fcitx5/smarttype.so"
if HOME="$TEST_HOME" SMARTTYPE_PREFIX="$TEST_HOME/broken-prefix" PATH="$MOCK_BIN:$PATH" \
    "$ROOT/scripts/install-user.sh" --prebuilt-dir "$broken_payload" >/dev/null 2>&1; then
    echo "Invalid prebuilt payload was accepted" >&2
    exit 1
fi

echo "Prebuilt installer smoke test passed"
