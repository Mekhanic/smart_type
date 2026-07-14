# SmartType Project State

## 1. Project Phase & Git Status
- **Public Branch**: `main` (`v0.1.0` public preview)
- **Current focus**: owner manual regression pass in the original applications and feedback from the first public preview.
- **Recently completed**:
  - **Prebuilt release channel** (2026-07-15): the normal installer no longer compiles SmartType on the user's computer. A checksum-verifying bootstrap selects a Fedora 44, Ubuntu 26.04, or Kali Rolling x86_64 bundle; GitHub Actions builds each bundle in its matching distribution container, runs the complete CTest suite, and attaches the archives plus SHA-256 files to a tagged release. Source compilation remains available through `./install.sh --build-from-source`.
  - **Initial public preview published** (2026-07-14): `https://github.com/Mekhanic/smart_type` is public. Its `main` branch contains one root commit rather than the private development history; tag and prerelease `v0.1.0` point to that commit. The repository includes the official GNU GPL-3.0 text, CI, issue templates, contribution and security policies, topics, and private vulnerability reporting. Fedora 44 CI builds the complete project and passes 12/12 tests.
  - **GPL-3.0 and project support link** (2026-07-14): the repository contains the official GNU GPL-3.0 text. The tray menu and a dedicated “О проекте” settings tab open `https://github.com/Mekhanic/smart_type` for issues, feedback and project support. The final publication UI was rebuilt from the current tree on Fedora 44, Ubuntu 26.04 and Kali Rolling; all three passed 12/12 tests, and each installed tray binary contains the exact project URL and passes `--check-settings`.
  - **Release installer and icon policy** (2026-07-14): the tray now embeds `dark_theme_logo.png` and `white_theme_logo.png`, selects the asset for the current Qt palette, and updates on a live palette change. Runtime state no longer changes the tray artwork; `icon_idle.png` is the sole application/desktop icon. The stale missing `icon_pause.png` clean-build dependency and installed legacy state icons were removed; `tray-icons-smoke` verifies all embedded assets. Root `install.sh` detects the three verified distro/session targets, installs dependencies, limits compilation to two jobs by default, runs the complete build/test/install path, configures Fcitx without deleting existing methods, and enables the matching Wayland or X11 services. The Fcitx profile configurator is idempotent and covered by an automated preservation test. The final source tree was redeployed on Fedora 44 KDE Wayland, Ubuntu 26.04 KDE Wayland, and Kali Rolling Xfce/X11; each environment passed 12/12 tests and the installer completed. Fedora/Kali doctor are green, and the Ubuntu cold-login pass is recorded in the public support matrix.
  - **Kali Xfce/X11 inline toolkit integration completed** (2026-07-14): the prior server-side XIM preedit workaround was rejected because it rendered the typed token inside the popup instead of the target field. That workaround has been removed. The explicit X11 installer now configures the installed GTK3/4 and Qt5/6 Fcitx modules in both `environment.d` and a managed `.xprofile` block, without changing Wayland modes. After cold login, Mousepad, Firefox ESR and Kate all inherited the three Fcitx variables, reported focused `frontend:dbus` contexts, rendered `вопщем` inside their own widgets, and showed a candidate-only native X11 popup. Candidate selection committed exactly `в общем ` and immediate Backspace restored `вопщем ` in all three applications. Alt+Shift produced `smarttype → smarttype-us → smarttype` and `hello вопщем `. The suite is 10/10 PASS; release evidence is summarized in `TEST_MATRIX.md`.
  - **Native X11 renderer restored in the production build** (2026-07-13): the Kali verification report claimed PASS even though top-level CMake forced `ENABLE_X11=OFF`, so `xcbui.cpp` and `xcbinputwindow.cpp` were absent from the installed `smarttypeui.so`. `SMARTTYPE_ENABLE_X11=ON` is now the default, required XCB dependencies are explicit, addon smoke asserts the `XCBInputWindow::update` symbol, and `doctor.sh` fails in an X11 session when the installed UI is Wayland-only. A clean local X11-enabled build passes CTest 8/8 and exports the XCB renderer.
  - **Controlled Chromium inputs + managed tray lifecycle** (2026-07-13): live Chrome logs reproduced cumulative client-preedit materialisation while typing `Махачкала` in a React-controlled city combobox. Chromium-family contexts now commit literal keys immediately while retaining the logical word buffer for correction; corrections continue to use forwarded Backspaces. The integration harness asserts exactly one commit per letter for `махачкала`. Tray ownership moved from a one-shot XDG autostart entry to `smarttype-tray.service` with restart recovery; tray bitmaps are embedded in the binary and the desktop icon is installed in the hicolor-supported `512x512/apps` directory. CTest 8/8; the final installed service restarted successfully after SIGTERM (`60502→60683`, `NRestarts=1`). The old idle/pause icon split was superseded on 2026-07-14.
  - **Settings and tray clarity pass** (2026-07-12): the tray no longer offers
    a misleading “close icon” action that removed the only obvious route back
    to settings. Settings are grouped by outcome, legacy renderer internals
    are not exposed as a user toggle, and application exclusions, personal
    dictionary, and disabled corrections now explain their purpose and empty
    state. The current focused application can be excluded with one action.
    `settings-smoke` and the full CTest suite pass; owner visual check remains
    appropriate.
  - **Candidate dismissal releases editing keys** (2026-07-12): Down/Up previously removed only the candidate list while retaining client preedit, so Left/Right and Delete were still captured until a mouse click reset the field. Down/Up now commit the literal composition and close the panel, immediately returning navigation and editing to the application. CTest 8/8.
  - **Runtime disabled-state detection + Delete forwarding** (2026-07-12): after reboot, Fcitx and the tray were healthy but the persisted `settings.enabled=0` made SmartType intentionally ignore all input. Restored `enabled=1`, confirmed it survives an Fcitx restart, and taught `doctor.sh` to fail on the false-healthy state. Delete is now explicitly forwarded to the application rather than being swallowed/rendered as a control-character square; CTest 8/8 after the change. Live Telegram check is pending.
  - **Candidate caret-anchor race + portable source install** (2026-07-11–12): a cursor-rect change inside the former 75 ms post-key exception could re-anchor the native candidate panel to a mouse click. External caret movement now always dismisses the active list; isolated Fcitx integration covers the immediate-click boundary. CMake installs addons through `CMAKE_INSTALL_LIBDIR` (validated with a staged `~/.local/lib/fcitx5` install), and the user installer no longer assumes Fedora, `rpm`, `dnf`, `lib64`, KDE, or `wayland-0`. Ubuntu verification is complete: successfully built and ran the 8 CTests on Ubuntu 24.04 inside a container, resolving cross-distro issues with dynamic Hunspell encoding lookup (detecting UTF-8 vs KOI8-R) and GCC 13 compiler warnings. Arch live-session verification remains pending.
  - **kimpanel / UIPriority fix** (2026-07-11): On VM (and any fresh install) kimpanel (`UIPriority=50`) was intercepting candidate rendering before `smarttypeui`, showing the plain flat Plasma system popup instead of the native SmartType panel. Fixed by adding `UIPriority=100` to `smarttypeui.conf.in.in` and explicitly disabling kimpanel in `~/.config/fcitx5/conf/kimpanel.conf` via `install-user.sh`. Also added `WAYLAND_DISPLAY=wayland-0` to `90-smarttype-user.conf.in` so systemd-launched processes (tray) can connect to the Wayland compositor. `doctor.sh` all-OK on VM (18 OK, 1 WARN for optional `~/.xinputrc`).
  - **Candidate dismissal + historical tray icons** (2026-07-11): external caret relocation hides the active candidate panel instead of moving it. The former idle/pause icon split was superseded by the single-state adaptive tray policy on 2026-07-14.
  - **Candidate interaction + punctuation controls** (2026-07-11): native panel hover now keeps the last candidate while the pointer crosses inter-candidate gaps; one-glyph highlights enforce capsule geometry; smart typography adds `--`→`—`, `<<`→`«`, `>>`→`»`; automatic spacing after punctuation is an independent setting. CTest 8/8; installed and loaded for owner testing.
  - **ST-042** (2026-07-10): Suppress Fcitx compact IM toast (`RU`/`EN`) after programmatic layout switch so the candidate panel no longer grows upward. Engine clears matching `auxUp` after `setCurrentInputMethod`; smarttypeui drops the same labels from upper layout. CTest 8/8.
  - **ST-028** (2026-07-10): Telegram/AyuGram session IM durability after ST-018. Post-reboot audit (boot 09:30): `QT_IM_MODULE` unset, `XMODIFIERS=@im=fcitx5`, `IMSETTINGS_MODULE=fcitx5`; Telegram/AyuGram ICs keep rich `cap:8000d0072` without clean-launch. `doctor.sh` gains FAIL/WARN checks for xim/none/imsettings-start.
  - **ST-026** (2026-07-10): Chrome phrase rewrite delete path. Chromium-family clients advertise SurroundingText but ignore `deleteSurroundingText`, leaving `F ns А ты что`. `erase_committed` / phrase rewrite / undo now use Backspace forwards for chrome/chromium/edge/brave/…; integration test asserts ≥5 Backspaces on `google-chrome` IC. CTest 8/8.
  - **ST-041 / ST-041.1** (2026-07-10–11): Deferred physical IM switching, queued-key coercion and reverse-layout guards fixed the mixed-preedit family. Owner confirms `ghbdtn`, `plhfdcndeqnt`, and normal `здравствуйте` no longer reproduce the problem on the current installed build.
  - **ST-022** (2026-07-10): Installed and loaded identifiable build `15d472c`; `doctor` is fully green, tray and layout-sync are active, and KDE Alt+Shift synchronizes both directions. Real Wayland smoke found two P0 failures: proactive layout switching can leave mixed text (`ghbвет`) in Kate at normal speed and in Chrome/Telegram during a 5 ms burst; Chrome phrase rewrite commits `А ты что` but fails to remove the original `F ns ` prefix. Telegram normal typing, phrase rewrite, typo correction and Backspace revert pass; Chrome normal correction, typo revert and domain protection pass.
  - **ST-034** (2026-07-10): Honest automated-test wiring — UI cases run independently, addon smoke validates the current build artifact, Fcitx integration refuses non-isolated data/config/state paths, manual-rule learning is asserted, and the no-`SurroundingText` path covers correction, timer-based Backspace revert, and continued typing. No test kills or touches a live `smarttype-ui` process.
  - **ST-021.2** (2026-07-09): `вщ нщг`→`do you` — RU→EN **len=2 auto** (`вщ`→`do`); phrase rewrite only matches tokens actually in SurroundingText (no optimistic rewrite of old context like `ghjtrn`→`проект`). Manual suite: `docs/MANUAL_TEST_SUITE.md`.
  - **ST-021.1 / ST-023 / ST-024** (2026-07-09): Reverse phrase (`ш ерштл`→`i think`); SurroundingText char offsets; coerce after proactive (`thinл`→`think`); no auto-split compounds; domain protect.
  - **ST-021** (2026-07-09): Phrase layout rewrite EN→RU; user verified. Committed `6681f3e`.
  - **ST-020** (2026-07-09): layout-sync enable on install; committed `8ace432`.
  - **Audit snapshot** (2026-07-09): Phase 6 backlog ST-022…040.
  - **ST-019**: Layout switch UX — snapshot undo, expire undo after continue-type, word-boundary clear, demote learning for proactive pairs.
  - **ST-012.1**: `CandidatePanelScale` + geometry freeze in smarttypeui/theme (committed).
  - **ST-016**: Tray panel-scale slider; its original `~/.config/autostart` lifecycle was superseded by the managed user service on 2026-07-13.
  - **ST-017**: Engine layout logical IM sync (committed + `smarttype.so` installed; Fcitx reloaded with `FCITX_ADDON_DIRS`).
  - **ST-018**: Session IM env — disabled user `imsettings-start`, `~/.xinputrc` → fcitx5, `environment.d/91-smarttype-im-wayland.conf` (`XMODIFIERS=@im=fcitx`, no `QT_IM_MODULE=xim`). Current session: `QT_IM_MODULE` unset.
