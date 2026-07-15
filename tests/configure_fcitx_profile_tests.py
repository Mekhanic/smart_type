#!/usr/bin/env python3

import pathlib
import subprocess
import sys
import tempfile
import importlib.util
from unittest import mock


def run(configurator: pathlib.Path, profile: pathlib.Path) -> None:
    subprocess.run([sys.executable, configurator, profile], check=True)


def main() -> None:
    configurator = pathlib.Path(sys.argv[1])
    with tempfile.TemporaryDirectory() as directory:
        profile = pathlib.Path(directory) / "fcitx5" / "profile"
        profile.parent.mkdir(parents=True)
        profile.write_text(
            "[Groups/0]\nName=Default\nDefault Layout=us\nDefaultIM=keyboard-us\n\n"
            "[Groups/0/Items/0]\nName=mozc\nLayout=\n\n"
            "[Groups/0/Items/7]\nName=keyboard-us\nLayout=\n\n"
            "[GroupOrder]\n0=Default\n",
            encoding="utf-8",
        )

        run(configurator, profile)
        first = profile.read_text(encoding="utf-8")
        assert "DefaultIM=smarttype-us" in first
        assert first.splitlines().count("Name=smarttype-us") == 1
        assert first.splitlines().count("Name=smarttype") == 1
        assert "Name=keyboard-us" in first
        assert "Name=mozc" in first
        assert (profile.parent / "profile.before-smarttype").exists()
        ordered_names = [line for line in first.splitlines() if line.startswith("Name=")]
        assert ordered_names.index("Name=keyboard-us") < ordered_names.index("Name=smarttype-us")
        assert ordered_names.index("Name=smarttype-us") < ordered_names.index("Name=smarttype")
        assert ordered_names.index("Name=smarttype") < ordered_names.index("Name=mozc")
        assert "[Groups/0/Items/0]\nName=keyboard-us" in first

        spec = importlib.util.spec_from_file_location("profile_config", configurator)
        assert spec and spec.loader
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        parsed = first.splitlines()
        assert module.group_value(parsed, "Name") == "Default"
        assert module.group_value(parsed, "Default Layout") == "us"
        assert module.ordered_items(parsed) == [
            ("keyboard-us", ""),
            ("smarttype-us", ""),
            ("smarttype", ""),
            ("mozc", ""),
        ]

        owner = subprocess.CompletedProcess([], 0, "b true\n", "")
        with mock.patch.object(module.shutil, "which", return_value="/usr/bin/busctl"), \
             mock.patch.object(
                 module.subprocess,
                 "run",
                 side_effect=[owner, subprocess.CalledProcessError(1, ["busctl"])],
             ):
            assert module.apply_live(parsed) is False

        run(configurator, profile)
        assert profile.read_text(encoding="utf-8") == first

        empty = pathlib.Path(directory) / "empty" / "profile"
        run(configurator, empty)
        generated = empty.read_text(encoding="utf-8")
        assert "Name=keyboard-us" in generated
        assert "Name=smarttype-us" in generated
        assert "Name=smarttype" in generated


if __name__ == "__main__":
    main()
