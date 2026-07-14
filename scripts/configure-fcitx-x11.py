#!/usr/bin/env python3
"""Configure Fcitx and toolkit IM modules for the SmartType X11 session."""

import configparser
import pathlib
import re
import sys


SETTINGS = {
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


def configure(path: pathlib.Path) -> None:
    parser = configparser.RawConfigParser(interpolation=None, strict=False)
    parser.optionxform = str
    if path.exists():
        parser.read(path, encoding="utf-8")

    for section, values in SETTINGS.items():
        if not parser.has_section(section):
            parser.add_section(section)
        for key, value in values.items():
            parser.set(section, key, value)

    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as stream:
        parser.write(stream, space_around_delimiters=False)


def configure_environment(path: pathlib.Path) -> None:
    """Select the Fcitx GTK/Qt modules so preedit stays inside app fields."""
    managed = {
        "GTK_IM_MODULE": "fcitx",
        "QT_IM_MODULE": "fcitx",
        "XMODIFIERS": "@im=fcitx",
    }
    preserved: list[str] = []
    if path.exists():
        for line in path.read_text(encoding="utf-8").splitlines():
            key = line.split("=", 1)[0].strip() if "=" in line else ""
            if key not in managed:
                preserved.append(line)
    while preserved and not preserved[-1]:
        preserved.pop()
    preserved.extend(f"{key}={value}" for key, value in managed.items())
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(preserved) + "\n", encoding="utf-8")


def configure_xprofile(path: pathlib.Path) -> None:
    """Persist toolkit IM selection in LightDM/Xfce's X11 login environment."""
    begin = "# >>> SmartType X11 input method >>>"
    end = "# <<< SmartType X11 input method <<<"
    managed_assignment = re.compile(
        r"^\s*(?:export\s+)?(?:GTK_IM_MODULE|QT_IM_MODULE|XMODIFIERS)\s*="
    )
    lines = path.read_text(encoding="utf-8").splitlines() if path.exists() else []
    kept: list[str] = []
    in_managed_block = False
    for line in lines:
        if line.strip() == begin:
            in_managed_block = True
            continue
        if line.strip() == end:
            in_managed_block = False
            continue
        if not in_managed_block and not managed_assignment.match(line):
            kept.append(line)
    while kept and not kept[-1]:
        kept.pop()
    if kept:
        kept.append("")
    kept.extend(
        [
            begin,
            "export GTK_IM_MODULE=fcitx",
            "export QT_IM_MODULE=fcitx",
            "export XMODIFIERS=@im=fcitx",
            end,
        ]
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(kept) + "\n", encoding="utf-8")


if __name__ == "__main__":
    if len(sys.argv) not in (2, 3, 4):
        raise SystemExit(
            f"Usage: {sys.argv[0]} PATH_TO_FCITX_CONFIG "
            "[PATH_TO_ENVIRONMENT_DROPIN [PATH_TO_XPROFILE]]"
        )
    configure(pathlib.Path(sys.argv[1]))
    if len(sys.argv) == 3:
        configure_environment(pathlib.Path(sys.argv[2]))
    elif len(sys.argv) == 4:
        configure_environment(pathlib.Path(sys.argv[2]))
        configure_xprofile(pathlib.Path(sys.argv[3]))
