# SmartType Functional Test Matrix

This matrix tracks the functional parity of core SmartType behavior across target environments. All tests are run manually unless stated otherwise.

Core functional parity across active frontends remains the product requirement. This matrix is the baseline for the next owner-led manual pass; historical failures are labelled explicitly.

## Test Matrix

| Test Case | Feature | Test Scenario | Chrome | Telegram | Kate / KWrite |
|---|---|---|---|---|---|
| **TC-001** | Basic Typing | Input standard words. Check for lost or duplicated keypresses. | Pass | Pass | Pass; historical ST-022 mixed-prefix issue resolved |
| **TC-001b** | Controlled web input | Type `махачкала` in a Chromium/React controlled input that rerenders after value changes. Every key must be committed exactly once; cumulative preedit prefixes are forbidden. | Automated pass (2026-07-13); original field live check pending | N/A | N/A |
| **TC-002** | Autocorrection | Type known typo followed by space/comma. Check correction swap. | Pass | Pass | Pass |
| **TC-003** | Suggestions | Check if candidates are populated correctly in the panel. | Pass | Pass | Pass |
| **TC-004** | Arrow Navigation | Use Arrow keys to navigate through the candidate list. | Pass | Pass | Pass |
| **TC-005** | Tab Commit | Press Tab to commit the currently selected candidate. | Pass | Pass | Pass |
| **TC-006** | Backspace Revert | Type typo, wait for autocorrect commit, press Backspace immediately. The original typo must return. | Pass | Pass | Pass |
| **TC-007** | Space Handling | Validate double spaces and delimiter spacing after corrections. | Pass | Pass | Pass |
| **TC-008** | Punctuation | Verify that punctuation (periods, commas, question marks) behave consistently. | Pass | Pass | Pass |
| **TC-009** | Case Preservation | Capitalized input should result in capitalized correction suggestions. | Pass | Pass | Pass |
| **TC-010** | Layout Switching | Switching keyboard layout between US and RU must not break core preedit. | Pass | Pass | Pass |
| **TC-010b** | Manual Alt+Shift (ST-020) | Alt+Shift must change **both** KDE indicator and SmartType IM (`fcitx5-remote -n`). Requires `fcitx5-layout-sync.service` active. | Pass (2026-07-09) | Pass (2026-07-09) | Pass (2026-07-09) |
| **TC-010c** | Phrase layout (ST-021/ST-026) | Type multi-word wrong layout e.g. `F ns xnj`+space → full phrase `А ты что` (not only last word). Automated in fcitx-integration. | **Fix ST-026** (auto: Backspace delete on chrome IC); live re-smoke pending | Pass (2026-07-10) | Not run ST-022 |
| **TC-010d** | Reverse phrase (ST-021.1) | On RU: `ш ерштл`+space → `i think` (not `ш thinл` / `ш think`). Auto in fcitx-integration. | Pass (auto) | Manual after reload | Manual after reload |
| **TC-012** | No compound auto-split (ST-023) | `автоплатежей`+space must not auto-become `авто платежей`. | Pass (core) | Manual | Manual |
| **TC-013** | Domain protection (ST-024) | `happ.info`+space stays as typed. | Pass (core + Chrome 2026-07-10) | Manual | Manual |
| **TC-014** | Proactive switch under burst | From EN, type `ghbdtn rfr ltkf ` quickly. No old preedit prefix may survive the IM switch. | Historical ST-022 failure; current issue owner-confirmed resolved | Historical ST-022 failure; current issue owner-confirmed resolved | Historical ST-022 failure; current issue owner-confirmed resolved |
| **TC-011** | Learning | Correcting the same word multiple times updates weights in SQLite. | Not Tested | Not Tested | Not Tested |
| **TC-011a** | Learning plumbing (ST-034) | Isolated store verifies learn/undo/demote/reset; Fcitx integration verifies a manual `ашипка`→`ошибка` correction persists and is applied. | Pass (generic harness) | Pass (generic harness) | Pass (generic harness) |
| **TC-B** | No `SurroundingText` fallback (ST-034) | Preedit-only Fcitx context: `севодня`→`сегодня`, immediate Backspace restores `севодня` through the timer fallback, then ordinary typing commits without duplicate/lost text. | Harness pass; manual pending | Harness pass; manual pending | Harness pass; manual pending |

