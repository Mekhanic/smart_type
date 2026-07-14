#!/usr/bin/env python3
"""Add SmartType input methods to the current Fcitx group without deleting others."""

from __future__ import annotations

import pathlib
import re
import shutil
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
    if len(sys.argv) != 2:
        raise SystemExit(f"Usage: {sys.argv[0]} PATH_TO_FCITX_PROFILE")
    configure(pathlib.Path(sys.argv[1]))
