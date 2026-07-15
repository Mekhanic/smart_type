# Third-party notices

SmartType's GNOME Wayland release payload includes files from
[`gnome-shell-extension-kimpanel`](https://github.com/wengxt/gnome-shell-extension-kimpanel)
at commit `ff828412608da89d8ede464c85649659a19a7650`.

Those extension files are distributed under GNU GPL version 2. Their original
`COPYING` file and upstream README are included next to the installed extension
in `share/smarttype/gnome/kimpanel@kde.org/`.

SmartType applies one documented compatibility patch after verifying the
upstream archive: legacy `SetSpotRect` coordinates are translated through the
focused window only for native Wayland clients. The installed
`SMARTTYPE_PATCHES` file records the same change.

SmartType itself remains licensed under GNU GPL version 3. See [`LICENSE`](LICENSE).
