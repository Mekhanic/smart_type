# SmartType Manual Test Suite (for independent tester / AI agent)

**Purpose**: After installing a new `smarttype.so` and reloading Fcitx (`fcitx5 -r`), run these scenarios and record PASS/FAIL with evidence.

**Environment (record once at start)**

```text
OS / DE: (e.g. Fedora KDE Plasma Wayland)
fcitx5-remote -n: 
systemctl --user is-active fcitx5-layout-sync.service:
busctl --user call org.kde.keyboard /Layouts org.kde.KeyboardLayouts getLayout:
echo QT_IM_MODULE=$QT_IM_MODULE XMODIFIERS=$XMODIFIERS
smarttype settings: layout_mode= auto|suggest|… ; layout_correction=on/off
Apps under test: Kate, Chrome, Telegram/AyuGram (each in empty field unless noted)
```

**How to report a FAIL**

```text
ID: MT-xxx
App: 
Keys / steps: 
Expected: 
Actual: 
IM after (fcitx5-remote -n): 
Notes / screenshot optional
```

Reload Fcitx before the suite: `fcitx5 -r`

---

## A. Baseline typing (P0)

| ID | App | Steps | Expected |
|----|-----|--------|----------|
| MT-001 | all | Type `привет` + Space | `привет ` no dup/lost chars |
| MT-002 | all | Type `hello` on EN + Space | `hello ` |
| MT-003 | all | Type URL `https://example.com/x` + Space | unchanged (no “correction”) |
| MT-004 | all | Type `happ.info` + Space | unchanged (ST-024 domain protect) |
| MT-005 | all | Type email-like `a@b.c` + Space | unchanged |

---

## B. Autocorrect & suggestions

| ID | App | Steps | Expected |
|----|-----|--------|----------|
| MT-010 | all | `севодня` + Space | → `сегодня ` (or clear top suggestion) |
| MT-011 | all | After MT-010, **immediate** Backspace | reverts to `севодня` (preedit or text) |
| MT-012 | all | `севодня` + Space, wait >300ms, Backspace | reverts correction (TC-006 style) |
| MT-013 | all | Typo with candidates (e.g. `вопщем`), Arrow Down, Tab | commits selected candidate |
| MT-014 | all | `автоплатежей` + Space | must **not** become `авто платежей` (ST-023; suggest OK) |
| MT-015 | all | `СЕВОДНЯ` + Space | case preserved → `СЕГОДНЯ ` |

### X11 inline-preedit acceptance

Run these only after `install-user.sh --enable-x11-layout-sync` and a fresh
login. Test Mousepad, Firefox and Kate.

| ID | App | Steps | Expected |
|----|-----|--------|----------|
| MT-X11-01 | all three | Type `вопщем` without Space | composition is visibly inside the application field; popup contains candidates only |
| MT-X11-02 | all three | Click `в общем` | exactly `в общем `, no duplicated source or fragments |
| MT-X11-03 | all three | Immediate Backspace after MT-X11-02 | restores `вопщем ` |
| MT-X11-04 | all three | Inspect Fcitx DebugInfo while focused | `frontend:dbus`, `focus:1`; raw `frontend:xim` is not accepted for this check |
| MT-016 | all | Open candidates, then immediately click another text position | panel closes and never follows the mouse/caret; typing remains intact |
| MT-017 | all | Put the caret before a visible character, press Delete | removes the character to the right; no square/control glyph |
| MT-018 | all | Open candidates, press Down, then Left/Right/Delete | panel closes; literal word remains; editing keys work immediately without a mouse click |

---

## C. Layout correction EN → RU

Start each case on **SmartType English** (`fcitx5-remote -n` → `smarttype-us`).

| ID | App | Steps | Expected |
|----|-----|--------|----------|
| MT-020 | all | `ghbdtn` + Space | → `привет `; IM → `smarttype` |
| MT-021 | all | `ghjtrn` + Space (`layout_mode=auto`) | → `проект `; IM → RU |
| MT-022 | all | Type `ghb` mid-word (no space) | preedit becomes `при` (or similar); IM → RU |
| MT-023 | all | After MT-022, immediate Backspace | restores Latin snapshot (`ghb`/`Ghb`); IM → EN |
| MT-024 | all | After MT-022, type more letters then Backspace | deletes one char; IM stays RU |
| MT-025 | all | `F ns xnj` + Space | full phrase **`А ты что `** (not `f ns что`) |
| MT-026 | all | `gh` + Space | stays `gh ` (EN→RU 2-letter no auto) |
| MT-027 | all | `rfr` + Space (no Russian context) | stays `rfr ` or suggest only (3-letter EN→RU safety) |
| MT-028 | all | `привет` then `rfr` + Space | `rfr` → `как ` (3-letter with cyr context) |

---

## D. Layout correction RU → EN (critical)

Start each case on **SmartType Russian** (`smarttype`).

| ID | App | Steps | Expected |
|----|-----|--------|----------|
| MT-030 | all | `руддщ` + Space | → `hello `; IM → `smarttype-us` |
| MT-031 | all | `ш` + Space + `ерштл` + Space | **`i think `** (not `ш think`, not `ш thinл`) |
| MT-032 | all | Type `ершт` then next key that would be `л` on RU | must end as **`think`**, never **`thinл`** |
| MT-033 | all | `вщ` + Space | → **`do `**; IM → EN |
| MT-034 | all | `вщ` + Space + type `you` (or continue keys for `нщг`) + Space | **`do you `** (not `вщ you`) |
| MT-035 | all | `нщг` + Space alone | → `you `; IM → EN |
| MT-036 | all | Invent 5 more RU-layout “English” phrases (agent’s choice), e.g. wrong-layout `руддщ цщкд` → `hello world` style | full phrase or progressive word fixes; no mixed-script leftovers |
| MT-037 | all | Mix: intentional Russian `да` then wrong-layout English | must not rewrite real Russian particles incorrectly |

