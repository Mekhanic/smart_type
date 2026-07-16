# SmartType Backlog

Only current actionable work belongs here. Completed behavior is documented in
`STATE.md`, verification evidence in `TEST_MATRIX.md`, and release history in
`CHANGELOG.md`.

## v0.2.3 release

- Commit and push the GNOME disabled-extension reinstall regression fix.
- Publish `v0.2.3` as a prerelease; do not replace or mutate the rejected
  `v0.2.2` assets.
- Install the tagged assets on restored clean VM overlays, reboot, run doctor
  and the short manual smoke test.
- Promote the same immutable assets to a normal release only after that pass.

## After v0.2.3

- Reproduce the original controlled Chromium city/region field with
  `Махачкала`; the focused integration regression is already green.
- Perform a real Fedora GNOME Wayland owner pass before describing it as
  owner-verified rather than installer/CI-supported.
- Investigate Ubuntu Firefox Snap popup placement only in its legacy
  client-side `fcitx4` renderer; Kimpanel cannot position that window.
- Add a new distribution only after a clean graphical install, cold reboot,
  doctor pass and the complete manual matrix. Arch and Ubuntu 24.04 are not
  currently advertised release targets.

## Deferred

- Native RPM/DEB packaging.
- XWayland-only and raw-XIM support.
- Exact visual parity across toolkit-owned candidate renderers.
- Canonical-case dictionary storage for deliberately mixed spellings such as
  `еГЭ`.
