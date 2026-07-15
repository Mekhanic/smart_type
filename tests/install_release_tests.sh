#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
TEST_ROOT=$(mktemp -d)
trap 'rm -rf "$TEST_ROOT"' EXIT

VERSION=v0.2.0
TARGET=fedora44
ARCHIVE="smarttype-${VERSION}-${TARGET}-x86_64.tar.gz"
BUNDLE="${ARCHIVE%.tar.gz}"
RELEASE_DIR="$TEST_ROOT/release/$VERSION"
mkdir -p "$TEST_ROOT/$BUNDLE/payload" "$RELEASE_DIR"

cat > "$TEST_ROOT/$BUNDLE/install.sh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
printf '%s\n' "$@" > "$SMARTTYPE_TEST_ARGS"
EOF
chmod +x "$TEST_ROOT/$BUNDLE/install.sh"
tar -czf "$RELEASE_DIR/$ARCHIVE" -C "$TEST_ROOT" "$BUNDLE"
(cd "$RELEASE_DIR" && sha256sum "$ARCHIVE" > "$ARCHIVE.sha256")
cat > "$TEST_ROOT/latest.json" <<EOF
{
  "tag_name": "$VERSION",
  "name": "SmartType $VERSION"
}
EOF

ARGS_FILE="$TEST_ROOT/installer-args"
SMARTTYPE_RELEASE_API_URL="file://$TEST_ROOT/latest.json" \
SMARTTYPE_RELEASE_BASE_URL="file://$RELEASE_DIR" \
SMARTTYPE_RELEASE_TARGET="$TARGET" \
SMARTTYPE_TEST_ARGS="$ARGS_FILE" \
    "$ROOT/scripts/install-release.sh" --mode gnome-wayland --skip-deps

grep -Fxq -- '--prebuilt-dir' "$ARGS_FILE"
grep -Fxq -- '--mode' "$ARGS_FILE"
grep -Fxq -- 'gnome-wayland' "$ARGS_FILE"
grep -Fxq -- '--skip-deps' "$ARGS_FILE"
grep -Fxq -- '--yes' "$ARGS_FILE"

printf 'corruption' >> "$RELEASE_DIR/$ARCHIVE"
if SMARTTYPE_VERSION="$VERSION" \
   SMARTTYPE_RELEASE_BASE_URL="file://$RELEASE_DIR" \
   SMARTTYPE_RELEASE_TARGET="$TARGET" \
   SMARTTYPE_TEST_ARGS="$ARGS_FILE" \
       "$ROOT/scripts/install-release.sh" --skip-deps >/dev/null 2>&1; then
    echo "Corrupted release archive passed checksum verification" >&2
    exit 1
fi

echo "Release bootstrap end-to-end test passed"