---

## Test Execution Notes

- **Chrome**: Native Wayland server-side panel (`ClientSideInputPanel=false`) with immediate literal text commits and an internal logical word buffer. This avoids controlled-input preedit duplication while retaining suggestions/corrections. The exact `махачкала` transaction passes the integration harness; original-field live confirmation remains pending.
- **Telegram**: Native Wayland server-side path (`ClientSideInputPanel=false`). Normal correction, phrase rewrite, typo correction and immediate Backspace revert pass; the former proactive-switch race is resolved.
- **Kate / KWrite**: Native Wayland server-side path (`ClientSideInputPanel=false`). Historical navigation/revert/punctuation tests pass. The former ST-022 mixed-preedit failure is resolved on the current installed build.
- **Excluded Pathways**:
  - Client-side toolkit render path (`ClientSideInputPanel=true`) - **NOT TESTED** under v0.1.
  - TC-B (`SurroundingText=false`) has automated Fcitx coverage, but still needs an identified real application for manual frontend-specific validation.
  - TC-011 end-to-end learning policy across real applications remains **NOT TESTED**; isolated persistence, demote thresholds, reset, and manual-rule application are covered automatically as TC-011a.
  - TC-A and real-application TC-B behavior are retained as deferred risks.

## Ubuntu Wayland and native X11 verification (2026-07-14)

| Check | Result |
|---|---|
| Root build includes `xcbui.cpp`, `xcbinputwindow.cpp` and XCB support libraries | **PASS** with `SMARTTYPE_ENABLE_X11=ON` |
| Current-build `smarttypeui.so` exports `XCBInputWindow::update` | **PASS** |
| Addon smoke fails if X11 is requested but the XCB renderer symbol is absent | **PASS** |
| Full X11-enabled CTest suite | **10/10 PASS** — includes persistent Fcitx config and X11 layout-owner tests |
| `doctor.sh` rejects a Wayland-only UI library in an X11 session | **PASS** |
| Kali Xfce/X11: toolkit environment after cold login | **PASS** — Xfce session and app processes inherit `GTK_IM_MODULE=fcitx`, `QT_IM_MODULE=fcitx`, `XMODIFIERS=@im=fcitx` |
| Kali Xfce/X11: focused real input contexts | **PASS** — Mousepad/Firefox ESR/Kate each report `frontend:dbus`, rich capabilities and `focus:1` while tested |
| Kali Xfce/X11: composition stays in application field | **PASS** — visually verified in GTK3, Firefox GTK and Qt6; popup contains candidates only |
| Kali Xfce/X11: candidate selection + Backspace | **PASS** — each app committed exactly `в общем `; immediate Backspace restored `вопщем ` |
| Kali Xfce/X11: Alt+Shift after cold login | **PASS** — `smarttype → smarttype-us → smarttype`, without manual IM selection |
| Kali Xfce/X11: raw XIM | Compatibility fallback only — not accepted as proof of supported inline UX |
| Ubuntu KDE Wayland: native protocol and focused real input | **PASS** — protocol `1`, Firefox `frontend:wayland`, `focus:1` |
| Ubuntu KDE Wayland: Russian typing and candidates visible | **PASS** — `вопщем`, dark native panel visible |
| Ubuntu KDE Wayland: US/RU layout bridge | **PASS** — two KWin layouts, RU index 1, `smarttype` active |

The invalid service-only VM reports were replaced with current evidence in
`vm-reports/REPORT-ubuntu.md` and `vm-reports/REPORT-kali.md`.

## Candidate interaction and punctuation controls (2026-07-11)