- **Critical finding (2026-07-09)**: Telegram “SmartType dead” was **IM session environment**, not ST-017. See ADR-006.
- **Critical finding (2026-07-09, ST-020)**: “Alt+Shift only changes system layout” = layout-sync service not running, not a Kate/Chrome bug.

---

## 2. Feature Implementation Status

### Core Functions (P0 Priority)
- **Typing Input & Delimiter Logic**: Implemented. Stable clients use client preedit; Chromium-family clients use immediate literal commits plus an internal word buffer so controlled web inputs cannot duplicate cumulative preedit. ST-041 still defers physical IM switches for true client-preedit paths.
- **Autocorrection / Suggestions Generation**: Implemented.
- **SQLite Learned Database**: Implemented.
- **Candidate Navigation (Arrow Keys / Tab)**: Implemented. Verified in Chrome, Telegram, and Kate.
- **Backspace Revert**: Implemented. Chrome phrase undo uses the ST-026 Backspace-forward path and awaits one focused live confirmation; Telegram and historical Kate checks pass.
- **Case & Layout Preservation**: Manual Alt+Shift synchronization passes. Mid-word proactive switch is deferred (logical translate first, physical IM after commit) per ST-041.
- **Smart Punctuation & Autocommit**: Implemented and verified.

### Rendering Addons
- **Internal `smarttypeui` Addon (`smarttypeui.so`)**: Built with native Wayland and native X11 renderers. Wayland is live-verified on Fedora and Ubuntu KDE; X11/XIM is live-verified on Kali Xfce, including candidate selection and cold-start persistence.
- **Client-Side Toolkit Fallback**: Toolkit client-side fallback path is supported but designated as NOT TESTED under v0.1 scope.
- **External `smarttype-ui`**: Legacy process. Fully disabled and does not spawn by default.

