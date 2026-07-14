# SmartType Runbook

This document describes how to compile the project, run automated test suites, and perform manual diagnostics safely without disrupting the system.

---

## 1. Building the Project

Ensure you have a C++20 compiler, CMake, Ninja, SQLite3 development libraries, and `fcitx5-devel` headers installed.

```bash
# Configure build directory
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DSMARTTYPE_BUILD_FCITX=ON \
      -DSMARTTYPE_BUILD_UI_DEMO=ON \
      -DSMARTTYPE_BUILD_TRAY=ON

# Build the targets
cmake --build build
```

---

## 2. Running Automated Tests

Run the test suite using `ctest` inside the build directory. Do not claim success based only on `100% passed`; always check the logs for detailed warnings or diagnostics.

```bash
# Run all tests and print details on failure
cd build && ctest --output-on-failure
```

The test suites include:
- `core`: Tests the core autocorrection rules and token boundaries.
- `eval-baseline`: Compares correction quality against standard baseline sets.
- `animation-math`, `animation-interrupt`: Independent UI animation checks.
- `addon-smoke`: Loads the generated addon config and `smarttypeui.so` from the current build; it never validates a stale `~/.local` installation.
- `fcitx-integration`: Uses isolated XDG directories and covers real addon input, correction, learning, layout, and no-`SurroundingText` fallback behavior.
- `settings-smoke`, `report-dialog-smoke`: Offscreen tray/settings construction checks.

`smarttype-fcitx-test` deliberately refuses direct execution unless `SMARTTYPE_INTEGRATION_TEST=1` and isolated `XDG_DATA_HOME`, `XDG_CONFIG_HOME`, and `XDG_STATE_HOME` are set. Use CTest instead of invoking it against the desktop session.

---

## 3. Manual Verification & Troubleshooting

To verify the Fcitx5 addon status:
1. Check environmental variables:
   ```bash
   echo $QT_IM_MODULE
   echo $GTK_IM_MODULE
   echo $XMODIFIERS
   ```
2. Verify active Fcitx5 config before changing it; prefer the project tools and keep manual edits traceable:
   ```bash
   cat ~/.config/fcitx5/config
   ```
3. Run the target applications (Chrome, Telegram, Kate) from a terminal to trace any stderr/stdout warnings from the IM module or DBus interface.

### X11 candidate panel missing

Do not infer X11 support from a tray icon, `smarttypectl status`, `smarttype-eval`
or green CTest. Confirm the installed UI library actually contains the XCB
renderer:

```bash
echo "$XDG_SESSION_TYPE"  # must be x11 for this check
./scripts/doctor.sh
nm -D ~/.local/lib/fcitx5/smarttypeui.so 2>/dev/null | \
  grep '_ZN5fcitx9classicui14XCBInputWindow6updateEPNS_12InputContextE'
```

If doctor reports a Wayland-only library, install the XCB development packages
from `docs/INSTALL.md`, configure with `-DSMARTTYPE_ENABLE_X11=ON`, rebuild,
reinstall, and restart Fcitx. While Kate has focus, use Fcitx DebugInfo to verify
that the context has an `x11` frontend/display; only then judge panel behavior.

For Xfce X11, install with `--enable-x11-layout-sync`, relogin, and verify the
Xfce session plus each target process contains:

```text
GTK_IM_MODULE=fcitx
QT_IM_MODULE=fcitx
XMODIFIERS=@im=fcitx
```

Mousepad, Firefox and Kate should report focused `frontend:dbus` contexts.
`frontend:xim` is only a fallback: do not accept a server-side preedit row in
the candidate popup as successful inline input.

### Layout stuck: Alt+Shift changes only the system indicator (ST-020)

SmartType uses **two Fcitx input methods** (`smarttype-us` / `smarttype`). Plasma **Alt+Shift** only toggles the KDE layout index (`org.kde.keyboard`). The bridge process is required:

```bash
systemctl --user status fcitx5-layout-sync.service
# must be active (running) and enabled

# Recover immediately:
systemctl --user enable --now fcitx5-layout-sync.service

# Or re-run user install (enables the unit since ST-020):
./scripts/install-user.sh

# Diagnostics:
fcitx5-remote -n                                          # smarttype-us | smarttype
busctl --user call org.kde.keyboard /Layouts org.kde.KeyboardLayouts getLayout
tail -20 ~/.local/state/smarttype/layout-events.jsonl
./scripts/doctor.sh
```