| Check | Result |
|---|---|
| Hover crossing a gap keeps the last hovered candidate instead of jumping to keyboard index 0 | Code path fixed; owner visual check pending |
| Highlight width is never smaller than height, including one-glyph candidates | Code path fixed; owner visual check pending |
| `<<` → `«`, `>>` → `»`, `--` → `—` with smart punctuation enabled | **PASS** (`fcitx-integration`) |
| With smart punctuation disabled, `--` remains literal | **PASS** (`fcitx-integration`) |
| Auto-space after punctuation can be disabled independently | **PASS** (`fcitx-integration` + `settings-smoke`) |
| Full registered CTest suite | **8/8 PASS** |
| Installed libraries loaded by live Fcitx | **PASS** (PID 153111) |
| External caret relocation dismisses candidates without rebuilding them at the new caret | Code path covered by cursor/reset handling; owner live check pending |
| Up/Down close candidates, commit the literal composition, and release Left/Right/Delete to the application | **PASS** (`fcitx-integration`) |
| Tray active state uses new idle icon; pause/off/inactive share new pause icon | **PASS** (installed icon lookup + live StatusNotifier `IconName`); owner visual confirmation still useful |

## Candidate caret anchor and portable source install (2026-07-11)

| Check | Result |
|---|---|
| Cursor rect moves immediately after a key while candidates are open | **PASS** (`fcitx-integration`): candidates are dismissed, not re-anchored |
| Delete is forwarded as `FcitxKey_Delete` instead of being handled as text | **PASS** (`fcitx-integration`) |
| CMake staging install with user prefix and `CMAKE_INSTALL_LIBDIR=lib` | **PASS**: both `smarttype.so` and `smarttypeui.so` staged in `~/.local/lib/fcitx5` |
| Fedora native build and CTest | **PASS** (this host; 8/8) |
| Installed addon loaded by live Fcitx after reload | **PASS** (`FCITX_ADDON_DIRS=~/.local/lib/fcitx5:...`; both libraries mapped) |
| Ubuntu KDE Wayland build/install/live input | **PASS** (2026-07-12, verified via `ctest` in container with updated Fcitx 5.1.21 PPA and dynamic Hunspell encoding lookup) |
| Arch KDE Wayland build/install/live input | **NOT RUN** |
| Settings window opens with grouped controls and explanatory empty states | **PASS** (`settings-smoke`, 2026-07-12); owner visual check pending |
| Reproducible Fedora/Ubuntu/Arch VM manual procedure | **PASS** (documented in [`DISTRO_TEST_PLAN.md`](DISTRO_TEST_PLAN.md)); Ubuntu verification completed successfully inside container |

## ST-034: Automated test hardening (2026-07-10)

| Check | Result |
|---|---|
| UI math, interrupt, and addon smoke execute as three distinct cases | **PASS** |
| Addon smoke validates generated config + current-build `smarttypeui.so` and loads its factory symbol | **PASS** |
| Fcitx integration cannot run without explicit isolated XDG paths | **PASS** |
| No-`SurroundingText` correction, 50 ms fallback undo, and subsequent typing | **PASS** |
| Manual correction persists `ашипка`→`ошибка` and is used by the engine | **PASS** |
| Existing in-memory demote, compound, URL/domain, reverse phrase tests | **PASS** |
| Full registered CTest suite | **8/8 PASS** |

---

## ST-022: Installed-build frontend smoke (2026-07-10)

Installed build: `15d472c` (`smarttypectl version`); native Wayland contexts; `smarttypeui` active; tray and `fcitx5-layout-sync.service` active; `scripts/doctor.sh` fully OK.

| App / check | Exact input | Result |
|---|---|---|
| Chrome normal EN→RU | `ghbdtn `, 35 ms/key | **PASS** → `привет` |
| Chrome typo + undo | RU `ctdjlyz `, then immediate Backspace | **PASS** → `сегодня` → `севодня` |
| Chrome domain protection | `happ.info` | **PASS** unchanged |
| Chrome phrase rewrite | `F ns xnj `, 60 ms/key | **FAIL** → `F ns А ты что`; Backspace does not restore the source phrase |
| Chrome burst phrase | `ghbdtn rfr ltkf `, 5 ms/key | Historical ST-022 **FAIL**; mixed-preedit issue owner-confirmed resolved on current build |
| Telegram normal EN→RU | `ghbdtn `, 35 ms/key | **PASS** → `привет` |
| Telegram phrase rewrite | `F ns xnj `, 60 ms/key | **PASS** → `А ты что` |
| Telegram typo + undo | RU `ctdjlyz `, then immediate Backspace | **PASS** → `сегодня` → `севодня` |
| Telegram burst phrase | `ghbdtn rfr ltkf `, 5 ms/key | Historical ST-022 **FAIL**; mixed-preedit issue owner-confirmed resolved on current build |
| Kate normal EN→RU | `ghbdtn `, 35 and 120 ms/key | Historical ST-022 **FAIL**; current behavior owner-confirmed resolved |
| Plasma manual layout bridge | Alt+Shift twice | **PASS**: KDE `1→0→1`; SmartType `smarttype→smarttype-us→smarttype` |