---

## E. Manual layout switch (Alt+Shift / tray)

| ID | App | Steps | Expected |
|----|-----|--------|----------|
| MT-040 | Kate, Chrome, Telegram | Alt+Shift several times | **both** KDE indicator and typed language change; `fcitx5-remote -n` flips `smarttype` ↔ `smarttype-us` |
| MT-041 | all | Switch IM via tray/Fcitx UI to RU, type, then Alt+Shift to EN | typing follows; no “indicator only” stuck state |
| MT-042 | all | After auto layout switch to RU, Alt+Shift back to EN | works (layout-sync alive) |
| MT-043 | Warp or blacklisted app | Alt+Shift | still works (baseline without ST) |

---

## F. Phrase / SurroundingText edge cases

| ID | App | Steps | Expected |
|----|-----|--------|----------|
| MT-050 | all | Wrong-layout phrase of 3–4 words + Space on last | all wrong-layout words fixed when SurroundingText available |
| MT-051 | all | Type correct Russian words, then EN gibberish on RU layout | only gibberish run fixed; prior good Russian stays |
| MT-052 | all | Cursor middle of document, type wrong-layout word | does not corrupt earlier paragraph |
| MT-053 | all | Words with comma: invent `x, y` wrong-layout | document stability; note if phrase rewrite skips |
| MT-054 | Telegram | Long chat, multitline; MT-031/034 | same as empty field (SurroundingText stress) |

---

## G. Session / app environment

| ID | App | Steps | Expected |
|----|-----|--------|----------|
| MT-060 | Telegram | After reboot or new login, open Telegram, type `севодня` | corrections work; not “dead IM” |
| MT-061 | Telegram | `busctl` DebugInfo or diagnostics: focus + Preedit caps | Preedit present when focused |
| MT-062 | all | Tray pause for current app | no corrections in that app; other apps OK |
| MT-063 | all | Tray disable SmartType | plain Fcitx/layout only |

---

## H. Stress / chaos (agent invents variations)

The following 15 additional scenarios have been added to stress-test the autocorrector under edge-case and chaotic conditions.

| ID | App | Steps | Expected |
|----|-----|--------|----------|
| MT-071 | all | Type `ghbdtn rfr ltkf` + Space extremely fast (no key delays) | `привет как дела ` without lost/duplicate chars |
| MT-072 | all | Type alternating EN/RU words: `привет` + Space, `hello` + Space, `мир` + Space, `world` + Space, `как` + Space, `you` + Space, `дела` + Space, `are` + Space, `хорошо` + Space, `fine` + Space | `привет hello мир world как you дела are хорошо fine ` |
| MT-073 | all | CapsLock on, type `GHBDTN` + Space on EN layout | `ПРИВЕТ ` |
| MT-074 | all | Type `ghj` (triggers proactive RU `про`), then immediate Backspace | restores pre-switch snapshot `ghj` |
| MT-075 | all | Type `F ns xnj` + Space (triggers phrase RU `А ты что `), then immediate Backspace | restores whole Latin phrase `F ns xnj ` |
| MT-076 | all | Type `ghbdtn, 123 rfr?` + Space on EN | `привет, 123 как? ` (numbers/punctuation preserved) |
| MT-077 | all | Copy `тест`, paste it, then type `ghbdtn` + Space | `тестпривет ` or `тест привет ` |
| MT-078 | all | Type `ghb` in Kate, immediately switch focus to Chrome, type `dtn` + Space | No cross-application buffer corruption or crash |
| MT-079 | all | Run `smarttypectl set layout_mode suggest`, type `ghbdtn` + Space | Stays `ghbdtn ` but candidate panel shows `привет` |
| MT-080 | all | Type `z` + Space, `ns` + Space, `ghjtrn` + Space, `fcitx5-layout-sync` + Space | `z ` and `ns ` unchanged, `ghjtrn` -> `проект `, `fcitx5-layout-sync` stays |
| MT-081 | all | Type mixed-script homoglyph word (e.g. English `c` + Cyrillic `обака`) | No bad corruption or loop |
| MT-082 | all | Type `CMakeLists.txt`, `git status`, or `foo.bar.baz()` + Space | Left completely unchanged |
| MT-083 | all | Restart service: `systemctl --user restart fcitx5-layout-sync.service`, Alt+Shift twice | System layout syncs successfully with Fcitx IM |
| MT-084 | all | Type false friend word `tom` + Space on EN layout | Stays `tom ` (valid English word) |
| MT-085 | all | Type `вопщем` + Space (auto-corrects), press Backspace. Repeat 3 times. | On 4th time, stays `вопщем ` (demoted/learned) |

---

## I. Automated regression (run by agent if build available)

```bash
cd <repo>/build
ctest --output-on-failure -R 'core|fcitx'
./scripts/doctor.sh
```

Expect: core + fcitx-integration PASS; doctor OK including layout-sync.

---

## J. Pass criteria (suite)

- **P0 block**: any of MT-001, MT-020, MT-025, MT-031, MT-033, MT-034, MT-040 failing in Kate **or** Telegram
- **P1**: MT-014, MT-004, MT-022/023, MT-060
- Deliverable: table of all IDs with PASS/FAIL + FAIL details; list of invented H-scenarios with results

---

## K. Known intentional behaviours (do not mark FAIL)

- EN→RU length-2 (`gh`) does not auto-replace on Space (suggest only)
- EN→RU length-3 without Russian context may not auto on Space
- Visual panel animation differences across apps are out of scope
- Warp may not run SmartType the same way as Kate/Chrome/Telegram
