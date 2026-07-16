# SmartType Project State

## Current release candidate

- Version: `0.2.2`.
- Branch: `agent/gnome-runtime-fix`, based on public `v0.2.1`.
- Automated suite: 17/17 PASS on the native build and in each release build.
- Publication state: release preparation is complete locally; no tag or
  publication exists yet.
- Release gate: Fedora 44 KDE, Ubuntu 26.04 GNOME and Kali Rolling Xfce have
  passed clean-install, cold-reboot and owner typing checks.

The current candidate fixes the release-blocking failures found during the
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

Locally verified pre-tag archives built from the release source tree:

| Target | SHA-256 |
|---|---|
| Fedora 44 x86_64 | `34177919a63d6fde57a2251ad4563355575d03586ba45c09cec2ac136a3db868` |
| Ubuntu 26.04 x86_64 | `107ac3a684ae41cf130bb768267d514b8c1546edf5aa2c0504bb980a39c18822` |
| Kali Rolling x86_64 | `da963cd71fe23db722ba36ea167f7f3ba6a6e2b0e663ccfe7b829683d2848835` |

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

The tag workflow will rebuild the archives and publish its own checksum files.
Publish `v0.2.2` as a prerelease after owner approval. Install the tagged assets
on restored clean VM overlays and promote the same immutable assets to a normal
release only after that pass.
