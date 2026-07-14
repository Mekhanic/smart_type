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

if __name__ == "__main__":
    main()
