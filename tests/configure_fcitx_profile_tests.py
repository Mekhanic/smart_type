#!/usr/bin/env python3

import pathlib
import subprocess
import sys
import tempfile


def run(configurator: pathlib.Path, profile: pathlib.Path) -> None:
    subprocess.run([sys.executable, configurator, profile], check=True)


def main() -> None:
    configurator = pathlib.Path(sys.argv[1])
    with tempfile.TemporaryDirectory() as directory:
        profile = pathlib.Path(directory) / "fcitx5" / "profile"
        profile.parent.mkdir(parents=True)
        profile.write_text(
            "[Groups/0]\nName=Default\nDefault Layout=us\nDefaultIM=keyboard-us\n\n"
            "[Groups/0/Items/0]\nName=keyboard-us\nLayout=\n\n"
            "[Groups/0/Items/7]\nName=mozc\nLayout=\n\n"
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
