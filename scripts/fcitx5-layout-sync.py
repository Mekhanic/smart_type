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


def kde_layout_codes():
    """Return KDE layout codes in index order, or an empty list on failure."""
    try:
        output = command("busctl", "--user", "call", "org.kde.keyboard", "/Layouts",
                         "org.kde.KeyboardLayouts", "getLayoutsList").stdout
        return re.findall(r'"([^"\\]*)"\s+"[^"\\]*"\s+"[^"\\]*"', output)
    except (OSError, subprocess.SubprocessError):
        return []


def kde_index_targets():
    """Use KDE as owner only for the index contract the engine can represent."""
    codes = kde_layout_codes()
    if len(codes) >= 2 and codes[0] == "us" and codes[1] == "ru":
        return TARGETS
    return {}


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


def select_input_method(layout, source, targets=TARGETS):
    requested = targets.get(layout)
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


def activate_startup_default(source):
    """Replace Fcitx's cold-start keyboard fallback exactly once.

    ActiveByDefault is not sufficient on every frontend: Fcitx may keep
    keyboard-us until the first explicit activation even though DefaultIM is
    smarttype-us. Only repair that startup fallback. Any real input method
    (including smarttype Russian or a third-party method) is left untouched.
    """
    current = current_input_method()
    if not current:
        return False
    if current != "keyboard-us":
        return True
    select_input_method(0, source)
    return current_input_method() == "smarttype-us"


def activate_x11_default():
    return activate_startup_default("x11-startup-default")


def activate_kde_default():
    return activate_startup_default("kde-startup-default")


def reconcile_forever():
    while True:
        layout = current_kde_layout()
        targets = kde_index_targets()
        if layout is not None and targets:
            select_input_method(layout, "periodic-reconcile", targets)
        time.sleep(2)


def monitor():
    session_type = os.environ.get("XDG_SESSION_TYPE", "").lower()
    desktop = os.environ.get("XDG_CURRENT_DESKTOP", "").lower()
    if session_type == "x11" or (os.environ.get("DISPLAY") and "kde" not in desktop):
        startup_default_activated = False
        while True:
            clear_x11_group_options()
            if not startup_default_activated:
                startup_default_activated = activate_x11_default()
            time.sleep(2 if startup_default_activated else 0.25)

    # With a single/custom KDE layout, KWin cannot represent SmartType's
    # logical RU method and therefore is not the owner. Fcitx owns Alt+Shift in
    # this mode, but may still cold-start on its technical keyboard-us item.
    # Repair that fallback once before listening for any later KDE changes.
    if "kde" in desktop or "plasma" in desktop:
        while not kde_index_targets() and not activate_kde_default():
            time.sleep(0.25)

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
                    targets = kde_index_targets()
                    if targets:
                        select_input_method(int(match.group(1)), "kde-layoutChanged", targets)
        except (OSError, subprocess.SubprocessError):
            time.sleep(1)
        finally:
            if process:
                process.terminate()
            time.sleep(1)


if __name__ == "__main__":
    time.sleep(1)
    monitor()
