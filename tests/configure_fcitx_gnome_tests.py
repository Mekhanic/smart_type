#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import pathlib
import tempfile
from types import SimpleNamespace
from unittest import mock


def load(path: pathlib.Path):
    spec = importlib.util.spec_from_file_location("configure_fcitx_gnome", path)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main() -> None:
    import sys

    module = load(pathlib.Path(sys.argv[1]))
    with tempfile.TemporaryDirectory() as temp:
        root = pathlib.Path(temp)
        fcitx_config = root / "fcitx5" / "config"
        config = root / "fcitx5" / "conf"
        environment = root / "90-smarttype.conf"
        autostart = root / "org.fcitx.Fcitx5.desktop"

        config.mkdir(parents=True)
        fcitx_config.write_text(
            "[Behavior/EnabledAddons]\n0=clipboard\n\n"
            "[Behavior/DisabledAddons]\n0=notificationitem\n",
            encoding="utf-8",
        )
        (config / "smarttypeui.conf").write_text(
            "[Addon]\nEnabled=True\n\n[Theme]\nScale=115\n", encoding="utf-8"
        )
        environment.write_text(
            "FCITX_ADDON_DIRS=/custom\nQT_IM_MODULE=xim\nUNRELATED=value\n",
            encoding="utf-8",
        )
        autostart.parent.mkdir(parents=True, exist_ok=True)
        autostart.write_text("[Desktop Entry]\nHidden=true\n", encoding="utf-8")

        module.configure_fcitx(fcitx_config)
        module.configure_environment(environment)
        module.configure_autostart(autostart)

        smarttypeui = (config / "smarttypeui.conf").read_text()
        assert "Enabled=True" in smarttypeui
        assert "Scale=115" in smarttypeui
        fcitx = fcitx_config.read_text()
        assert "0=Alt+Shift_L" in fcitx
        assert "1=Shift+Alt_L" in fcitx
        assert "EnumerateSkipFirst=True" in fcitx
        assert "ActiveByDefault=True" in fcitx
        assert "ShareInputState=All" in fcitx
        assert "[Behavior/EnabledAddons]" in fcitx
        assert "0=clipboard" in fcitx
        assert "1=kimpanel" in fcitx
        assert "2=ibusfrontend" in fcitx
        assert "[Behavior/DisabledAddons]" in fcitx
        assert "0=notificationitem" in fcitx
        assert "1=smarttypeui" in fcitx

        managed = environment.read_text()
        assert "FCITX_ADDON_DIRS=/custom" in managed
        assert "UNRELATED=value" in managed
        assert "QT_IM_MODULE=xim" not in managed
        assert "GTK_IM_MODULE=fcitx" in managed
        assert "QT_IM_MODULE=fcitx" in managed
        assert "QT_IM_MODULES=wayland;fcitx" in managed
        assert "XMODIFIERS=@im=fcitx" in managed

        desktop = autostart.read_text()
        assert "Exec=fcitx5 -d --replace" in desktop
        assert "X-GNOME-Autostart-enabled=true" in desktop
        assert "X-SmartType-Managed=true" in desktop
        assert autostart.with_name(autostart.name + ".before-smarttype-gnome").exists()

        before = (fcitx, smarttypeui, managed, desktop)
        module.configure_fcitx(fcitx_config)
        module.configure_environment(environment)
        module.configure_autostart(autostart)
        after = (
            fcitx_config.read_text(),
            (config / "smarttypeui.conf").read_text(),
            environment.read_text(),
            autostart.read_text(),
        )
        assert before == after

    settings = {
        "enabled-extensions": ["existing@example.org"],
        "disabled-extensions": [
            module.KIMPANEL_UUID,
            "disabled@example.org",
            "appindicator@example.org",
        ],
    }
    writes: dict[str, list[str]] = {}

    def fake_run(command, **_kwargs):
        key = command[3]
        if command[1] == "get":
            return SimpleNamespace(stdout=repr(settings[key]))
        assert command[1] == "set"
        import ast

        writes[key] = ast.literal_eval(command[4])
        return SimpleNamespace(stdout="")

    with mock.patch.object(module.shutil, "which", return_value="/usr/bin/gsettings"), \
         mock.patch.object(module.subprocess, "run", side_effect=fake_run):
        module.enable_extensions(
            [module.KIMPANEL_UUID, "appindicator@example.org"]
        )

    assert writes["enabled-extensions"] == [
        "existing@example.org",
        module.KIMPANEL_UUID,
        "appindicator@example.org",
    ]
    assert writes["disabled-extensions"] == ["disabled@example.org"]

    print("GNOME Fcitx configuration tests passed")


if __name__ == "__main__":
    main()
