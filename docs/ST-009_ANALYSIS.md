# SmartType ST-009 Functional Analysis & Verification Plan

This document contains a detailed read-only analysis of the core SmartType behavior under different render paths, mapping test cases to engine code and automated tests, highlighting unconfirmed claims, and providing a step-by-step manual testing protocol.

---

### 1. Mapping Test Matrix to Code & Automated Tests

Here is how each test case from [TEST_MATRIX.md](./TEST_MATRIX.md) is implemented in the engine and verified by existing autotests in [fcitx_integration.cpp](../tests/fcitx_integration.cpp).

| Test Case | Feature | Engine Code Location | Autotest Coverage (Line range in `fcitx_integration.cpp`) |
|---|---|---|---|
| **TC-001** | Basic Typing | [smarttype_engine.cpp:L1537-1653](../src/fcitx/smarttype_engine.cpp#L1537-L1653) | [L293-304](../tests/fcitx_integration.cpp#L293-L304) (URL, Email, Path, IP validation) |
| **TC-002** | Autocorrection | [smarttype_engine.cpp:L766-885](../src/fcitx/smarttype_engine.cpp#L766-L885) (`finish_word`) | [L140-145](../tests/fcitx_integration.cpp#L140-L145) (Standard Cyrillic typo correction) |
| **TC-003** | Suggestions | [smarttype_engine.cpp:L620-631](../src/fcitx/smarttype_engine.cpp#L620-L631) (`update_preedit`) | [L316-320](../tests/fcitx_integration.cpp#L316-L320) (suggestions setting enabled/disabled checks) |
| **TC-004** | Arrow Navigation | [smarttype_engine.cpp:L1477-1496](../src/fcitx/smarttype_engine.cpp#L1477-L1496) | [L330-333](../tests/fcitx_integration.cpp#L330-L333) (navigation keys filter check) |
| **TC-005** | Tab Commit | [smarttype_engine.cpp:L1498-1512](../src/fcitx/smarttype_engine.cpp#L1498-L1512) | [L334-336](../tests/fcitx_integration.cpp#L334-L336) (tab key commit check) |
| **TC-006a**| Revert Before Delay | [smarttype_engine.cpp:L1378-1392](../src/fcitx/smarttype_engine.cpp#L1378-L1392) (`key_event`) | [L146-147](../tests/fcitx_integration.cpp#L146-L147) (Backspace revert during delayed preedit) |
| **TC-006b**| Revert After Delay | [smarttype_engine.cpp:L1105-1153](../src/fcitx/smarttype_engine.cpp#L1105-L1153) (`undo_last`) | [L151](../tests/fcitx_integration.cpp#L151) (revert Cyrillic autocorrection after commit) |
| **TC-007** | Space Handling | [smarttype_engine.cpp:L1709-1718](../src/fcitx/smarttype_engine.cpp#L1709-L1718) | [L165-167](../tests/fcitx_integration.cpp#L165-L167) (double spacing and delimiter replace) |
| **TC-008** | Punctuation | [smarttype_engine.cpp:L1655-1750](../src/fcitx/smarttype_engine.cpp#L1655-L1750) | [L193-216](../tests/fcitx_integration.cpp#L193-L216) (smart punctuation, attachments) |
| **TC-009** | Case Preservation | [smarttype_engine.cpp:L1574-1582](../src/fcitx/smarttype_engine.cpp#L1574-L1582) | [L246-253](../tests/fcitx_integration.cpp#L246-L253), [L267-276](../tests/fcitx_integration.cpp#L267-L276) (Mixed case / Caps normalization) |
| **TC-010** | Layout Switching | [smarttype_engine.cpp:L1333-1365](../src/fcitx/smarttype_engine.cpp#L1333-L1365), [L1587-1638](../src/fcitx/smarttype_engine.cpp#L1587-L1638) | [L369-558](../tests/fcitx_integration.cpp#L369-L558) (Proactive and manual layout translation) |
| **TC-011** | Learning | [smarttype_engine.cpp:L873-896](../src/fcitx/smarttype_engine.cpp#L873-896) (`learning`) | [L223-242](../tests/fcitx_integration.cpp#L223-L242) (Manual correction learning) |

---

## 2. Dynamic Status of Identified Issues

### Confirmed Bugs/Issues
* *None currently confirmed; pending deterministic reproduction.*

### Suspected Ordering Risks / High-Risk Hypotheses

#### TC-A: Delayed Preedit / Timer Interleaving (High-Risk Hypothesis)
* **Commit Hash**: `8d16bebaafcda8d0ffb1bee20e27d54164451a7c`
* **File & Function**: [smarttype_engine.cpp](../src/fcitx/smarttype_engine.cpp) $\rightarrow$ [SmartTypeState::finish_word](../src/fcitx/smarttype_engine.cpp#L766) and [SmartTypeState::key_event](../src/fcitx/smarttype_engine.cpp#L1268) / [SmartTypeState::commit_delayed](../src/fcitx/smarttype_engine.cpp#L1155).
* **Line Numbers**: [L964-1004](../src/fcitx/smarttype_engine.cpp#L964-L1004) (delayed commit logic) and [L1312-1324](../src/fcitx/smarttype_engine.cpp#L1312-L1324) (key_event delayed check).
* **Event Sequence**:
  1. User types Cyrillic typo `севодня` and hits `Space`.
  2. Engine identifies correction to `Сегодня`, clears `buffer_`, and schedules a 160ms timer: `loop.addTimeEvent(..., 160000, ...)`.
  3. During the 160ms window (tested at `0ms`, `20ms`, `50ms`, `100ms`, `170ms`), the user continues typing the next word starting with letter `п`.
  4. Key event `п` is processed in `key_event()`'s `is_word_character` block and appended to `buffer_`.
  5. The remaining characters of the word `ривет` are typed at a fixed interval of `50 ms` between characters.
  6. Fcitx preedit is updated to `Сегодня привет` (concatenated string).
  7. At 160ms, the timer expires, calling `commit_delayed(true)`.
  8. `commit_delayed` executes `input_context_->commitString("Сегодня ")`.
  9. The application receives the commit of `Сегодня `, which clears the preedit in the application.
  10. `commit_delayed` calls `update_preedit()`, which updates Fcitx preedit to `привет`.
* **State Transition**:
  - *Before step 1*: `buffer_=""`, `delayed_commit_` is empty.
  - *After step 2*: `buffer_=""`, `delayed_commit_` stores original `севодня` and replacement `Сегодня`, 160ms timer is active.
  - *After step 4*: `buffer_="п"`, `delayed_commit_` timer is active, Fcitx client preedit set to `Сегодня п`.
  - *After step 8*: `commitString("Сегодня ")` is called. Application document text receives `Сегодня `, application preedit is cleared.
  - *After step 10*: Fcitx client preedit set to `привет` (rest of characters `ривет` appended).
* **Why existing code is suspected to violate order**:
  We hypothesize that the frontend may queue or asynchronously process incoming messages. The application receives a commit string and preedit updates. If these are processed asynchronously or reordered by the frontend, there is a risk that the commit replaces or interleaves with the active preedit update (e.g. committing "Сегодня " might overwrite the preedit "привет", or characters are dropped/duplicated).
* **Closest existing autotest**: [fcitx_integration.cpp:L140-145](../tests/fcitx_integration.cpp#L140-L145) (standard typo correction, but without typing during the flash delay).
* **Proposed new test to reproduce**:
  A test inside `fcitx_integration.cpp` that schedules key events for a subsequent word starting with letter `п` at various delays (0ms, 20ms, 50ms, 100ms, 170ms) using the Fcitx event loop time events, pending audit of the existing test harness's capabilities for virtual/fake clock, checking that final committed text matches without duplication or loss.

#### TC-B: Replacement Fallback without SurroundingText (Suspected Ordering Risk)
* **Commit Hash**: `8d16bebaafcda8d0ffb1bee20e27d54164451a7c`
* **File & Function**: [smarttype_engine.cpp](../src/fcitx/smarttype_engine.cpp) $\rightarrow$ [SmartTypeState::erase_committed](../src/fcitx/smarttype_engine.cpp#L363) and [SmartTypeState::undo_last](../src/fcitx/smarttype_engine.cpp#L1105).
* **Line Numbers**: [L366-370](../src/fcitx/smarttype_engine.cpp#L366-L370) (Backspace loop fallback) and [L1139-1151](../src/fcitx/smarttype_engine.cpp#L1139-L1151) (undo fallback event loop).
* **Event Sequence**:
  1. User presses Backspace after correction.
  2. Since `SurroundingText` is false, `undo_last()` calls `forwardKey(FcitxKey_BackSpace)` `length` times.
  3. Engine schedules a 50ms time event: `undo_timer_ = loop.addTimeEvent(..., 50000, ...)`.
  4. User presses a key (e.g., `x`) at `10ms` (before 50ms expires).
  5. `key_event()` intercepts `x` and sees `undo_timer_` is active.
  6. `key_event()` calls `undo_timer_.reset()` and calls `commit_undo()` immediately.
  7. `commit_undo()` calls `input_context_->commitString(undo_->original)`.
* **State Transition**:
  - *Before step 1*: `undo_` has valid undo info, `buffer_=""`.
  - *After step 2*: `forwardKey` called `length` times, `undo_timer_` active.
  - *After step 5*: key event `x` is processed, `undo_timer_` is canceled, `commit_undo()` called immediately.
  - *After step 7*: `commitString` called.
* **Why existing code is suspected to violate order**:
  If the frontend queues forwarded Backspace events, an immediate commitString may be delivered before all deletions have been applied. The actual ordering must be measured by the deterministic test.
* **Closest existing autotest**: [fcitx_integration.cpp:L146-147](../tests/fcitx_integration.cpp#L146-L147) (Cyrillic Backspace revert, but with `SurroundingText` enabled and without fast typing).
* **Proposed new test to reproduce**:
  Create an integration test with a deterministic, non-GUI `InputContext` where `CapabilityFlag::SurroundingText` is disabled, utilizing a mock model of the final application text buffer to trace the actual committed string and deletions. Trigger correction, send Backspace to undo, immediately send a key (e.g., 5ms later), and verify character counts and sequence ordering.

### Not Tested / Fallback Scenarios
* Behavior of typing without `Preedit` capability.
* Behavior of typing in blacklisted applications when some settings are toggled.

---

## 3. Portability Issues (Non-Blockers)

### KDE-Specific Layout Switching (Portability Issue)
* **File & Function**: [smarttype_engine.cpp](../src/fcitx/smarttype_engine.cpp) $\rightarrow$ `check_and_switch_layout` and `key_event` layout toggles.
* **Line Numbers**: [L1348-1350](../src/fcitx/smarttype_engine.cpp#L1348-L1350) and [L1384-1386](../src/fcitx/smarttype_engine.cpp#L1384-L1386) / [L1408-1410](../src/fcitx/smarttype_engine.cpp#L1408-L1410).
* **Portability Details**: This layout toggle uses the KDE D-Bus command `busctl --user call org.kde.keyboard ...`. This is KDE-specific and will fail silently in other desktop environments (like GNOME or XFCE). This is not a blocker for `ST-009` functional matrix validation in the current KDE environment. It is deferred to separate ticket **`ST-013` (Multi-Desktop Layout Portability)**.

---

## 4. Temporary Database Selection Mechanism

SmartType resolves the database file path dynamically:
1. In production, [PersonalStore::default_path](../src/core/personal_store.cpp#L43) queries the `XDG_DATA_HOME` environment variable (falling back to `HOME` if unset) and maps to `smarttype/personal.sqlite3`.
2. Unit tests (e.g., [core_tests.cpp](../tests/core_tests.cpp)) bypass the default path by creating an isolated in-memory store: `smarttype::PersonalStore store(":memory:")`.
3. Fcitx integration tests (e.g., [fcitx_integration.cpp](../tests/fcitx_integration.cpp)) isolate the environment by setting and clearing `XDG_DATA_HOME` before Fcitx instance initialization.

To prevent any modifications to the user's database, **`TC-011 Learning` is verified separately in an isolated automated test environment** using temporary directories via `XDG_DATA_HOME`. The user's personal database must never be touched.

---

## 5. Step-by-Step Manual Verification Protocol

### Session Metadata Recording
Before testing each application, the tester must query and record the active session parameters:
- `frontendName`
- `capability flags`
- `ClientSideInputPanel`
- `SurroundingText`
- `Preedit`
- `program`
- `active UI`

### Test Case Independence
Each test case (TC) must start independently in a new empty input field, without any active preedit or pending timers.

### Verification Script

| Step | Test Case | Action | Input Keystrokes | Expected Visual / Document State |
|---|---|---|---|---|
| 1 | **TC-001** | Normal typing protection | Type `https://example.com/test` followed by `Space`. | Document contains: `https://example.com/test ` (no corrections or suggestion panel popup). |
| 2 | **TC-002** | Typos correction | Type `севодня` followed by `Space`. | Document commits: `Сегодня ` (capitalized automatically). |
| 3 | **TC-006a**| Revert Before Delay | Type `севодня` + `Space` $\rightarrow$ press `BackSpace` immediately (within 100ms). | Autocorrect is canceled before commit. Original `севодня` returns as active preedit. |
| 4 | **TC-006b**| Revert After Delay | Type `севодня` + `Space` $\rightarrow$ wait more than 200ms $\rightarrow$ press `BackSpace`. | Committed `Сегодня ` is deleted, and `севодня` returns as active preedit. |
| 5 | **TC-003** | Suggestion panel | Type `вопщем`. | Candidate panel shows suggestions: `вообще`, `в общем`. |
| 6 | **TC-004/005**| Candidate commit | Press `Down` then `Tab` (or navigate/select natively in client-side fallback). | Document commits the selected candidate. Core index and committed candidate match without capsule animation requirements. |
| 7 | **TC-007** | Space handling | Привет → Space → Space | Document contains `Привет␠␠` (exactly two spaces). |
| 8 | **TC-008** | Smart punctuation | телевизор → Space → , | Auto-space is replaced by comma: `телевизор,␠` (exactly one space after comma). |
| 9 | **TC-009** | Case preservation (Capitalization test starts in new empty field) | Type `СЕВОДНЯ` followed by `Space`. | Document commits: `СЕГОДНЯ ` (all uppercase preserved). |
| 10 | **TC-010** | Manual layout toggle | Type `ghjtrn` (english layout keys for "проект"), then press `Control+Shift+Space`. | Preedit text translates to `проект` and system keyboard layout toggles. |
| 11 | **TC-002** | Fast continuous typing | Type `севодня` $\rightarrow$ `Space` $\rightarrow$ letter `п` (choose delay option) $\rightarrow$ rest of `ривет` (fixed typing speed). | Test the following delays between `Space` and `п`:<br>- **Option A (immediately)**<br>- **Option B (around 100 ms)**<br>- **Option C (more than 200 ms)**<br><br>Document contains: `Сегодня привет` (no lost letters, no reordering, correct spelling). |

---

## 6. Proposed Ticket Sequence for Future Phases

* **Candidate Ticket: ST-009.1: Fast Typing Interleaving Fix**
  * *Status*: Candidate ticket. Created only after TC-A interleaving bug is reproduced deterministically.
* **Candidate Ticket: ST-009.2: Asynchronous Backspace Safety**
  * *Status*: Candidate ticket. Created only after TC-B backspace race bug is reproduced deterministically.
* **ST-013: Multi-Desktop Layout Portability**
  * *Status*: Portability task. Resolve layout toggle dependency on KDE settings.
