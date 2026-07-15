#!/usr/bin/env python3
"""Add SmartType input methods to the current Fcitx group without deleting others."""

from __future__ import annotations

import pathlib
import re
import shutil
import subprocess
import sys


GROUP = "Groups/0"
REQUIRED_INPUT_METHODS = ("smarttype-us", "smarttype")


def section_bounds(lines: list[str], section: str) -> tuple[int, int] | None:
    header = f"[{section}]"
    try:
        start = lines.index(header)
    except ValueError:
        return None
    end = next(
        (index for index in range(start + 1, len(lines)) if lines[index].startswith("[")),
        len(lines),
    )
    return start, end


def set_group_value(lines: list[str], key: str, value: str) -> None:
    bounds = section_bounds(lines, GROUP)
    if bounds is None:
        lines[:0] = [f"[{GROUP}]", "Name=Default", "Default Layout=us", f"{key}={value}", ""]
        return
    start, end = bounds
    prefix = f"{key}="
    for index in range(start + 1, end):
        if lines[index].startswith(prefix):
            lines[index] = f"{key}={value}"
            return
    lines.insert(end, f"{key}={value}")


def item_names(lines: list[str]) -> tuple[set[str], int]:
    names: set[str] = set()
    maximum = -1
    item_header = re.compile(r"^\[Groups/0/Items/(\d+)\]$")
    for index, line in enumerate(lines):
        match = item_header.match(line)
        if not match:
            continue
        maximum = max(maximum, int(match.group(1)))
        end = next(
            (cursor for cursor in range(index + 1, len(lines)) if lines[cursor].startswith("[")),
            len(lines),
        )
        for cursor in range(index + 1, end):
            if lines[cursor].startswith("Name="):
                names.add(lines[cursor].split("=", 1)[1])
                break
    return names, maximum


def reorder_primary_items(lines: list[str]) -> None:
    """Keep every group-0 item, but make keyboard-us the skipped first item."""
    item_header = re.compile(r"^\[Groups/0/Items/(\d+)\]$")
    sections: list[tuple[int, int, str, list[str]]] = []
    for start, line in enumerate(lines):
        if not item_header.match(line):
            continue
        end = next(
            (cursor for cursor in range(start + 1, len(lines)) if lines[cursor].startswith("[")),
            len(lines),
        )
        name = ""
        for cursor in range(start + 1, end):
            if lines[cursor].startswith("Name="):
                name = lines[cursor].split("=", 1)[1]
                break
        sections.append((start, end, name, lines[start:end]))

    if not sections or "keyboard-us" not in {section[2] for section in sections}:
        return

    for start, end, _name, _block in reversed(sections):
        del lines[start:end]

    priority = {"keyboard-us": 0, "smarttype-us": 1, "smarttype": 2}
    ordered = sorted(
        enumerate(sections),
        key=lambda entry: (priority.get(entry[1][2], 3), entry[0]),
    )
    insertion = next(
        (index for index, line in enumerate(lines) if line == "[GroupOrder]"),
        len(lines),
    )
    rebuilt: list[str] = []
    for new_index, (_old_index, (_start, _end, _name, block)) in enumerate(ordered):
        block[0] = f"[Groups/0/Items/{new_index}]"
        rebuilt.extend(block)
        if rebuilt and rebuilt[-1]:
            rebuilt.append("")
    lines[insertion:insertion] = rebuilt


def group_value(lines: list[str], key: str, default: str = "") -> str:
    bounds = section_bounds(lines, GROUP)
    if bounds is None:
        return default
    start, end = bounds
    prefix = f"{key}="
    for line in lines[start + 1 : end]:
        if line.startswith(prefix):
            return line.split("=", 1)[1]
    return default


