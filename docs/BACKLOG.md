# SmartType Backlog

This file contains only work that is currently actionable. Completed implementation history belongs in
`STATE.md`, verification results in `TEST_MATRIX.md`, and architectural decisions in `DECISIONS.md`.
Ideas and hypothetical platform work are not backlog items until manual testing or a product decision makes
them necessary.

## Current phase: owner manual testing

The automated suite is green. The previously recorded mixed-preedit cases
(`ghbdtn`, `plhfdcndeqnt`, and normal `здравствуйте`) are resolved according to owner verification on
2026-07-11. The 2026-07-13 controlled-Chromium duplicate defect is fixed in code and has focused integration coverage; owner verification in the original city combobox remains pending.

Use `MANUAL_TEST_SUITE.md` for the next testing pass. Record newly reproduced problems with:

- application and frontend;
- exact typed input and resulting text;
- typing speed when relevant;
- expected behavior;
- whether the problem survives an application restart;
- a screenshot or diagnostic excerpt when useful.

Do not create implementation tickets for behavior that has not been reproduced.

## Pending focused checks

These are verification checks, not presumed bugs:

- **ST-042 live check** — an automatic EN↔RU switch must not add an `RU`/`EN` row above candidates.
- **ST-026 Chrome check** — `F ns xnj ` should become `А ты что `; immediate Backspace should restore the source phrase.
- **ST-043 candidate anchor check** — with candidates open in a multiline field, click another line. The native popup must disappear immediately and must not follow the newly focused caret.
- **ST-044 Delete check** — in Telegram, Kate and Chrome, Delete must remove the character to the right of the caret and must never insert a square/control glyph.
- **ST-045 remaining clean-distro check** — Ubuntu 26.04 KDE Wayland is live-verified on 2026-07-14; run [DISTRO_TEST_PLAN.md](DISTRO_TEST_PLAN.md) in an Arch KDE Plasma Wayland VM before advertising Arch as a verified release target.
- **Controlled Chrome field check** — in the original dynamic city/region combobox, type `Махачкала`; the value must contain exactly `Махачкала`, with no cumulative prefixes.
- **Original-field tray/icon visual check** — reboot persistence now passes on Ubuntu KDE and Kali Xfce; the owner should still confirm the intended icon appearance in their production desktop panel.
- **Kali X11 owner visual pass** — automated live control passed in Mousepad, Firefox ESR and Kate with toolkit-inline preedit. The owner should repeat natural typing in the applications intended for the release claim; raw XIM remains compatibility-only.

If either check fails, reopen its existing ticket with the new evidence. If it passes, record the result in
`TEST_MATRIX.md`; no follow-up task is needed.

## Deferred until evidence or an explicit product goal

- KDE↔Fcitx layout snap-back investigation;
- smart-punctuation policy changes;
- additional sentence-capitalization work;
- client-side renderer styling research;
- XWayland coordinate mapping and support;
- GNOME/non-KDE portability;
- long-session performance work;
- native RPM/DEB package recipes and CI matrix (the portable source installer is implemented).

These entries intentionally have no ticket status. Reintroduce only the ones justified by manual testing or a
chosen release target.

## Retired items

- **GitHub publication gate** — completed on 2026-07-14. The public `main`
  branch contains one root commit, GPL-3.0, source installation documentation,
  community files, and a green Fedora 44 CI workflow. Release claims remain
  limited to the exact desktop/session matrix in `README.md`.
- **ST-012 Theme Lab** — completed experiment, deprecated. It was not useful for final design work; production
  panel design continued directly under ST-015. No maintenance or further development is planned.
- Duplicate historical entries for ST-010/011/013 and ST-023/024 were removed from this backlog.
- ST-027, ST-029, ST-030, and ST-035 were already satisfied by current behavior and automated coverage.
