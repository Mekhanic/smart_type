# SmartType Project State

## Current stable release

- Version: `0.2.3`.
- Branch: `agent/gnome-runtime-fix`; public `main` and tag `v0.2.3` contain the
  release source.
- Automated suite: 17/17 PASS in all three immutable tag builds.
- Publication state: `v0.2.3` is the stable GitHub Latest release. The default
  one-line installer resolves `v0.2.3`.
- Release gate: PASS. Tagged assets were checksum-verified, installed in the
  Fedora, Ubuntu and Kali VMs, cold-rebooted and checked with doctor. The owner
  confirmed the final Ubuntu GNOME typing and candidate-panel behavior.

The current release fixes the release-blocking failures found during the
three clean-VM passes:

- KDE no longer forces SmartType back to English every two seconds;
- proactive `Ghbdtn` → `Привет` correction has an exact Backspace transaction,
  both before and after a delimiter;
- moving the caret with candidates open cancels the old composition instead of
  inserting it at the new caret;
- terminal and paused-application exclusions keep stateless RU/EN layout
  mapping while disabling correction, candidates, learning and edit
  transactions;
- GNOME compositor contexts, Kimpanel mouse interaction and LibreOffice's GTK
  Fcitx path work without duplicated or lost text;
- Kali/X11 cold login selects SmartType even when Fcitx stores the startup
  group outside numeric `Groups/0`.

## Verified installed candidates

| Environment | Installed evidence | Owner result |
|---|---|---|
| Fedora 44 KDE Plasma Wayland | clean overlay, one-line install, cold reboot, canonical engine loaded, doctor green | PASS: switching, candidates, correction and both undo paths |
| Ubuntu 26.04 GNOME Wayland | clean official-image overlay, one-line install, cold reboot, Kimpanel active, canonical engine loaded, doctor green | PASS: Writer/terminal switching while paused, correction/undo and caret relocation |
| Kali Rolling Xfce/X11 | cleaned overlay of the existing QEMU VM, one-line install, cold reboot, `smarttype-us` active, canonical engine/UI loaded, doctor green | PASS: Mousepad/Firefox ESR/Kate/QTerminal matrix |

Final published archives:

| Target | SHA-256 |
|---|---|
| Fedora 44 x86_64 | `1ae37fe3d2eff4225f9eafa9010fd1b5bc0074e969a839f716c38bbf67d2c75f` |
| Ubuntu 26.04 x86_64 | `ca311ff9205803ccc23c571c80d30c014c9f4c5fb93b58fd84fa551cf26f42b0` |
| Kali Rolling x86_64 | `dc5cd5bad560db2ffb2cbfa2a43d4577945760dc440bdfb471cbc28928f5c142` |

The rejected `v0.2.2` prerelease exposed a conflicting GNOME
`disabled-extensions` entry during tagged-asset verification. The `v0.2.3`
installer removes that conflict; after reboot Kimpanel was `ACTIVE`, Fcitx
delegated candidates to it and doctor had no `FAIL`.

## Supported architecture

- Fedora and Ubuntu KDE/Wayland use native server-side `smarttypeui`.
- Fedora and Ubuntu GNOME/Wayland use Fcitx IBus plus Kimpanel.
- Kali Xfce/X11 uses toolkit Fcitx preedit plus the native XCB candidate window.
- Toolkit client-side panels may render differently but must preserve core
  correction behavior.
- XWayland-only, raw XIM and unsupported distro releases are not release
  acceptance paths.

## Known limitation

Ubuntu Firefox Snap bundles a legacy client-side `fcitx4` panel. Corrections
work, but Firefox owns that popup and may place it away from the caret. This is
not evidence of a Kimpanel positioning defect.

## Next action

Monitor incoming reports against the published hashes. Do not replace release
assets; any code or packaging fix must receive a new version and repeat the
same prerelease, tagged-asset and owner-verification gates.
