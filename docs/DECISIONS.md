# SmartType Architectural Decisions Log (ADR)

This document tracks major design and structural decisions.

---

## ADR-001: Separation of Functional Parity and Visual Parity

### Context
Attempting to force visual parity (fonts, sizing, animations) across all desktop environments and sandboxed applications (e.g., Flatpaks, Snap) requires implementing multiple custom renderers. It introduces high complexity to the code and breaks stability.

### Decision
- **Functional identicality** is a mandatory product requirement. Characters must not be lost or duplicated, corrections must occur identically, and navigation/revert behavior must be uniform. ST-009 established the baseline; ongoing owner testing is recorded in `TEST_MATRIX.md`.
- **Visual divergence** between frontends is acceptable. The appearance of candidate selection depends on the rendering path chosen.

### Consequences
- Candidate window appearance and optional visual effects may vary. Core typing and correction behavior must not vary.
- Ongoing manual testing must verify core functional equivalence even if UI appearance differs.

---

## ADR-002: Design Freeze during Functional Stabilization (superseded)

### Context
Frequent adjustments to visual styles (margins, padding, colors, transparency, blur, animations, and graphic assets) lead to regressions in basic typing correctness and environment stability.

### Decision
- Historical decision: the candidate panel design was frozen while early P0 behavior was unstable.
- This restriction no longer applies globally; visual changes should be intentional and kept separate from unrelated functional fixes.

### Consequences
- Superseded on 2026-07-11: visual changes are allowed when deliberately requested; unrelated visual edits should still be avoided during functional fixes.

---

## ADR-003: Policy on Render Paths and User Environment

### Context
Fcitx selects the candidate rendering path from the input-context capabilities and active frontend. SmartType must support the selected path without modifying the user's environment.

### Decision
1. **Capability-First Render Path Selection**:
   The render path is chosen dynamically based on the input context capabilities and active frontend:
   * **Client-side toolkit renderer**: Selected when `ClientSideInputPanel=true`. The candidate list is passed via a frontend-specific client-side UI mechanism; the visual rendering (fonts, layout, window borders) is controlled entirely by the client application's toolkit IM-module, and is not managed by the internal `smarttypeui` Fcitx5 addon.
   * **Internal `smarttypeui` on Wayland**: Selected when `ClientSideInputPanel=false`, `frontend=wayland/wayland_v2`, and a native popup surface is available.
   * **Internal `smarttypeui` on native X11**: The production addon is built with `SMARTTYPE_ENABLE_X11=ON` and contains the existing XCB renderer. The build and addon smoke test must fail if X11 was requested but `XCBInputWindow` was omitted.
   * **Unsupported / Future Fallback**: XWayland contexts inside a Wayland session remain unsupported until their coordinate mapping is verified. A client-side toolkit renderer may still be used when the input context advertises `ClientSideInputPanel`.
2. **Legacy/Experimental Path**:
   * The external `smarttype-ui` standalone QML process is designated as legacy/experimental and is not considered a current supported path.
3. **Scope of Non-Interference**:
   * SmartType does not silently force frontend selection during a normal install.
   * The explicit `--enable-x11-layout-sync` integration mode is the narrow exception approved on 2026-07-14: it writes a managed, idempotent X11 block with `GTK_IM_MODULE=fcitx`, `QT_IM_MODULE=fcitx`, and `XMODIFIERS=@im=fcitx` to the user's environment drop-in and `.xprofile`. This is required for inline GTK/Qt preedit on LightDM/Xfce X11.
   * The exception must never be applied by the KDE Wayland install modes, where globally forcing toolkit variables can select the wrong frontend.
   * Other toolkit configuration, addons, and application executables remain outside SmartType's control unless the user explicitly authorizes a separate integration change.
4. **Observed Environment Examples**:
   * *Wayland-native applications* (e.g., observed Chrome setup): Usually resolve to the `wayland` or `wayland_v2` frontend with `ClientSideInputPanel=false`, using the internal `smarttypeui` addon.
   * *D-Bus client-side applications* (e.g., observed Telegram setup): Usually resolve to the `dbus` frontend with `ClientSideInputPanel=true`, using the frontend-specific client-side UI mechanism (D-Bus).
   * *Environments where Fcitx5 is bypassed* (e.g., observed Kate/KWrite setup): The input method might not activate. The actual frontend loaded depends on the application's toolkit, Qt version, and session environment. SmartType does not attempt to force activation.

