#!/usr/bin/env bash

smarttype_runtime_packages() {
    local target=$1 mode=$2
    case "$target" in
        fedora44)
            printf '%s\n' \
                fcitx5 fcitx5-qt fcitx5-gtk fcitx5-configtool \
                hunspell hunspell-ru glibc-gconv-extra \
                qt6-qtbase qt6-qtdeclarative cairo pango gdk-pixbuf2 glib2 \
                sqlite-libs libwayland-client libwayland-cursor \
                libxcb xcb-util xcb-util-wm xcb-util-keysyms
            if [[ $mode == gnome-wayland ]]; then
                printf '%s\n' fcitx5-autostart gnome-shell-extension-appindicator
            fi
            ;;
        ubuntu2604|kali-rolling)
            printf '%s\n' \
                fcitx5 fcitx5-frontend-qt6 fcitx5-frontend-gtk3 fcitx5-config-qt \
                hunspell hunspell-ru hunspell-en-us \
                libqt6core6t64 libqt6gui6 libqt6widgets6 libqt6dbus6 libqt6qml6 \
                libcairo2 libpango-1.0-0 libgdk-pixbuf-2.0-0 libglib2.0-0t64 \
                libsqlite3-0 libwayland-client0 libwayland-cursor0 \
                libxcb1 libxcb-util1 libxcb-icccm4 libxcb-xinerama0 \
                libxcb-randr0 libxcb-ewmh2 libxcb-keysyms1
            if [[ $mode == gnome-wayland && $target == ubuntu2604 ]]; then
                printf '%s\n' fcitx5-frontend-gtk4 gnome-shell-ubuntu-extensions
            fi
            ;;
        *) return 2 ;;
    esac
}

smarttype_libreoffice_is_installed() {
    local target=$1
    case "$target" in
        fedora44) rpm -q libreoffice-core >/dev/null 2>&1 ;;
        ubuntu2604|kali-rolling)
            dpkg-query -W -f='${Status}\n' libreoffice-core 2>/dev/null |
                grep -Fxq 'install ok installed'
            ;;
        *) return 1 ;;
    esac
}

smarttype_optional_desktop_packages() {
    local target=$1 mode=$2
    case "$mode" in
        gnome-wayland|x11) ;;
        *) return 0 ;;
    esac

    # Do not pull in the full office suite for users who do not have it. When
    # LibreOffice is present, its GTK VCL plugin provides the supported Fcitx
    # toolkit path instead of the unsafe generic raw-XIM fallback.
    if smarttype_libreoffice_is_installed "$target"; then
        printf '%s\n' libreoffice-gtk3
    fi
}
