#!/usr/bin/env python3
"""Configure the Fcitx 5 integration required by GNOME Wayland."""

from __future__ import annotations

import argparse
import ast
import configparser
import pathlib
import shutil
import subprocess


KIMPANEL_UUID = "kimpanel@kde.org"
FCITX_SETTINGS = {
    "Hotkey": {
        "EnumerateSkipFirst": "True",
        "ModifierOnlyKeyTimeout": "-1",
    },
    "Hotkey/EnumerateForwardKeys": {
        "0": "Alt+Shift_L",
        "1": "Shift+Alt_L",
    },
    "Behavior": {
        "ActiveByDefault": "True",
        "ShareInputState": "All",
    },
}

GNOME_ENABLED_ADDONS = ("kimpanel", "ibusfrontend")
GNOME_DISABLED_ADDONS = ("smarttypeui",)


def configure_addon_overrides(parser: configparser.RawConfigParser) -> None:
    """Persist addon state in the Fcitx global config.

    Fcitx does not read an ``[Addon] Enabled=`` key from an addon's own
    configuration file.  It stores overrides as indexed lists below
    ``Behavior/EnabledAddons`` and ``Behavior/DisabledAddons``.
    """

    enabled_section = "Behavior/EnabledAddons"
    disabled_section = "Behavior/DisabledAddons"
    managed = set(GNOME_ENABLED_ADDONS) | set(GNOME_DISABLED_ADDONS)

    def values(section: str) -> list[str]:
        if not parser.has_section(section):
            return []
        return [value for key, value in parser.items(section) if key.isdigit()]

    enabled = [value for value in values(enabled_section) if value not in managed]
    disabled = [value for value in values(disabled_section) if value not in managed]
    enabled.extend(GNOME_ENABLED_ADDONS)
    disabled.extend(GNOME_DISABLED_ADDONS)

    for section, addons in ((enabled_section, enabled), (disabled_section, disabled)):
        if parser.has_section(section):
            parser.remove_section(section)
        parser.add_section(section)
        for index, addon in enumerate(addons):
            parser.set(section, str(index), addon)


def configure_fcitx(path: pathlib.Path) -> None:
    parser = configparser.RawConfigParser(interpolation=None, strict=False)
    parser.optionxform = str
    if path.exists():
        parser.read(path, encoding="utf-8")
    for section, values in FCITX_SETTINGS.items():
        if not parser.has_section(section):
            parser.add_section(section)
        for key, value in values.items():
            parser.set(section, key, value)
    configure_addon_overrides(parser)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as stream:
        parser.write(stream, space_around_delimiters=False)


def set_addon(path: pathlib.Path, enabled: bool) -> None:
    if path.exists():
        backup = path.with_name(path.name + ".before-smarttype-gnome")
        if not backup.exists():
            shutil.copy2(path, backup)
    parser = configparser.RawConfigParser(interpolation=None, strict=False)
    parser.optionxform = str
    if path.exists():
        parser.read(path, encoding="utf-8")
    if not parser.has_section("Addon"):
        parser.add_section("Addon")
    parser.set("Addon", "Enabled", "True" if enabled else "False")
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as stream:
        parser.write(stream, space_around_delimiters=False)


def configure_environment(path: pathlib.Path) -> None:
    managed = {"GTK_IM_MODULE", "QT_IM_MODULE", "QT_IM_MODULES", "XMODIFIERS"}
    marker = "# GNOME Wayland: Fcitx toolkit modules plus Kimpanel."
    preserved: list[str] = []
    if path.exists():
        for line in path.read_text(encoding="utf-8").splitlines():
            key = line.split("=", 1)[0].strip() if "=" in line else ""
            if key not in managed and not line.startswith("# GNOME Wayland:"):
                preserved.append(line)
    while preserved and not preserved[-1]:
        preserved.pop()
    preserved.extend(
        [
            marker,
            "GTK_IM_MODULE=fcitx",
            "QT_IM_MODULE=fcitx",
            "QT_IM_MODULES=wayland;fcitx",
            "XMODIFIERS=@im=fcitx",
        ]
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(preserved) + "\n", encoding="utf-8")


def configure_autostart(path: pathlib.Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and "X-SmartType-Managed=true" not in path.read_text(
        encoding="utf-8", errors="replace"
    ):
        backup = path.with_name(path.name + ".before-smarttype-gnome")
        if not backup.exists():
            shutil.copy2(path, backup)
    path.write_text(
        """[Desktop Entry]
Type=Application
Name=Fcitx 5
Comment=Start Fcitx 5 for SmartType on GNOME
Exec=fcitx5 -d --replace
Terminal=false
NoDisplay=true
X-GNOME-Autostart-enabled=true
X-SmartType-Managed=true
""",
        encoding="utf-8",
    )


def extension_setting(key: str) -> list[str]:
    result = subprocess.run(
        ["gsettings", "get", "org.gnome.shell", key],
        check=True,
        capture_output=True,
        text=True,
    )
    raw = result.stdout.strip()
    if raw.startswith("@as "):
        raw = raw[4:]
    value = ast.literal_eval(raw)
    if not isinstance(value, list) or not all(isinstance(item, str) for item in value):
        raise ValueError(f"unexpected org.gnome.shell {key} value")
    return value


def enable_extensions(uuids: list[str]) -> None:
    if not shutil.which("gsettings"):
        raise RuntimeError("gsettings is required to enable GNOME extensions")
    enabled = extension_setting("enabled-extensions")
    disabled = extension_setting("disabled-extensions")
    enabled_changed = False
    for uuid in uuids:
        if uuid and uuid not in enabled:
            enabled.append(uuid)
            enabled_changed = True
    if enabled_changed:
        subprocess.run(
            [
                "gsettings",
                "set",
                "org.gnome.shell",
                "enabled-extensions",
                repr(enabled),
            ],
            check=True,
        )
    updated_disabled = [uuid for uuid in disabled if uuid not in uuids]
    if updated_disabled != disabled:
        subprocess.run(
            [
                "gsettings",
                "set",
                "org.gnome.shell",
                "disabled-extensions",
                repr(updated_disabled),
            ],
            check=True,
        )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("fcitx_config", type=pathlib.Path)
    parser.add_argument("config_dir", type=pathlib.Path)
    parser.add_argument("environment_file", type=pathlib.Path)
    parser.add_argument("autostart_file", type=pathlib.Path)
    parser.add_argument("--enable-session-extensions", action="store_true")
    parser.add_argument("--appindicator-uuid", default="")
    args = parser.parse_args()

    configure_fcitx(args.fcitx_config)
    configure_environment(args.environment_file)
    configure_autostart(args.autostart_file)
    if args.enable_session_extensions:
        enable_extensions([KIMPANEL_UUID, args.appindicator_uuid])


if __name__ == "__main__":
    main()
