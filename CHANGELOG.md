# Changelog

## 0.2.0 - 2026-07-15

- Added GNOME Wayland integration for Fedora 44 and Ubuntu 26.04.
- Added Fcitx IBus/Kimpanel configuration while keeping composition in the
  focused application field.
- Added a checksum-pinned upstream Kimpanel GNOME Shell extension to release
  bundles and preserved existing GNOME extensions during configuration.
- Hardened the one-line release installer with release lookup retries, strict
  checksum parsing, archive path-traversal rejection, and pipe-safe package
  installation.
- Added automated GNOME configuration, idempotency and staged-install tests.

## 0.1.1 - 2026-07-15

- Added checksum-verified prebuilt x86_64 bundles for Fedora 44, Ubuntu 26.04,
  and Kali Rolling.
- Added a one-command installer that does not compile on the user's computer.
- Added a GitHub Actions release matrix that builds and tests each bundle in
  its matching distribution container before publishing release assets.
- Kept the complete source-build path as `./install.sh --build-from-source`.
- Installed the doctor and uninstaller scripts alongside the application.
- Made common Russian corrections and one-letter candidate behavior stable
  across distribution Hunspell dictionary updates.
- Added Fedora's KOI8-R conversion runtime so the Russian dictionary works on
  minimal installations as well as full desktops.
- Added the explicit English Hunspell dictionary on Ubuntu and Kali so layout
  detection does not depend on a desktop metapackage.

## 0.1.0 - 2026-07-14

Initial public preview.

- Context-aware Russian and English autocorrection through Fcitx 5.
- Candidate panel, keyboard navigation, punctuation handling, and Backspace
  revert.
- Automatic correction of text entered with the wrong RU/EN layout.
- Personal dictionary, custom rules, local learning, correction history, and
  a privacy-conscious problem report.
- Native candidate rendering for KDE Wayland and X11 toolkit applications.
- User-local installer verified on Fedora 44 KDE Wayland, Ubuntu 26.04 KDE
  Wayland, and Kali Rolling Xfce/X11.
- Managed tray startup, adaptive light/dark tray artwork, and project support
  links.
