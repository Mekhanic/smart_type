# Changelog

## 0.2.3 - 2026-07-16

- Make GNOME extension activation remove Kimpanel and AppIndicator from
  `disabled-extensions` as well as adding them to `enabled-extensions`. This
  fixes a clean reinstall where Fcitx loaded `classicui` after reboot even
  though the installer and stored enabled list requested Kimpanel.

## 0.2.2 - 2026-07-16

- Stop KDE's periodic layout observation from forcing an explicitly selected
  Russian SmartType method back to English.
- Make proactive layout correction undo transactional: `Ghbdtn` → `Привет`
  now restores exact `Ghbdtn` with immediate Backspace and exact `Ghbdtn `
  after Space plus Backspace.
- Cancel candidates, client preedit and the pending edit transaction when the
  application moves the caret; a later delimiter can no longer insert the old
  candidate at the new position.
- Preserve stateless RU/EN mapping while SmartType is globally paused, paused
  for the current application or excluding a terminal. Correction, candidates,
  learning and edit transactions remain disabled, and shortcuts pass through.
- Configure the Fcitx group selected first by `GroupOrder` instead of assuming
  it is numeric `Groups/0`; this fixes Kali/X11 cold starts stuck on the
  technical `keyboard-us` method while preserving unrelated groups.

- Make clean Fedora Plasma logins select the distro Fcitx 5 imsettings profile
  at `~/.config/imsettings/xinputrc`. Fedora's early Plasma environment hook
  otherwise replaces `XMODIFIERS=@im=fcitx` with `@im=none`, leaving a healthy
  tray and loaded addon unable to receive application input after reboot.
- Preserve and restore existing imsettings selections and user autostart
  overrides during install/uninstall; add release-bundle regression coverage.
- Fixed GNOME Wayland input being rejected because compositor-proxied
  application contexts identify their program as `gnome-shell`.
- Persist Fcitx addon overrides in the global configuration so GNOME really
  uses Kimpanel and does not load the native KDE/Wayland SmartType UI.
- Explicitly enable SmartType in a new personal database while preserving an
  existing user's deliberate disabled state during upgrades.
- Made the doctor compare the configured addon policy with Fcitx's actual
  running UI instead of accepting ineffective addon-local flags.
- Added regression coverage for GNOME compositor contexts, fresh-database
  activation and persistent addon selection.
- Fixed GNOME IBus corruption during uninterrupted layout correction by keeping
  its correction transactions immediate instead of appending new keys to a
  delayed highlighted preedit.
- Clear client preedit before committing a Tab-selected candidate so the caret
  remains after the inserted candidate and trailing space.
- Avoid unsafe multi-word `deleteSurroundingText` rewrites through Mutter's
  asynchronous IBus bridge; ordinary per-word RU/EN correction remains active.
- Use LibreOffice's GTK VCL/Fcitx frontend when LibreOffice is already present,
  and fail closed on raw XIM contexts without preedit instead of risking
  duplicated or over-deleted document text.
- Added installer and integration regressions for conditional
  `libreoffice-gtk3` setup and the raw-XIM safety predicate.
- Normalize the full physical punctuation row on logical Russian layouts:
  `[ ] ; ' , . /` now produces `х ъ ж э б ю .` instead of leaking US symbols.
  Physical XKB codes distinguish Russian `ю` from the adjacent Russian dot,
  with shifted and Caps Lock variants covered by regression tests.
- Keep GNOME Kimpanel candidates interactive when SmartType intentionally omits
  numeric labels; mouse hover is now visible and a click commits the pointed
  candidate. Existing pinned extension copies are patched during upgrades.
- Restore accidental-case correction for learned words, so a previously learned
  `понял` no longer prevents `ПОнял` from becoming `Понял`.
- Migrate historical Fedora `lib64/fcitx5` user addons to the canonical
  `lib/fcitx5` directory without deleting unrelated addons, and make the doctor
  fail when the running Fcitx still has a stale/non-canonical engine loaded.
- Recognize QTerminal and current terminal families when “Do not correct in
  terminals” is enabled. On logical-layout desktops, terminals retain only the
  stateless RU/EN character mapping: no autocorrection, candidates, learning or
  SmartType editing transactions, while Alt+Shift and terminal shortcuts remain
  application-controlled.
- Remove the legacy later-sorting `fcitx5-smarttype.conf` environment drop-in
  during upgrades. It could override the canonical `~/.local/lib/fcitx5` path
  with the retired Fedora `lib64` directory and leave both SmartType methods
  visible but unavailable after login. Doctor now validates the effective
  lexical-order result instead of accepting any `FCITX_ADDON_DIRS` line.

## 0.2.1 - 2026-07-15

- Fixed the one-line release bootstrap on real distributions: sourcing
  `/etc/os-release` no longer overwrites the GitHub release tag through its
  standard `VERSION` variable.
- Added an end-to-end regression fixture containing the conflicting
  `VERSION="..."` field before publishing the patch release.

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
