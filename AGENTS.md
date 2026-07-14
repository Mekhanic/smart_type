# SmartType — Agent Instructions

This repository expects coding agents to work autonomously and carry tasks through to a verified result.
Before substantial work, consult the project documents relevant to the task, especially `docs/PRODUCT.md`,
`docs/ARCHITECTURE.md`, `docs/STATE.md`, `docs/BACKLOG.md`, `docs/TEST_MATRIX.md`,
`docs/DECISIONS.md`, `docs/RUNBOOK.md`, and `docs/UI_SPEC.md`.

## Working model

- Infer the intended outcome from the request and repository state; ask only when a missing choice would materially change the result.
- Diagnose, edit, build, test, install, reload, and perform live verification when those actions are useful to complete the request.
- Work across multiple related tickets when that is the shortest coherent route to the requested outcome.
- Keep changes focused, but fix adjacent causes when evidence shows they are part of the same problem.
- Update `STATE.md`, `BACKLOG.md`, and `TEST_MATRIX.md` when their recorded state changes.
- Report concrete evidence: relevant diffs, commands, test failures, installed artifact identity, and live behavior.
- Do not claim success from a test summary alone; distinguish automated, installed, and live verification.

## Safety and repository hygiene

- Preserve unrelated user changes in a dirty worktree.
- Do not use `git reset --hard`, discard user work, or delete user data unless explicitly requested.
- Do not use `git add .`; stage and commit only intentional files when a commit is requested or clearly part of the task.
- Prefer project installation and management scripts. Direct local fixes are allowed when the scripts are broken or insufficient, but document them.
- Configuration, Fcitx addons, environment settings, installation, and process reloads may be changed when required by the task; inspect current state first and verify afterward.
- Avoid destructive or irreversible system changes when a safer route exists.

## Product priorities

Core behavior across supported applications takes priority: no duplicated or lost text, consistent typing,
autocorrection, suggestions, candidate navigation, Backspace revert, punctuation, case, layout, and learning.
Rendering differences are acceptable where the architecture requires them.

Current rendering policy:

- `wayland` / `wayland_v2` uses native server-side `smarttypeui`;
- `dbus` with `ClientSideInputPanel` uses the toolkit fallback;
- X11 uses the native XCB renderer with GTK/Qt Fcitx frontend modules;
- XWayland-only and plain XIM fallback paths are not release targets.
