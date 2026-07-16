#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
# shellcheck source=../scripts/install-dependencies.sh
. "$ROOT/scripts/install-dependencies.sh"

TARGET=${1:-}
case "$TARGET" in
    fedora44) MODES=(kde-wayland gnome-wayland) ;;
    ubuntu2604) MODES=(kde-wayland gnome-wayland) ;;
    kali-rolling) MODES=(x11) ;;
    *)
        echo "Usage: tests/runtime_dependency_resolution.sh fedora44|ubuntu2604|kali-rolling" >&2
        exit 2
        ;;
esac

for mode in "${MODES[@]}"; do
    while IFS= read -r package; do
        [[ -n $package ]] || continue
        case "$TARGET" in
            fedora44)
                metadata=$(dnf -q --disablerepo='*' --enablerepo=fedora \
                    --enablerepo=updates repoquery --qf '%{name}' \
                    "$package" 2>/dev/null)
                grep -Fxq "$package" <<<"$metadata"
                ;;
            ubuntu2604|kali-rolling)
                metadata=$(apt-cache show "$package" 2>/dev/null)
                grep -q '^Package:' <<<"$metadata"
                ;;
        esac || {
            echo "$TARGET/$mode: runtime package is unavailable: $package" >&2
            exit 1
        }
    done < <(smarttype_runtime_packages "$TARGET" "$mode" | sort -u)
done

echo "$TARGET runtime dependency resolution passed"