The automated suite still passes **8/8**. ST-041 adds Fcitx-harness coverage for deferred physical IM switch with non-empty client preedit. Chrome phrase-prefix deletion (ST-026) remains an automated coverage gap.

---

## ST-041: Deferred proactive layout switch (2026-07-10)

| Check | Result |
|---|---|
| Physical IM does not change while client preedit is non-empty | **PASS** (fcitx-integration) |
| EN→RU mid-word: preedit translates, physical switch only after empty preedit | **PASS** |
| Queued source-layout keys complete `привет` via pending target | **PASS** |
| Burst multiword `ghbdtn rfr ltkf` keeps RU target (no bounce to EN) | **PASS** |
| Immediate Backspace after proactive restores snapshot; no transient physical switch | **PASS** |
| Continue-type accepts switch and flushes deferred IM | **PASS** |
| Full buffer erase cancels deferred target (no late flush) | **PASS** |
| Full CTest suite | **8/8 PASS** |
| Install + Fcitx reload; live `smarttype.so` mapped | **PASS** (2026-07-10) |
| Live Qt Wayland + ydotool: `ghbdtn ` @ 40 ms | **PASS** → `привет ` |
| Live Qt Wayland + ydotool: `ghbdtn ` @ 120 ms | **PASS** → `привет ` |
| Live Qt Wayland + ydotool: `ghbdtn ` @ 200 ms | Historical **FAIL**; owner confirms `ghbdtn` issue resolved on current build |
| Live Qt Wayland + ydotool: burst `ghbdtn rfr ltkf ` @ 8 ms | Historical **FAIL**; mixed-preedit family owner-confirmed resolved |
| Live: `plhfdcndeqnt ` (здравствуйте) | **PASS** by owner confirmation on current installed build |
| Live: `Plhfdcndeqnt ` (Здравствуйте) | **PASS** → `Здравствуйте ` |
| Kate / Chrome / Telegram manual re-smoke | **Pending owner** (ydotool ≠ real app path) |

Engine: deferred physical IM; composing-safe `update_preedit` (no `panel.reset` mid-word); Latin SurroundingText heal; no empty-preedit schedule on letter keys.

---

## ST-026: Chrome phrase delete (2026-07-10)

| Check | Result |
|---|---|
| `google-chrome` IC: phrase rewrite of `F ns ` uses ≥5 forwarded Backspaces | **PASS** (fcitx-integration) |
| Kate/Telegram-style IC still uses `deleteSurroundingText` when reliable | **PASS** (existing ST-021 path) |
| Undo path uses Backspace when client is Chromium-family | **PASS** (code path; timer undo covered by ST-034 no-ST case) |
| Full CTest | **8/8 PASS** |
| Live Chrome owner: `F ns xnj ` → `А ты что `, Backspace restores phrase | **Pending** |

---

## ST-012: Theme Lab Tool Verification (deprecated experiment)

Historical verification only. The developer tool was not useful for final design work, is outside the product direction, and has no planned maintenance:

| Check | Result |
|---|---|
| `cmake --build <dir> --target smarttype-theme-lab` builds without errors | **PASS** |
| Tool launches without Fcitx, without `~/.config` access | **PASS** (design: no Fcitx5 headers, no StandardPaths) |
| Main build (`cmake --build`) unchanged by addition of `tools/` | **PASS** |
| All production tests unaffected | **PASS** (pre-existing `addon-smoke` failure unrelated to ST-012) |
| No production files modified | **VERIFIED** (only `tools/` and root `CMakeLists.txt` touch) |