---

## 3. Environment & Configuration Audit Results
- **Active Environment**: Fedora KDE Plasma Wayland, Fcitx 5.
- **User `environment.d`**:
  - `90-smarttype.conf` / `fcitx5-smarttype.conf`: `FCITX_ADDON_DIRS=...`
  - `91-smarttype-im-wayland.conf` (ST-018): `XMODIFIERS=@im=fcitx`, deliberately **no** `QT_IM_MODULE`.
- **Session IM durability (ST-028, 2026-07-10, after reboot 09:30)**:
  - `systemctl --user show-environment`: `QT_IM_MODULE` **unset**; `XMODIFIERS=@im=fcitx5`; `IMSETTINGS_MODULE=fcitx5`.
  - User autostart: `imsettings-start.desktop` with `Hidden=true`.
  - Telegram + AyuGram process env: no `QT_IM_MODULE=xim`; Fcitx ICs `cap:8000d0072` (Preedit + SurroundingText) without clean-launch hack.
  - `~/.xinputrc` currently **missing** (optional hardening only; doctor WARNs).
- **Historical (2026-07-09, pre-ST-018)**:
  - Session had `QT_IM_MODULE=xim` + `XMODIFIERS=@im=none` from imsettings **none**.
  - Clean launch `env -u QT_IM_MODULE -u XMODIFIERS <Telegram>` restored rich caps — proof of env root cause, not engine.
- **FCITX_ADDON_DIRS**: `$HOME/.local/lib64/fcitx5:/usr/lib64/fcitx5` (verified on Fedora).
- **Diagnostics**: `smarttypectl status`; Fcitx `Controller1.DebugInfo` for IC focus/caps; `./scripts/doctor.sh` (includes ST-028 IM checks); SQLite `settings.current_app` updates only on SmartType `keyEvent`.
- **Installed build (2026-07-13, `9679635`, built `2026-07-13T07:05:37Z`)**: CTest 8/8 PASS. Fcitx restarted to PID 60632 and maps the current installed `smarttype.so` (SHA-256 `946206befdf3058c18ee803a6ea4e4dac4e7ec73354ccd676fb2d99cac3450b2`) plus `smarttypeui.so`; tray is owned by enabled `smarttype-tray.service` at PID 60683. `doctor.sh` has no FAIL (only optional `~/.xinputrc` WARN).