### Consequences
- Core engine logic (autocorrection, delimiters, learning, and case preservation) must function uniformly across all current supported paths and is tracked in `TEST_MATRIX.md`.
- Raw XIM is a compatibility fallback, not proof of supported X11 UX. Release
  verification on X11 requires a GTK/Qt toolkit context with inline preedit.

---

## ADR-004: Candidate List Clamping and Boundary Navigation

### Context
In standard Fcitx, candidate list navigation wraps around (pressing Right at the last candidate wraps to the first candidate, and pressing Left at the first wraps to the last). However, standard Apple-style candidate panels clamp selection to the boundaries (navigation stops at the ends) and provide visual boundary/rubber-band feedback on the selection pill to indicate that the boundary has been reached.

### Decision
- **Clamping Navigation**: SmartType candidate navigation is clamped to the boundaries (indices 0 and count-1). Selected index does not wrap around when pressing navigation keys at the boundary.
- **Selection Pill Edge Feedback**: When navigation is attempted beyond the boundary, a visual rubber-band stretch/bounce animation is applied to the selection pill, while the popup window itself remains completely stationary.

### Consequences
- `smarttype_engine.cpp` clamps the selected index during Arrow key / Tab navigation.
- Platform-native renderers (e.g. native Wayland `smarttypeui` addon) implement the stationary window and selection-pill bounce/stretch feedback animation.
- Automated tests (`fcitx_integration`) are updated to assume clamping navigation instead of wrapping navigation.

---

## ADR-005: Candidate Panel Scale Parameter

### Context
Users on HiDPI or accessibility-oriented setups may need the candidate panel at a different visual size without modifying theme files or restarting Fcitx.

### Decision
- A single integer option `CandidatePanelScale` (range 80–130, default 100, stored in `conf/smarttypeui.conf`) is added to `ClassicUIConfig`.
- The scale factor `s = candidatePanelScale / 100.0` is applied at every `sizeHint()` / `updateYogaLayout()` call to: font size (via `pango_font_description_set_absolute_size`), all text margins, yoga root padding, max panel width (520 × s), and bounce shift (2.5 × s).
- Colors, opacity, 1 px borders/highlights, animation duration, and candidate count are NOT scaled.
- Panel width is fit-content: `MultilineLayout::width()` returns `max(normal_width, semibold_width)` to prevent layout shift without reserving space for non-current candidates.
- No theme.conf, SVG, or PNG files are regenerated on scale change.
- No Fcitx restart is needed; the new value takes effect on the next candidate panel update event.

### Consequences
- `classicui.h` gains one `Option<int, IntConstrain>` field.
- `inputwindow.cpp` reads `candidatePanelScale` in `sizeHint()` and `updateYogaLayout()`.
- The engine and all other addons are unaware of this setting.
- Tray (ST-016) exposes the same key via Fcitx D-Bus config API; do not edit `smarttypeui.conf` by hand from the tray.

---

## ADR-006: Telegram / Qt Wayland IM environment (imsettings “none”)

### Context
SmartType appeared “completely dead” in Telegram/AyuGram (no autocorrect, no layout switch) while Chrome worked and Fcitx showed `smarttype-us`. DebugInfo sometimes showed Telegram ICs with `focus:0` and weak `cap:1072` (FormattedPreedit + ClientUnfocusCommit + Lowercase only — **no Preedit, no SurroundingText**). Session environment inherited:

```text
QT_IM_MODULE=xim
XMODIFIERS=@im=none
```

from imsettings module **none** (`/etc/X11/xinit/xinput.d/none.conf`), not from SmartType `environment.d` and not from Telegram `.desktop` Exec lines.

### Decision
1. **Root cause classification**: broken application IM environment / imsettings, **not** missing ST-017 layout sync and **not** “treat FormattedPreedit as Preedit” without a frontend audit.
2. **Diagnostic order** (mandatory before engine hacks):
   - While typing: `settings.current_app` and Fcitx `DebugInfo` (`focus`, `cap`, `program`, `frontend`).
   - One-shot clean launch: `env -u QT_IM_MODULE -u XMODIFIERS <real Telegram binary>` without editing environment.d.
3. **Confirmed experiment (2026-07-09)**: clean launch produced `focus:1` and `cap:0x8000d0072` (**Preedit + SurroundingText** present). User reported SmartType worked again after that launch.
4. **Permanent fix** is tracked as **ST-018**: correct the real env source (imsettings / user systemd environment) so Plasma+Wayland apps use Fcitx-compatible input without `QT_IM_MODULE=xim` + `@im=none`.
5. **Do not** auto-map `FormattedPreedit` → full client preedit path without verifying what the Wayland frontend actually delivers.