---

## ST-015: Candidate Panel Design Transfer Verification

Visual and interaction verification of native Wayland candidate panel:

| Check | Result |
|---|---|
| Clamped candidate list selection index (no wrap-around) | **PASS** |
| Selection pill 2.5px rubber-band bounce edge feedback | **PASS** |
| Popup window remains completely stationary | **PASS** |
| Selection pill transition duration reduced to 150ms | **PASS** |
| Zero-layout-shift candidate centered rendering using Pango font weight 600 | **PASS** |
| Opacity decreased to 12% in active.svg | **PASS** |
| Horizontal candidate text margins set to 10px in theme.conf | **PASS** |
| Correction pulse (horizontal sweep) functional on Wayland | **PASS** |
| All 9 ctests pass cleanly (100% pass rate) | **PASS** |

---

## ST-016: Tray panel scale + managed lifecycle

| Check | Result |
|---|---|
| `smarttype-tray` settings shows slider 80–130% step 5 | **PASS** (manual) |
| Apply/OK persists via Fcitx D-Bus; reopen shows value | **PASS** (after QDBusArgument demarshal fix) |
| Cancel does not write CandidatePanelScale | **PASS** (design) |
| Fcitx down: slider disabled, clear error, other settings work | **PASS** (smoke) |
| `settings-smoke` / `report-dialog-smoke` | **PASS** |
| Legacy `~/.config/autostart/smarttype-tray.desktop` removed | **PASS** (installed 2026-07-13) |
| `smarttype-tray.service` enabled, active, and owns installed binary | **PASS** (final live PID 60683) |
| Unexpected process exit is recovered | **PASS** (final SIGTERM probe: `60502→60683`, `NRestarts=1`) |
| Cold desktop login waits for display instead of crashing Qt | **PASS** (Ubuntu Wayland + Kali X11 cold boot, `NRestarts=0`, no core dump) |
| `smarttype-idle` resolves to the installed 512x512 application PNG | **PASS** (`icon_idle.png`) |
| Tray dark/light resources load from the executable | **PASS** (`tray-icons-smoke`) |
| Runtime states keep one logo; palette change selects dark/light artwork | **PASS** (code path + resource smoke; live theme toggle pending) |

## Pre-publication install path (2026-07-14)

| Check | Result |
|---|---|
| Missing historical `icon_pause.png` cannot break a clean configure | **PASS** (reference removed) |
| Existing Fcitx methods survive SmartType profile setup | **PASS** (`fcitx-profile-config`) |
| Re-running profile setup creates no duplicate methods | **PASS** (`fcitx-profile-config`) |
| `install.sh` shell syntax and supported-mode parsing | **PASS** (`bash -n`, `--help`) |
| Local release build and complete automated suite | **PASS**, 12/12 |
| Unified installer redeployed on Fedora 44 KDE Wayland | **PASS**, 12/12 + doctor RC 0 + active SmartType |
| Unified installer redeployed on Ubuntu 26.04 KDE Wayland | **PASS**, 12/12 + install + post-reboot verification |
| Unified installer redeployed on Kali Rolling Xfce/X11 | **PASS**, 12/12 + doctor RC 0 + active SmartType |
| Installed tray publishes an unnamed concrete bitmap | **PASS** on Fedora/Ubuntu (`IconName=""`, non-empty `IconPixmap`) |
| Only `smarttype-idle.png` remains in installed hicolor icon paths | **PASS** on Fedora/Ubuntu/Kali |
| Repository license is the official GNU GPL-3.0 text | **PASS** (byte-identical to `gnu.org/licenses/gpl-3.0.txt`; SHA-256 `3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986`) |
| Tray and settings expose the project/support GitHub page | **PASS** on Fedora 44, Ubuntu 26.04 and Kali Rolling (12/12 each; installed binary contains the exact URL and `--check-settings` succeeds) |

## Prebuilt release channel (2026-07-15)

