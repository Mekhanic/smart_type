# Installing SmartType on Linux

## Supported scope

SmartType is an Fcitx 5 input-method addon. The release environments are
Fedora 44 KDE/GNOME Wayland, Ubuntu 26.04 KDE/GNOME Wayland, and Kali Rolling
Xfce/X11. The native X11 renderer is built by default. XWayland, Ubuntu 24.04,
Arch, and the Fcitx client-side input-panel path remain unsupported release
targets.

Do not use Flatpak or AppImage for the addon: a sandboxed application cannot
install a shared addon into the host Fcitx process. Use the checksum-verified
user-local release bundle.

## Recommended installation

Run this command from a terminal inside the graphical session:

```bash
curl -fsSL https://raw.githubusercontent.com/Mekhanic/smart_type/main/scripts/install-release.sh | bash
```

It detects the verified distro/session combination, installs dependencies,
downloads the matching x86_64 release asset, verifies the published SHA-256,
configures the matching Fcitx integration, adds SmartType English/Russian to
the existing Fcitx group without deleting other input methods, and enables the
tray services. The original Fcitx profile is saved once as
`~/.config/fcitx5/profile.before-smarttype`.

When the bootstrap is piped to Bash, package-manager confirmations are handled
non-interactively so they cannot consume the script input. `sudo` may still ask
for the user's password through the terminal; SmartType never reads or stores it.

On GNOME Wayland the installer makes Fcitx the session input method, enables
its IBus frontend, and installs the Kimpanel GNOME Shell extension for the
candidate popup. The extension source is pinned to a known upstream commit and
verified by SHA-256 before it enters a release bundle. The existing GNOME
extension list and unrelated Fcitx settings are preserved. GNOME uses its own
candidate renderer, so the panel can look slightly different from KDE while
typing, correction, candidate selection and Backspace behavior remain the same.

Ubuntu Firefox Snap is a known exception: its bundled legacy `fcitx4` GTK
module requests `ClientSideInputPanel`, so Firefox owns the candidate popup.
SmartType corrections still run, but Kimpanel cannot correct that popup's
placement. Use a normal GNOME GTK/Qt application for the release-grade panel
check; do not claim the Firefox Snap client-side renderer as Kimpanel coverage.

Log out and back in once after the first installation. SmartType English is
selected by default. The normal path does not install compilers or development
headers. It needs an x86_64 system, internet access, and temporary administrator
access only for runtime packages.

Cloning the repository and running `./install.sh` uses the same prebuilt path.
Use `./install.sh --build-from-source` for development or local compilation.
Source builds use two parallel jobs by default; 4 GB RAM, two CPU cores and
about 5 GB free space are recommended. Set `SMARTTYPE_BUILD_JOBS` to override
the job count.

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

For a manual GNOME Wayland source install use:

```bash
./install.sh --build-from-source --mode gnome-wayland
```

The root `install.sh` and the one-line release installer select this mode
automatically; the manual sequence is intended for installer development.
Log out and back in after installation because GNOME Shell and newly started
applications must inherit the Fcitx environment. Do not enable the KDE layout
bridge in a GNOME session.

For the optional KDE-only Alt+Shift bridge:

```bash
./scripts/install-user.sh --enable-kde-layout-sync
```

If kimpanel takes precedence over the native SmartType panel, deliberately
opt in to disabling it:

```bash
./scripts/install-user.sh --disable-kimpanel
```

## Release bundles and future native packages

GitHub Actions builds and tests separate binary bundles inside Fedora 44,
Ubuntu 26.04 and Kali Rolling containers. A tag publishes all three archives
and checksum files to the GitHub release. This keeps the host-level Fcitx and
per-user desktop configuration in one verified installer.

Native RPM/DEB packages may be added later, but they still need a post-install
user configuration step. AppImage and Flatpak remain a poor technical fit for
a library loaded by the host Fcitx process.

## Verification

```bash
~/.local/share/smarttype/doctor.sh
systemctl --user status smarttype-tray.service
fcitx5-configtool
```

On X11, `doctor.sh` must print `SmartType candidate panel includes native X11
support`. If it reports a Wayland-only library, do not continue to visual
testing: rebuild and reinstall first.

It must also report that both `GTK_IM_MODULE` and `QT_IM_MODULE` use `fcitx`.
An application reporting `frontend:xim` is a fallback context and is not a
release-grade inline-preedit verification.

On GNOME Wayland, `doctor.sh` must report the Kimpanel extension, the Fcitx
autostart entry, enabled `kimpanel` and `ibusfrontend` addons, disabled
`smarttypeui`, the GTK/Qt Fcitx modules, and the four managed input-method
environment variables. If the
extension was enabled for the first time, log out and back in before treating a
missing popup as a defect.

Select **SmartType Русский** and **SmartType Английский** in Fcitx. Then check
typing, a candidate click, and a click elsewhere while candidates are visible:
the latter must close the panel rather than moving it to the mouse position.

For a release-grade Ubuntu/Arch check, including the exact reboot and typing
cases to record, use [DISTRO_TEST_PLAN.md](DISTRO_TEST_PLAN.md).