def ordered_items(lines: list[str]) -> list[tuple[str, str]]:
    result: list[tuple[str, str]] = []
    item_header = re.compile(r"^\[Groups/0/Items/(\d+)\]$")
    for start, line in enumerate(lines):
        if not item_header.match(line):
            continue
        end = next(
            (cursor for cursor in range(start + 1, len(lines)) if lines[cursor].startswith("[")),
            len(lines),
        )
        values = {"Name": "", "Layout": ""}
        for item_line in lines[start + 1 : end]:
            for key in values:
                if item_line.startswith(f"{key}="):
                    values[key] = item_line.split("=", 1)[1]
        if values["Name"]:
            result.append((values["Name"], values["Layout"]))
    return result


def apply_live(lines: list[str]) -> bool:
    """Apply the same group through Fcitx D-Bus so it cannot overwrite the file."""
    if not shutil.which("busctl"):
        return False
    owner = subprocess.run(
        [
            "busctl",
            "--user",
            "call",
            "org.freedesktop.DBus",
            "/org/freedesktop/DBus",
            "org.freedesktop.DBus",
            "NameHasOwner",
            "s",
            "org.fcitx.Fcitx5",
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    if owner.returncode != 0 or owner.stdout.strip() != "b true":
        return False

    items = ordered_items(lines)
    if not items:
        return False
    group_name = group_value(lines, "Name", "Default")
    default_layout = group_value(lines, "Default Layout", "us")
    command = [
        "busctl",
        "--user",
        "call",
        "org.fcitx.Fcitx5",
        "/controller",
        "org.fcitx.Fcitx.Controller1",
        "SetInputMethodGroupInfo",
        "ssa(ss)",
        group_name,
        default_layout,
        str(len(items)),
    ]
    for name, layout in items:
        command.extend([name, layout])
    try:
        subprocess.run(command, check=True)
        subprocess.run(
            [
                "busctl",
                "--user",
                "call",
                "org.fcitx.Fcitx5",
                "/controller",
                "org.fcitx.Fcitx.Controller1",
                "Save",
            ],
            check=True,
        )
    except subprocess.CalledProcessError:
        # A package upgrade may race with the old Fcitx process exiting. The
        # profile is already durable on disk, so the next Fcitx start will use
        # it; installation must not fail merely because the live reload became
        # unavailable between NameHasOwner and SetInputMethodGroupInfo.
        return False
    return True


def configure(path: pathlib.Path) -> None:
    original = path.read_text(encoding="utf-8") if path.exists() else ""
    lines = original.splitlines()

    if not lines:
        lines.extend(
            [
                f"[{GROUP}]",
                "Name=Default",
                "Default Layout=us",
                "DefaultIM=smarttype-us",
                "",
                "[Groups/0/Items/0]",
                "Name=keyboard-us",
                "Layout=",
                "",
                "[GroupOrder]",
                "0=Default",
            ]
        )

    set_group_value(lines, "DefaultIM", "smarttype-us")
    names, maximum = item_names(lines)
    for name in REQUIRED_INPUT_METHODS:
        if name in names:
            continue
        maximum += 1
        if lines and lines[-1]:
            lines.append("")
        lines.extend(
            [
                f"[Groups/0/Items/{maximum}]",
                f"Name={name}",
                "Layout=",
            ]
        )

    reorder_primary_items(lines)

    updated = "\n".join(lines).rstrip() + "\n"
    if updated == original:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        backup = path.with_name(path.name + ".before-smarttype")
        if not backup.exists():
            shutil.copy2(path, backup)
    path.write_text(updated, encoding="utf-8")


if __name__ == "__main__":
    args = sys.argv[1:]
    apply_running = False
    if "--apply-live" in args:
        args.remove("--apply-live")
        apply_running = True
    if len(args) != 1:
        raise SystemExit(f"Usage: {sys.argv[0]} PATH_TO_FCITX_PROFILE [--apply-live]")
    profile_path = pathlib.Path(args[0])
    configure(profile_path)
    if apply_running:
        apply_live(profile_path.read_text(encoding="utf-8").splitlines())