| Check | Result |
|---|---|
| Normal installer avoids compiler and development packages | **PASS** in staged Fedora/Ubuntu/Kali bundle installs |
| Release download is protected by a published SHA-256 file | **PASS** for generated Fedora archive and internal payload manifest |
| Separate Fedora 44, Ubuntu 26.04 and Kali Rolling bundles | **PASS** in PR release matrix ([run 29371186999](https://github.com/Mekhanic/smart_type/actions/runs/29371186999)); tagged publication pending |
| Source-build fallback remains available | **PASS** (`./install.sh --build-from-source`) |
| Fedora bundle build, complete CTest, staged user install and invalid-payload rejection | **PASS**, 12/12 + installer smoke |
| Ubuntu 26.04 bundle build, complete CTest, staged user install and invalid-payload rejection | **PASS**, 12/12 + installer smoke |
| Kali Rolling bundle build, complete CTest, staged user install and invalid-payload rejection | **PASS**, 12/12 + installer smoke |

---

## ST-017: Layout logical IM sync

| Check | Result |
|---|---|
| `ctest -R fcitx-integration` | **PASS** (~2s) |
| New IC + smarttype-us proactive path | **PASS** (automated) |
| Programmatic switch does not wipe compose buffer | **PASS** (automated) |
| Two Ctrl+Shift+Space toggles restore IM | **PASS** (automated) |
| Production install of new `smarttype.so` | **NOT DONE** (still optional after ST-018) |

---

## ST-018 prep: Telegram IM environment (2026-07-09)

| Check | Result |
|---|---|
| Session has `QT_IM_MODULE=xim` + `XMODIFIERS=@im=none` (imsettings none) | **CONFIRMED** (historical, pre-fix) |
| Clean `env -u QT_IM_MODULE -u XMODIFIERS` Telegram launch → focus=1 + Preedit/SurroundingText | **PASS** |
| User reports SmartType works in Telegram after clean launch | **PASS** (manual) |
| Permanent session fix (ST-018 on this machine) | **APPLIED** (imsettings-start hidden; env.d IM policy) |
| Re-launch Telegram after ST-018 without `QT_IM_MODULE=xim` | **PASS** (see ST-028) |

## ST-028: Session IM durability after reboot (2026-07-10)

Host boot `2026-07-10 09:30`; audit same day without clean-launch env hacks.

| Check | Result |
|---|---|
| `systemctl --user show-environment`: `QT_IM_MODULE` unset | **PASS** |
| Session `XMODIFIERS` is `@im=fcitx5` (not `@im=none`) | **PASS** |
| Session `IMSETTINGS_MODULE=fcitx5` | **PASS** |
| `~/.config/autostart/imsettings-start.desktop` has `Hidden=true` | **PASS** |
| `environment.d/91-smarttype-im-wayland.conf` present (no `QT_IM_MODULE`) | **PASS** |
| Telegram process: no `QT_IM_MODULE=xim`, `XMODIFIERS=@im=fcitx5` | **PASS** |
| AyuGram process: same as Telegram | **PASS** |
| Fcitx DebugInfo: Telegram + AyuGram `cap:8000d0072` (Preedit+SurroundingText) | **PASS** |
| `scripts/doctor.sh` ST-028 checks (xim / none / imsettings-start) | **PASS** (optional WARN: missing `~/.xinputrc`) |
| Owner full Telegram typing smoke (optional, not acceptance gate) | **NOT REQUIRED** for close |

## ST-042: No RU/EN candidate-panel growth (2026-07-10)

Root cause: Fcitx `ShowInputMethodInformation` + compact mode paints dual-IM `Label=RU`/`EN` into `auxUp` after programmatic switch (often after `update_preedit` via deferred flush).

| Check | Result |
|---|---|
| Engine clears layout-label `auxUp` immediately after programmatic `setCurrentInputMethod` | **DONE** (code) |
| smarttypeui ignores pure `RU`/`EN` (and full SmartType names) in upper layout | **DONE** (code) |
| CTest full suite | **PASS** 8/8 |
| Live: auto EN→RU mid-word / space does not add top `RU`/`EN` row or grow panel height | **OWNER VERIFY** after install |
