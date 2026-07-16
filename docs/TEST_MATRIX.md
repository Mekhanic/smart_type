# SmartType Release Test Matrix

This file records only the current `0.2.2` release evidence. Historical release
changes belong in `CHANGELOG.md`.

## Automated suite

The native build and Fedora/Ubuntu/Kali release builds run 17 tests:

| Area | Required result |
|---|---|
| Core correction, learning, case and punctuation | PASS |
| Fcitx integration and text transactions | PASS |
| Fcitx profile preservation and multi-group startup selection | PASS |
| KDE/X11 layout ownership and technical `keyboard-us` recovery | PASS |
| GNOME Fcitx/Kimpanel configuration | PASS |
| Release bootstrap, dependencies, payload and archive validation | PASS |
| Native X11 renderer symbol and addon loading smoke | PASS |
| Tray startup, settings, icons and report dialog | PASS |

The focused regressions include:

- KDE layout 0 must not periodically force `smarttype-us` after an explicit
  switch to Russian;
- `Ghbdtn` → `Привет`, Backspace restores exact `Ghbdtn`;
- `Ghbdtn` → `Привет`, Space, Backspace restores exact `Ghbdtn `;
- moving the caret with candidates open discards preedit and the pending
  transaction; the following Space commits only one space;
- global pause, per-application pause and terminal exclusion retain RU/EN
  mapping without correction UI or learning;
- `Ctrl+C` passes through terminal and disabled-context paths;
- a startup Fcitx group selected by `GroupOrder` receives SmartType even when
  it is stored outside `Groups/0`;
- GNOME compositor, Chromium controlled-field, LibreOffice raw-XIM fail-closed,
  punctuation-row and Kimpanel mouse regressions.

The clean local pre-tag rebuilds passed the same 17 tests, payload verification,
prebuilt-installer smoke and extracted-bundle installation smoke. The tag
workflow rebuilds these artifacts and publishes separate checksum files:

| Archive | SHA-256 |
|---|---|
| `smarttype-v0.2.2-fedora44-x86_64.tar.gz` | `34177919a63d6fde57a2251ad4563355575d03586ba45c09cec2ac136a3db868` |
| `smarttype-v0.2.2-ubuntu2604-x86_64.tar.gz` | `107ac3a684ae41cf130bb768267d514b8c1546edf5aa2c0504bb980a39c18822` |
| `smarttype-v0.2.2-kali-rolling-x86_64.tar.gz` | `da963cd71fe23db722ba36ea167f7f3ba6a6e2b0e663ccfe7b829683d2848835` |

## Installed and owner verification

| Check | Fedora 44 KDE/Wayland | Ubuntu 26.04 GNOME/Wayland | Kali Rolling Xfce/X11 |
|---|---|---|---|
| Clean state before bootstrap | PASS | PASS | PASS |
| One-line prebuilt install, no compilation | PASS | PASS | PASS |
| Full VM reboot after install | PASS | PASS | PASS |
| Canonical engine loaded by running Fcitx | PASS | PASS | PASS |
| Correct candidate renderer active | native Wayland PASS | Kimpanel PASS | native XCB PASS |
| Tray and layout services healthy | PASS | PASS | PASS |
| `doctor.sh` has no FAIL | PASS | PASS | PASS |
| Manual RU/EN switching | PASS | PASS | PASS |
| Autocorrection and candidates | PASS | PASS | PASS |
| Backspace undo before/after delimiter | PASS | PASS | PASS |
| Candidate mouse selection | PASS | PASS | PASS |
| External caret cancels candidates | PASS | PASS | PASS |
| Terminal exclusion | automated PASS | owner PASS | owner PASS |
| Paused context retains layout switching | automated PASS | owner PASS | owner PASS |

Applications used across the owner passes include Telegram/Kate on Fedora;
LibreOffice Writer and GNOME Terminal on Ubuntu; and Mousepad, Firefox ESR,
Kate and QTerminal on Kali.

## Manual release smoke

After installing a tagged archive on a restored clean overlay:

1. Confirm `doctor.sh` contains no `FAIL` and `fcitx5-remote -n` returns
   `smarttype-us` or `smarttype`.
2. Type `севодня ` and verify `сегодня `; press Backspace and verify
   `севодня `.
3. Type `Ghbdtn`; verify `Привет`; press Backspace and verify `Ghbdtn`.
4. Repeat with Space before Backspace and verify `Ghbdtn `.
5. Open candidates, click another line, then press Space. The old candidate
   must not be inserted at the new caret.
6. Select a candidate with the mouse.
7. In a terminal, verify literal RU/EN typing and Alt+Shift with no correction
   or candidate popup. Verify `Ctrl+C` remains a terminal shortcut.
8. Pause SmartType for the current application and verify that RU/EN switching
   still works while correction and candidates remain disabled.

## Release boundaries

- Ubuntu Firefox Snap's toolkit-owned legacy panel may be positioned away from
  the caret; ordinary correction still works.
- Raw XIM and XWayland-only contexts are not accepted as supported inline UX.
- Fedora GNOME is installer/CI-supported but has not received the same owner
  visual pass as Ubuntu GNOME.
- Ubuntu 24.04 and Arch are not verified release targets.
