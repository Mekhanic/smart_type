#!/usr/bin/env python3

import importlib.util
import pathlib
import types
import sys


def main() -> None:
    script = pathlib.Path(sys.argv[1])
    spec = importlib.util.spec_from_file_location("smarttype_layout_sync", script)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)

    calls = []

    def fake_command(*args):
        calls.append(args)
        if args == ("setxkbmap", "-query"):
            return types.SimpleNamespace(
                stdout="rules: evdev\nlayout: us,ru\n"
                       "options: caps:escape,grp:alt_shift_toggle,grp_led:scroll\n")
        return types.SimpleNamespace(stdout="")

    module.command = fake_command
    module.current_input_method = lambda: "smarttype"
    module.record = lambda *args: calls.append(("record",) + args)

    group, keep = module.x11_group_options()
    assert group == ["grp:alt_shift_toggle", "grp_led:scroll"]
    assert keep == ["caps:escape"]
    module.clear_x11_group_options()
    assert ("setxkbmap", "-option", "") in calls
    assert ("setxkbmap", "-option", "caps:escape") in calls
    assert not any("grp:alt_shift_toggle" in call for call in calls[1:]
                   if call and call[0] == "setxkbmap")

    state = {"current": "keyboard-us"}

    def fake_select(layout, source):
        calls.append(("select", layout, source))
        state["current"] = "smarttype-us"

    module.current_input_method = lambda: state["current"]
    module.select_input_method = fake_select
    assert module.activate_x11_default()
    assert ("select", 0, "x11-startup-default") in calls

    calls.clear()
    state["current"] = "smarttype"
    assert module.activate_x11_default()
    assert not any(call and call[0] == "select" for call in calls)

    # Exact clean-KDE P0: a single US layout cannot represent Russian. KDE
    # index 0 must therefore not be allowed to snap an explicit smarttype IM
    # back to smarttype-us every two seconds.
    module.command = lambda *args: types.SimpleNamespace(
        stdout='a(sss) 1 "us" "" "English (US)"\n'
    )
    assert module.kde_layout_codes() == ["us"]
    assert module.kde_index_targets() == {}

    # A clean KDE profile has no RU index to drive the bridge. Fcitx must own
    # the two SmartType methods, but it may cold-start on its technical
    # keyboard-us fallback despite DefaultIM=smarttype-us. Repair that startup
    # state once; otherwise SmartType, correction and candidates are all dead.
    calls.clear()
    state["current"] = "keyboard-us"
    module.current_input_method = lambda: state["current"]
    module.select_input_method = fake_select
    assert module.activate_kde_default()
    assert ("select", 0, "kde-startup-default") in calls

    calls.clear()
    state["current"] = "smarttype"
    assert module.activate_kde_default()
    assert not any(call and call[0] == "select" for call in calls)

    module.command = lambda *args: types.SimpleNamespace(
        stdout='a(sss) 2 "us" "" "English (US)" "ru" "" "Russian"\n'
    )
    assert module.kde_layout_codes() == ["us", "ru"]
    assert module.kde_index_targets() == module.TARGETS

if __name__ == "__main__":
    main()
