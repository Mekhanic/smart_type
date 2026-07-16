#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
TEST_ROOT=$(mktemp -d)
trap 'rm -rf "$TEST_ROOT"' EXIT

# shellcheck source=../scripts/install-dependencies.sh
. "$ROOT/scripts/install-dependencies.sh"

MOCK_BIN="$TEST_ROOT/bin"
mkdir -p "$MOCK_BIN"

cat > "$MOCK_BIN/rpm" <<'EOF'
#!/usr/bin/env bash
[[ $1 == -q && $2 == libreoffice-core ]] || exit 64
[[ ${SMARTTYPE_TEST_LO_INSTALLED:-0} == 1 ]]
EOF
cat > "$MOCK_BIN/dpkg-query" <<'EOF'
#!/usr/bin/env bash
[[ $1 == -W && $2 == '-f=${Status}\n' && $3 == libreoffice-core ]] || exit 64
if [[ ${SMARTTYPE_TEST_LO_INSTALLED:-0} == 1 ]]; then
    printf '%s\n' 'install ok installed'
    exit 0
fi
exit 1
EOF
chmod +x "$MOCK_BIN/rpm" "$MOCK_BIN/dpkg-query"
PATH="$MOCK_BIN:$PATH"

fedora_packages=$(smarttype_runtime_packages fedora44 kde-wayland)
grep -Fxq libwayland-client <<<"$fedora_packages"
grep -Fxq libwayland-cursor <<<"$fedora_packages"
grep -Fxq sqlite-libs <<<"$fedora_packages"
! grep -Fxq wayland-libs <<<"$fedora_packages"

debian_packages=$(smarttype_runtime_packages ubuntu2604 kde-wayland)
grep -Fxq libwayland-client0 <<<"$debian_packages"
grep -Fxq libwayland-cursor0 <<<"$debian_packages"
grep -Fxq libsqlite3-0 <<<"$debian_packages"
grep -Fxq libglib2.0-0t64 <<<"$debian_packages"
grep -Fxq libqt6core6t64 <<<"$debian_packages"

fedora_gnome_packages=$(smarttype_runtime_packages fedora44 gnome-wayland)
grep -Fxq fcitx5-autostart <<<"$fedora_gnome_packages"
grep -Fxq gnome-shell-extension-appindicator <<<"$fedora_gnome_packages"

ubuntu_gnome_packages=$(smarttype_runtime_packages ubuntu2604 gnome-wayland)
grep -Fxq fcitx5-frontend-gtk4 <<<"$ubuntu_gnome_packages"
grep -Fxq gnome-shell-ubuntu-extensions <<<"$ubuntu_gnome_packages"

for target in fedora44 ubuntu2604 kali-rolling; do
    packages=$(SMARTTYPE_TEST_LO_INSTALLED=0 \
        smarttype_optional_desktop_packages "$target" gnome-wayland)
    [[ -z $packages ]] || {
        echo "$target unexpectedly installs LibreOffice" >&2
        exit 1
    }

    packages=$(SMARTTYPE_TEST_LO_INSTALLED=1 \
        smarttype_optional_desktop_packages "$target" gnome-wayland)
    [[ $packages == libreoffice-gtk3 ]] || {
        echo "$target did not add the LibreOffice GTK integration" >&2
        exit 1
    }

    packages=$(SMARTTYPE_TEST_LO_INSTALLED=1 \
        smarttype_optional_desktop_packages "$target" x11)
    [[ $packages == libreoffice-gtk3 ]] || exit 1

    packages=$(SMARTTYPE_TEST_LO_INSTALLED=1 \
        smarttype_optional_desktop_packages "$target" kde-wayland)
    [[ -z $packages ]] || {
        echo "$target KDE mode unexpectedly forces the GTK backend" >&2
        exit 1
    }
done

echo "Optional desktop dependency test passed"
