# SmartType Project State

## Current release candidate

- Version: `0.2.3`.
- Branch: `agent/gnome-runtime-fix`; public `main` currently contains the
  `v0.2.2` source commit.
- Automated suite: 17/17 PASS in the Ubuntu 26.04 target build; the focused
  GNOME configuration regression also passes directly on the host.
- Publication state: `v0.2.2` exists only as a rejected prerelease. It is not
  Latest and the default installer still resolves `v0.2.1`. Version `0.2.3`
  has not been tagged or published.
- Release gate: the `0.2.3` GNOME reinstall regression and the full 17-test
  suite pass. New immutable tagged assets still require VM installation and
  owner typing confirmation.

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

The `v0.2.2` prerelease assets passed checksums and target-container smoke
tests, but the Ubuntu VM reinstall exposed a conflicting GNOME
`disabled-extensions` entry. The `0.2.3` fix removes that conflict; after a
reboot Kimpanel was `ACTIVE`, Fcitx delegated candidates to Kimpanel and doctor
had no `FAIL`.

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

Commit the `0.2.3` reinstall fix, pass main CI, publish new immutable prerelease
assets and install them on the three prepared VM environments. Promote only
`v0.2.3` after owner confirmation; leave rejected `v0.2.2` as a prerelease.
