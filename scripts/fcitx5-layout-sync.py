#!/usr/bin/env python3
"""Keep the desktop layout owner and SmartType's Fcitx IM in sync."""

import datetime
import json
import os
import pathlib
import re
import subprocess
import threading
import time

TARGETS = {0: "smarttype-us", 1: "smarttype"}
LOCK = threading.Lock()
STATE_DIR = pathlib.Path(os.environ.get("XDG_STATE_HOME", pathlib.Path.home() / ".local/state")) / "smarttype"
LOG_PATH = STATE_DIR / "layout-events.jsonl"


def command(*args):
    return subprocess.run(args, text=True, capture_output=True, timeout=2)


def current_kde_layout():
    try:
        output = command("busctl", "--user", "call", "org.kde.keyboard", "/Layouts",
                         "org.kde.KeyboardLayouts", "getLayout").stdout
        match = re.search(r"\bu\s+(\d+)", output)
        return int(match.group(1)) if match else None
    except (OSError, subprocess.SubprocessError):
        return None


def x11_group_options():
    """Return XKB group-switch options that conflict with Fcitx Alt+Shift."""
    try:
        output = command("setxkbmap", "-query").stdout
    except (OSError, subprocess.SubprocessError):
        return [], []
    match = re.search(r"^options:\s*(.*)$", output, re.MULTILINE)
    options = [item.strip() for item in match.group(1).split(",") if item.strip()] \
        if match else []
    group = [item for item in options
             if item.startswith("grp:") or item.startswith("grp_led:")]
    keep = [item for item in options if item not in group]
    return group, keep


def clear_x11_group_options():
    group, keep = x11_group_options()
    if not group:
        return
    command("setxkbmap", "-option", "")
    for option in keep:
        command("setxkbmap", "-option", option)
    current_im = current_input_method()
    record("x11-clear-group-options", None, current_im,
           ",".join(group), current_im)


def fcitx_has_dbus_owner():
    """Check availability without activating Fcitx through D-Bus.

    On KDE Wayland, KWin must be the process that starts Fcitx so it can pass
    the one-shot input-method socket. Calling fcitx5-remote before KWin does
    that would D-Bus-activate an unusable second instance.
    """
    try:
        output = command(
            "busctl", "--user", "call", "org.freedesktop.DBus",
            "/org/freedesktop/DBus", "org.freedesktop.DBus", "NameHasOwner",
            "s", "org.fcitx.Fcitx5").stdout
        return output.strip() == "b true"
    except (OSError, subprocess.SubprocessError):
        return False


def current_input_method():
    if not fcitx_has_dbus_owner():
        return ""
    try:
        return command("fcitx5-remote", "-n").stdout.strip()
    except (OSError, subprocess.SubprocessError):
        return ""


def record(source, layout, requested, before, after):
    STATE_DIR.mkdir(parents=True, exist_ok=True)
    entry = {
        "time": datetime.datetime.now(datetime.timezone.utc).astimezone().isoformat(),
        "source": source,
        "kde_layout": layout,
        "requested": requested,
        "before": before,
        "after": after,
        "ok": after == requested,
    }
    with LOG_PATH.open("a", encoding="utf-8") as stream:
        stream.write(json.dumps(entry, ensure_ascii=False) + "\n")


def select_input_method(layout, source):
    requested = TARGETS.get(layout)
    if not requested or not fcitx_has_dbus_owner():
        return
    with LOCK:
        before = current_input_method()
        if before == requested:
            return
        after = before
        for _ in range(6):
            subprocess.run(["fcitx5-remote", "-s", requested],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            after = current_input_method()
            if after == requested:
                break
            time.sleep(0.15)
        record(source, layout, requested, before, after)


def reconcile_forever():
    while True:
        layout = current_kde_layout()
        if layout is not None:
            select_input_method(layout, "periodic-reconcile")
        time.sleep(2)


def monitor():
    session_type = os.environ.get("XDG_SESSION_TYPE", "").lower()
    desktop = os.environ.get("XDG_CURRENT_DESKTOP", "").lower()
    if session_type == "x11" or (os.environ.get("DISPLAY") and "kde" not in desktop):
        while True:
            clear_x11_group_options()
            time.sleep(2)

    threading.Thread(target=reconcile_forever, daemon=True).start()
    cmd = ["dbus-monitor", "--session",
           "type='signal',interface='org.kde.KeyboardLayouts',member='layoutChanged'"]
    while True:
        process = None
        try:
            process = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                                       stderr=subprocess.DEVNULL, text=True)
            for line in process.stdout:
                match = re.search(r"uint32\s+(\d+)", line)
                if match:
                    select_input_method(int(match.group(1)), "kde-layoutChanged")
        except (OSError, subprocess.SubprocessError):
            time.sleep(1)
        finally:
            if process:
                process.terminate()
            time.sleep(1)


if __name__ == "__main__":
    time.sleep(1)
    monitor()
