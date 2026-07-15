#!/usr/bin/env bash
set -euo pipefail

REPOSITORY=${SMARTTYPE_GITHUB_REPOSITORY:-Mekhanic/smart_type}
RELEASE_VERSION=${SMARTTYPE_VERSION:-}
MODE=auto
INSTALL_DEPS=1
ASSUME_YES=0

usage() {
    cat <<'EOF'
Usage: install-release.sh [--version vX.Y.Z]
                          [--mode auto|kde-wayland|gnome-wayland|x11]
                          [--skip-deps] [--yes]

Downloads the matching SmartType release from GitHub, verifies its SHA-256
checksum, and installs it for the current user without compiling source code.
EOF
}

while (($#)); do
    case "$1" in
        --version)
            [[ $# -ge 2 ]] || { echo "--version requires a value" >&2; exit 2; }
            RELEASE_VERSION=$2
            shift
            ;;
        --mode)
            [[ $# -ge 2 ]] || { echo "--mode requires a value" >&2; exit 2; }
            MODE=$2
            shift
            ;;
        --skip-deps) INSTALL_DEPS=0 ;;
        -y|--yes) ASSUME_YES=1 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

# With `curl ... | bash`, stdin contains the script itself and must never be
# consumed by apt/dnf confirmation prompts. Sudo still uses the controlling
# terminal for a password when one is required.
if [[ ! -t 0 ]]; then
    ASSUME_YES=1
fi

for command in curl tar sha256sum; do
    command -v "$command" >/dev/null || {
        echo "Required command is missing: $command" >&2
        exit 1
    }
done

case "$(uname -m)" in
    x86_64|amd64) ARCH=x86_64 ;;
    *)
        echo "Prebuilt SmartType releases currently support x86_64 only." >&2
        echo "Clone the repository and run ./install.sh --build-from-source instead." >&2
        exit 1
        ;;
esac

TARGET=${SMARTTYPE_RELEASE_TARGET:-}
if [[ -z $TARGET ]]; then
    os_release_file=${SMARTTYPE_OS_RELEASE_FILE:-/etc/os-release}
    [[ -r $os_release_file ]] || { echo "Cannot identify this Linux distribution." >&2; exit 1; }
    # shellcheck source=/dev/null
    . "$os_release_file"
    case "${ID:-}:${VERSION_ID:-}" in
        fedora:44) TARGET=fedora44 ;;
        ubuntu:26.04) TARGET=ubuntu2604 ;;
        kali:*) TARGET=kali-rolling ;;
        *)
            echo "No prebuilt SmartType release for ${PRETTY_NAME:-${ID:-unknown}}." >&2
            echo "Supported: Fedora 44, Ubuntu 26.04, Kali Rolling (x86_64)." >&2
            exit 1
            ;;
    esac
fi
case "$TARGET" in
    fedora44|ubuntu2604|kali-rolling) ;;
    *) echo "Invalid SmartType release target: $TARGET" >&2; exit 2 ;;
esac

if [[ -z $RELEASE_VERSION ]]; then
    echo "==> Resolving the newest SmartType release"
    release_api=${SMARTTYPE_RELEASE_API_URL:-https://api.github.com/repos/$REPOSITORY/releases/latest}
    release_json=$(curl --fail --silent --show-error --location --retry 3 "$release_api")
    RELEASE_VERSION=$(sed -n 's/^[[:space:]]*"tag_name": "\([^"]*\)",$/\1/p' <<<"$release_json" | head -n1)
    [[ -n $RELEASE_VERSION ]] || { echo "Could not resolve a SmartType release tag." >&2; exit 1; }
fi

archive="smarttype-${RELEASE_VERSION}-${TARGET}-${ARCH}.tar.gz"
base_url=${SMARTTYPE_RELEASE_BASE_URL:-https://github.com/$REPOSITORY/releases/download/$RELEASE_VERSION}
tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

echo "==> Downloading SmartType $RELEASE_VERSION for $TARGET"
curl --fail --show-error --location --retry 3 -o "$tmpdir/$archive" "$base_url/$archive"
curl --fail --show-error --location --retry 3 -o "$tmpdir/$archive.sha256" "$base_url/$archive.sha256"

echo "==> Verifying SHA-256 checksum"
checksum_lines=$(grep -cve '^[[:space:]]*$' "$tmpdir/$archive.sha256")
expected_hash=$(awk 'NF { print $1; exit }' "$tmpdir/$archive.sha256")
expected_name=$(awk 'NF { print $2; exit }' "$tmpdir/$archive.sha256")
expected_name=${expected_name#\*}
if [[ $checksum_lines -ne 1 || ! $expected_hash =~ ^[0-9a-fA-F]{64}$ || $expected_name != "$archive" ]]; then
    echo "Invalid checksum file for $archive" >&2
    exit 1
fi
actual_hash=$(sha256sum "$tmpdir/$archive" | awk '{print $1}')
[[ ${actual_hash,,} == ${expected_hash,,} ]] || {
    echo "SHA-256 mismatch for $archive" >&2
    exit 1
}
echo "$archive: OK"

tar -tzf "$tmpdir/$archive" > "$tmpdir/archive-contents"
while IFS= read -r entry; do
    case "$entry" in
        /*|../*|*/../*|*/..) echo "Unsafe path in release archive: $entry" >&2; exit 1 ;;
    esac
done < "$tmpdir/archive-contents"
tar -xzf "$tmpdir/$archive" -C "$tmpdir"
bundle_dir="$tmpdir/${archive%.tar.gz}"
[[ -x "$bundle_dir/install.sh" && -d "$bundle_dir/payload" ]] || {
    echo "Invalid SmartType release archive." >&2
    exit 1
}

args=(--prebuilt-dir "$bundle_dir/payload" --mode "$MODE")
(( INSTALL_DEPS )) || args+=(--skip-deps)
(( ASSUME_YES )) && args+=(--yes)
"$bundle_dir/install.sh" "${args[@]}"
