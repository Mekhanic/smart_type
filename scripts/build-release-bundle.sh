#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
VERSION=${1:-}
TARGET=${2:-}
OUT_DIR=${3:-$ROOT/dist}
ARCH=x86_64

if [[ -z $VERSION || -z $TARGET ]]; then
    echo "Usage: scripts/build-release-bundle.sh VERSION fedora44|ubuntu2604|kali-rolling [OUT_DIR]" >&2
    exit 2
fi
case "$VERSION" in v[0-9]*.[0-9]*.[0-9]*) ;; *) echo "Version must look like v1.2.3" >&2; exit 2 ;; esac
case "$TARGET" in fedora44|ubuntu2604|kali-rolling) ;; *) echo "Unknown target: $TARGET" >&2; exit 2 ;; esac
case "$(uname -m)" in x86_64|amd64) ;; *) echo "Release bundles currently support x86_64 only" >&2; exit 1 ;; esac

BUILD_DIR=${SMARTTYPE_RELEASE_BUILD_DIR:-$ROOT/build-release-$TARGET}
NAME="smarttype-${VERSION}-${TARGET}-${ARCH}"
STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT
BUNDLE="$STAGE/$NAME"

cmake_args=(-S "$ROOT" -B "$BUILD_DIR" -G Ninja
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_INSTALL_PREFIX=/usr
    -DCMAKE_INSTALL_LIBDIR=lib
    -DSMARTTYPE_BUILD_UI_DEMO=OFF)
if [[ -n ${SMARTTYPE_FCITX_SDK_ROOT:-} ]]; then
    cmake_args+=("-DSMARTTYPE_FCITX_SDK_ROOT=$SMARTTYPE_FCITX_SDK_ROOT")
fi
cmake "${cmake_args[@]}"
cmake --build "$BUILD_DIR" --parallel "${SMARTTYPE_BUILD_JOBS:-2}"
ctest --test-dir "$BUILD_DIR" --output-on-failure

mkdir -p "$BUNDLE/payload" "$BUNDLE/scripts" "$BUNDLE/config" "$OUT_DIR"
cmake --install "$BUILD_DIR" --prefix "$BUNDLE/payload"
# The source build intentionally rewrites this entry for the current user.
# Release archives must not retain the CI account's home directory.
sed -i 's|^Exec=.*|Exec=smarttype-tray|' \
    "$BUNDLE/payload/share/applications/smarttype-tray.desktop"
install -m755 "$ROOT/install.sh" "$BUNDLE/install.sh"
for script in install-user.sh doctor.sh uninstall-user.sh configure-fcitx-profile.py \
    configure-fcitx-x11.py fcitx5-layout-sync.py; do
    install -m755 "$ROOT/scripts/$script" "$BUNDLE/scripts/$script"
done
install -m644 "$ROOT/config/fcitx5-layout-sync.service" "$BUNDLE/config/fcitx5-layout-sync.service"
install -m644 "$ROOT/LICENSE" "$BUNDLE/LICENSE"
printf '%s\n' "$VERSION" > "$BUNDLE/VERSION"
printf '%s\n' "$TARGET" > "$BUNDLE/TARGET"

(cd "$BUNDLE/payload" && find . -type f -print0 | sort -z | xargs -0 sha256sum) \
    > "$BUNDLE/payload.sha256"
(cd "$BUNDLE/payload" && sha256sum --check "$BUNDLE/payload.sha256")
"$ROOT/tests/prebuilt_installer_tests.sh" "$BUNDLE/payload"

tar -czf "$OUT_DIR/$NAME.tar.gz" -C "$STAGE" "$NAME"
(cd "$OUT_DIR" && sha256sum "$NAME.tar.gz" > "$NAME.tar.gz.sha256")
echo "Created $OUT_DIR/$NAME.tar.gz"
