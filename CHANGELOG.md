# Changelog

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
