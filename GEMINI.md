# SmartType Engineering Contract

@./docs/PRODUCT.md
@./docs/ARCHITECTURE.md
@./docs/STATE.md
@./docs/BACKLOG.md
@./docs/TEST_MATRIX.md
@./docs/DECISIONS.md
@./docs/RUNBOOK.md
@./docs/UI_SPEC.md

Agents are authorized to work autonomously from diagnosis through implementation, testing, installation,
component reload, and live verification. They may address multiple related tickets in one session and should
pause only when a missing user decision would materially change the result or an action is unusually destructive.

## Required behavior

- Read the relevant project state before acting and follow evidence rather than assumptions.
- Preserve unrelated user changes and keep the resulting diff intentional.
- Update project tracking documents when reality changes.
- Show actual verification evidence and distinguish automated tests from installed and live results.
- Protect user data and avoid irreversible changes when a safer method exists.
- Never use `git reset --hard`, silently discard user work, or broadly stage unrelated files.

## Product direction

Prioritize identical core typing behavior across supported applications: no duplicated or lost text,
autocorrection, suggestions, navigation, Backspace revert, punctuation, case, layout, and learning.

Rendering policy:

- `wayland` / `wayland_v2`: native server-side SmartTypeUI;
- `dbus` with `ClientSideInputPanel`: toolkit fallback;
- X11: native XCB renderer with GTK/Qt Fcitx frontend modules;
- XWayland-only and plain XIM fallback paths are not release targets.

Visual work is allowed when it is the requested task or does not undermine higher-priority functional work.
