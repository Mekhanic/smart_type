# Installing SmartType on Linux

## Supported scope

SmartType is an Fcitx 5 input-method addon. The verified release environments
are Fedora 44 KDE Wayland, Ubuntu 26.04 KDE Wayland, and Kali Rolling
Xfce/X11. The native X11 renderer is built by default. GNOME-specific
integration, XWayland, Ubuntu 24.04, Arch, and the Fcitx client-side input-panel
path remain unsupported release targets.

Do not use Flatpak or AppImage for the addon: a sandboxed application cannot
install a shared addon into the host Fcitx process. Use the source install or a
native distro package.

## Recommended installation

Run this command from a terminal inside the graphical session:

```bash
./install.sh
```

It detects the verified distro/session combination, installs dependencies,
builds and tests the project, configures the matching Fcitx integration, adds
SmartType English/Russian to the existing Fcitx group without deleting other
input methods, and enables the tray services. The original Fcitx profile is
saved once as `~/.config/fcitx5/profile.before-smarttype`.

Log out and back in once after the first installation. SmartType English is
selected by default. `./install.sh --skip-deps` skips package installation;
`--mode kde-wayland` and `--mode x11` override session detection.
Builds use two parallel compiler jobs by default so installation remains
reliable on small VMs. Set `SMARTTYPE_BUILD_JOBS=4` (or another value) to
override it.

## Dependencies for manual installation

Install the Fcitx runtime, development headers, CMake, Ninja, SQLite, Qt 6,
Cairo/Pango/GdkPixbuf, Wayland client headers and protocols. Package names:

| Distribution | Command |
|---|---|
| Fedora | `sudo dnf install fcitx5 fcitx5-devel fcitx5-qt fcitx5-gtk fcitx5-configtool hunspell hunspell-ru gcc-c++ cmake ninja-build sqlite-devel pkgconf-pkg-config cairo-devel pango-devel gdk-pixbuf2-devel glib2-devel wayland-devel wayland-protocols-devel qt6-qtbase-devel qt6-qtdeclarative-devel libxcb-devel xcb-util-devel xcb-util-wm-devel xcb-util-keysyms-devel` |
| Ubuntu 26.04 / Kali | `sudo apt install build-essential cmake ninja-build pkg-config libsqlite3-dev fcitx5 libfcitx5core-dev libfcitx5utils-dev libfcitx5config-dev fcitx5-modules-dev fcitx5-frontend-qt6 fcitx5-frontend-gtk3 fcitx5-config-qt hunspell hunspell-ru qt6-base-dev qt6-declarative-dev libcairo2-dev libpango1.0-dev libgdk-pixbuf-2.0-dev libglib2.0-dev libwayland-dev wayland-protocols libxcb1-dev libxcb-util-dev libxcb-icccm4-dev libxcb-xinerama0-dev libxcb-randr0-dev libxcb-ewmh-dev libxcb-keysyms1-dev` |

Package splits can differ by release. If CMake reports a missing Fcitx module
header, install the distribution's Fcitx module development package as well.

## User-local install

```bash
./scripts/install-user.sh
```

`install-user.sh` builds, tests, and installs the project. It writes the actual local addon directory to
`~/.config/environment.d/90-smarttype.conf`; it does not assume `lib64`, does
not invoke a package manager, and does not modify KDE settings by default. It
also enables `smarttype-tray.service`, removes the legacy XDG tray autostart,
and refreshes the desktop/icon caches. The generated service uses the actual
install prefix, so both `$HOME/.local` and `/usr` installations launch the
matching binary.
It also adds both SmartType methods to the current Fcitx group while preserving
the user's existing methods. Pass `--no-configure-profile` only when managing
the Fcitx profile manually. Log out and in after the first install so Fcitx
inherits the new environment.

On Xfce/X11, make Fcitx the sole layout owner and enable persistent SmartType
activation with:

```bash
./scripts/install-user.sh --enable-x11-layout-sync
```

Do not combine this with `--enable-kde-layout-sync`. The X11 mode removes only
the conflicting `grp:*`/`grp_led:*` XKB options, preserves unrelated XKB
options, assigns Alt+Shift to Fcitx, and writes a managed `.xprofile` block for
the GTK/Qt Fcitx modules. Log out or reboot before testing; already-running
applications retain their old input module.

For the optional KDE-only Alt+Shift bridge:

```bash
./scripts/install-user.sh --enable-kde-layout-sync
```

If kimpanel takes precedence over the native SmartType panel, deliberately
opt in to disabling it:

```bash
./scripts/install-user.sh --disable-kimpanel
```

## Distribution packages

Future release channels are native RPM and DEB packages. A system
package must install the addons to the distro-selected `${CMAKE_INSTALL_LIBDIR}/fcitx5`
and the Fcitx metadata to `${CMAKE_INSTALL_DATADIR}/fcitx5`. Package recipes
and CI smoke builds remain release work; until then the user-local source
install above is the supported installation path.

## Verification

```bash
./scripts/doctor.sh
systemctl --user status smarttype-tray.service
fcitx5-configtool
```

On X11, `doctor.sh` must print `SmartType candidate panel includes native X11
support`. If it reports a Wayland-only library, do not continue to visual
testing: rebuild and reinstall first.

It must also report that both `GTK_IM_MODULE` and `QT_IM_MODULE` use `fcitx`.
An application reporting `frontend:xim` is a fallback context and is not a
release-grade inline-preedit verification.

Select **SmartType Русский** and **SmartType Английский** in Fcitx. Then check
typing, a candidate click, and a click elsewhere while candidates are visible:
the latter must close the panel rather than moving it to the mouse position.

For a release-grade Ubuntu/Arch check, including the exact reboot and typing
cases to record, use [DISTRO_TEST_PLAN.md](DISTRO_TEST_PLAN.md).