### Consequences
- ST-017 remains a valid engine hardening ticket for layout desync, but must not be sold as the “Telegram is dead” fix.
- Operators should prefer Fcitx as Plasma Virtual Keyboard and avoid imsettings **none** on Wayland sessions.
- `current_app` only updates on SmartType keyEvent — absence of update without typing is not proof of failure if `focus:1` is observed.
- **Applied fix (ST-018)**: disable user `imsettings-start` autostart; document Wayland policy in `environment.d/91-smarttype-im-wayland.conf` without setting `QT_IM_MODULE=xim`; optional `~/.xinputrc` → fcitx5.
- **Durability verified (ST-028, 2026-07-10)**: after reboot, session keeps `QT_IM_MODULE` unset and `XMODIFIERS=@im=fcitx5`; Telegram/AyuGram ICs retain rich Preedit+SurroundingText (`cap:8000d0072`) without clean-launch. `scripts/doctor.sh` fails closed on regression to xim/none.

---

## ADR-007: Chromium text transaction and tray ownership

### Context

Some controlled Chromium web inputs materialise the active Wayland preedit when
their JavaScript value is synchronised, then reset the input context. Sending
the complete preedit again on the next key accumulates prefixes (`М` + `Ма` +
`Маха` ...). Separately, the XDG-autostart tray process could exit successfully
mid-session and remain absent; its 1024x1024 desktop icon directory was not part
of the installed hicolor theme index.

### Decision

- Chromium-family contexts use immediate literal commits and retain only the
  logical word in the engine. Word-boundary correction uses forwarded Backspaces.
- Stable non-Chromium clients continue to use client preedit.
- `smarttype-tray.service` is the sole owner of the long-lived tray process and
  uses restart recovery. The application desktop entry remains a settings launcher.
- Tray images are compiled into the executable; desktop images are installed in
  the standard hicolor 512x512 application directory and icon caches are refreshed.

### Consequences

- Controlled web inputs cannot duplicate cumulative SmartType preedit.
- Chromium corrections may produce several Backspace transactions, which is the
  already-tested compatibility path for browsers that ignore surrounding deletion.
- Tray health is testable through systemd `is-enabled`, `is-active`, and `MainPID`
  rather than inferred from an arbitrary process name.

---

## ADR-008: GNOME Wayland integration through Fcitx IBus and Kimpanel

### Context

GNOME Wayland does not provide KDE's input-panel protocol to third-party input
methods. Reusing the KDE `smarttypeui` path would leave the candidate popup
without a supported compositor surface. Reusing the former X11 server-side
preedit workaround would be worse: composition could appear inside the popup
instead of the focused application field.

### Decision

- GNOME Wayland enables Fcitx `ibusfrontend` and `kimpanel` and disables the
  internal `smarttypeui` addon through Fcitx's global addon override lists.
- Application input proxied by GNOME's IBus compositor is accepted even when
  its program identifier is `gnome-shell`; rejecting that identifier disables
  SmartType for every native GNOME application, not only Shell UI fields.
- `GTK_IM_MODULE=fcitx` is used because Ubuntu's confined Firefox cannot create
  a working Fcitx IBus context. Its bundled legacy-compatible GTK module sends
  a window-relative caret rectangle through Kimpanel's old absolute method.
  The pinned extension contains a narrow correction that treats this method as
  relative only for a focused native Wayland window; X11/XWayland stays absolute.
- GTK and Qt applications receive composition through their Fcitx frontend;
  Kimpanel renders candidates only.
- SmartType installs the upstream `kimpanel@kde.org` GNOME Shell extension from
  one pinned commit whose archive SHA-256 is verified during release assembly.
- Configuration is user-local and idempotent. Existing extension membership,
  unrelated environment variables, Fcitx methods and addon settings are
  preserved or backed up.
- The KDE layout synchronizer is disabled in GNOME. Fcitx owns RU/US switching
  through Alt+Shift and shared input state; the engine normalizes physical
  keysyms to the selected SmartType method because GNOME keeps a single
  compositor XKB source.

### Consequences

- GNOME and KDE use different candidate renderers but the same SmartType engine
  and correction semantics.
- A first install requires a logout/login so GNOME Shell loads the extension
  and applications inherit the input-method environment.
- XWayland-only contexts remain outside the release target; supporting GNOME
  Wayland does not imply support for every application packaging/sandbox path.