Symptoms when the service is **down**: auto layout correction still works (engine calls Fcitx + KDE `setLayout`); **manual** Alt+Shift only moves the Plasma indicator; typing language stays on the last SmartType IM. Re-selecting EN/RU in the Fcitx/keyboard tray “fixes” it until the next desync.

Do **not** fix this by restarting `fcitx5` first — start `fcitx5-layout-sync` and retest Alt+Shift.

### After reboot checklist

1. `systemctl --user is-enabled smarttype-tray.service` → `enabled`
2. `systemctl --user is-active smarttype-tray.service` → `active`
3. `systemctl --user is-active fcitx5-layout-sync.service` → `active`
4. `fcitx5-remote -n` → `smarttype` or `smarttype-us`
5. Kate: Alt+Shift twice — language of typed characters must follow
6. `./scripts/doctor.sh` — no FAIL (owned tray process + layout-sync + session IM)

The tray is owned by `smarttype-tray.service`, not by an XDG autostart desktop
entry. A plain application-menu launch contacts the already-running instance
and opens settings. Diagnose startup with:

```bash
systemctl --user status smarttype-tray.service
journalctl --user -b -u smarttype-tray.service
systemctl --user show smarttype-tray.service -p MainPID
```

### Session IM durability (ST-018 / ST-028) — Telegram “dead input”

**Healthy session** (Wayland + Fcitx as Plasma Virtual Keyboard):

```bash
systemctl --user show-environment | grep -E 'QT_IM|XMODIFIERS|IMSETTINGS'
# expect: QT_IM_MODULE absent; XMODIFIERS=@im=fcitx or @im=fcitx5
# expect: IMSETTINGS_MODULE=fcitx5 (not none)

# Telegram / AyuGram must NOT need:
#   env -u QT_IM_MODULE -u XMODIFIERS <binary>

busctl --user call org.fcitx.Fcitx5 /controller org.fcitx.Fcitx.Controller1 DebugInfo
# focused Telegram/AyuGram IC should show rich caps (e.g. 0x8000d0072 = Preedit+SurroundingText)
# weak caps like 0x1072 without Preedit ⇒ session IM broken again

./scripts/doctor.sh
# FAIL if QT_IM_MODULE=xim, XMODIFIERS=@im=none, or imsettings-start not Hidden
```

**Recover if Telegram is dead again:**

1. Check whether `QT_IM_MODULE=xim` or `XMODIFIERS=@im=none` reappeared in `systemctl --user show-environment`.
2. Ensure `~/.config/autostart/imsettings-start.desktop` exists with `Hidden=true` (and optionally `X-GNOME-Autostart-enabled=false`).
3. Keep `~/.config/environment.d/91-smarttype-im-wayland.conf` with `XMODIFIERS=@im=fcitx` and **no** `QT_IM_MODULE=xim`.
4. `systemctl --user unset-environment QT_IM_MODULE` then **relaunch** Telegram/AyuGram (or re-login).
5. Optional hardening: `ln -sf /etc/X11/xinit/xinput.d/fcitx5.conf ~/.xinputrc` if imsettings re-engages on this distro.
6. Do **not** treat this as an engine/layout bug until DebugInfo shows rich caps and env is clean.

### Phrase layout (ST-021)

If you type several words on the wrong layout and the last word finally “clicks” (auto or mid-word), SmartType rewrites the **preceding wrong-layout run** (up to 5 words) when SurroundingText is available — e.g. `F ns xnj` → `А ты что`. Requires app support for SurroundingText (Kate/Chrome/Telegram normally yes).

### Quick manual smoke (15 min) — ST-022

Run in **Kate**, then **Chrome**, then **Telegram** (empty field each time):

| # | Keys / action | Expect |
|---|----------------|--------|
| 1 | `привет` + Space | `привет ` no garbage |
| 2 | `севодня` + Space | → `сегодня ` (or suggest) |
| 3 | After (2) immediate Backspace | reverts correction |
| 4 | `ghbdtn` + Space on EN | → `привет ` + IM→RU |
| 5 | `F ns xnj` + Space on EN | → `А ты что ` (phrase) |
| 6 | Alt+Shift twice | both indicator **and** typed language flip |
| 7 | `автоплатежей` + Space | must **not** become `авто платежей` (note if it does = ST-023) |
| 8 | `happ.info` + Space | must **not** become English gibberish (ST-024) |
| 9 | Arrow/Tab on candidates for a typo | navigates and commits |
| 10 | Pause ST for app from tray | no corrections in that app |
| 11 | Chromium dynamic city field: `Махачкала` | exactly `Махачкала`, never cumulative prefixes |

Note fails with: app, exact keys, what you saw, `fcitx5-remote -n`, layout-sync active?
