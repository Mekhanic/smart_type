#!/usr/bin/env bash
set -euo pipefail

DESTINATION=${1:-}
[[ -n $DESTINATION ]] || {
    echo "Usage: scripts/prepare-gnome-kimpanel.sh DESTINATION" >&2
    exit 2
}

KIMPANEL_COMMIT=ff828412608da89d8ede464c85649659a19a7650
KIMPANEL_SHA256=68fdf340af8c7281c9ca215f1505a475b0c28a27b7d00193f8a486dad2e0dc13
KIMPANEL_URL="https://github.com/wengxt/gnome-shell-extension-kimpanel/archive/${KIMPANEL_COMMIT}.tar.gz"

for command in tar sha256sum sed install; do
    command -v "$command" >/dev/null || {
        echo "Required command is missing: $command" >&2
        exit 1
    }
done

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT
archive="$tmpdir/kimpanel.tar.gz"

if [[ -n ${SMARTTYPE_KIMPANEL_ARCHIVE:-} ]]; then
    cp "$SMARTTYPE_KIMPANEL_ARCHIVE" "$archive"
else
    command -v curl >/dev/null || {
        echo "curl is required to download the pinned GNOME Kimpanel extension" >&2
        exit 1
    }
    curl --fail --show-error --location --retry 3 -o "$archive" "$KIMPANEL_URL"
fi

printf '%s  %s\n' "$KIMPANEL_SHA256" "$archive" | sha256sum --check --status || {
    echo "Kimpanel archive checksum verification failed" >&2
    exit 1
}

tar -xzf "$archive" -C "$tmpdir"
source_dir="$tmpdir/gnome-shell-extension-kimpanel-$KIMPANEL_COMMIT"
[[ -f $source_dir/metadata.json.in && -f $source_dir/COPYING ]] || {
    echo "Pinned Kimpanel archive has an unexpected structure" >&2
    exit 1
}

rm -rf "$DESTINATION"
install -d "$DESTINATION/schemas"
for file in extension.js indicator.js lib.js menu.js panel.js prefs.js stylesheet.css; do
    install -m644 "$source_dir/$file" "$DESTINATION/$file"
done

install -m644 "$source_dir/schemas/org.gnome.shell.extensions.kimpanel.gschema.xml" \
    "$DESTINATION/schemas/"
install -m644 "$source_dir/COPYING" "$DESTINATION/COPYING"
install -m644 "$source_dir/README" "$DESTINATION/README"
sed 's|@localedir@|/usr/share/locale|g' "$source_dir/metadata.json.in" \
    > "$DESTINATION/metadata.json"

if command -v glib-compile-schemas >/dev/null; then
    glib-compile-schemas "$DESTINATION/schemas"
else
    install -m644 "$source_dir/schemas/gschemas.compiled" "$DESTINATION/schemas/"
fi

printf '%s\n' "$KIMPANEL_COMMIT" > "$DESTINATION/UPSTREAM_COMMIT"
echo "Prepared GNOME Kimpanel $KIMPANEL_COMMIT in $DESTINATION"
