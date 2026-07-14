#!/usr/bin/env python3

import configparser
import pathlib
import subprocess
import sys
import tempfile


def main() -> None:
    configurator = pathlib.Path(sys.argv[1])
    with tempfile.TemporaryDirectory() as directory:
        config_path = pathlib.Path(directory) / "fcitx5" / "config"
        config_path.parent.mkdir(parents=True)
        config_path.write_text(
            "[Behavior]\nShowInputMethodInformation=False\n\n"
            "[Hotkey/EnumerateForwardKeys]\n0=Super+space\n",
            encoding="utf-8",
        )
        environment_path = pathlib.Path(directory) / "environment.d" / "90-smarttype.conf"
        environment_path.parent.mkdir(parents=True)
        environment_path.write_text(
            "# Preserve the addon path and unrelated values.\n"
            "FCITX_ADDON_DIRS=/tmp/fcitx5\n"
            "GTK_IM_MODULE=xim\n"
            "QT_IM_MODULE=xim\n"
            "XMODIFIERS=@im=none\n"
            "UNRELATED=value\n",
            encoding="utf-8",
        )
        xprofile_path = pathlib.Path(directory) / ".xprofile"
        xprofile_path.write_text(
            "export FCITX_ADDON_DIRS=/tmp/fcitx5\n"
            "export XMODIFIERS=@im=fcitx\n"
            "export UNRELATED=value\n",
            encoding="utf-8",
        )

        subprocess.run(
            [
                sys.executable,
                configurator,
                config_path,
                environment_path,
                xprofile_path,
            ],
            check=True,
        )

        parser = configparser.RawConfigParser(interpolation=None)
        parser.optionxform = str
        parser.read(config_path, encoding="utf-8")
        assert parser.get("Behavior", "ShowInputMethodInformation") == "False"
        assert parser.get("Behavior", "ActiveByDefault") == "True"
        assert parser.get("Behavior", "ShareInputState") == "All"
        assert parser.get("Hotkey", "EnumerateSkipFirst") == "True"
        assert parser.get("Hotkey", "ModifierOnlyKeyTimeout") == "-1"
        assert parser.get("Hotkey/EnumerateForwardKeys", "0") == "Alt+Shift_L"
        assert parser.get("Hotkey/EnumerateForwardKeys", "1") == "Shift+Alt_L"

        environment = environment_path.read_text(encoding="utf-8")
        assert "FCITX_ADDON_DIRS=/tmp/fcitx5" in environment
        assert "UNRELATED=value" in environment
        assert "GTK_IM_MODULE=fcitx" in environment
        assert "QT_IM_MODULE=fcitx" in environment
        assert "XMODIFIERS=@im=fcitx" in environment
        assert "GTK_IM_MODULE=xim" not in environment
        assert "QT_IM_MODULE=xim" not in environment
        assert "XMODIFIERS=@im=none" not in environment

        # Re-running is idempotent and preserves unrelated login setup.
        subprocess.run(
            [
                sys.executable,
                configurator,
                config_path,
                environment_path,
                xprofile_path,
            ],
            check=True,
        )
        xprofile = xprofile_path.read_text(encoding="utf-8")
        assert "export FCITX_ADDON_DIRS=/tmp/fcitx5" in xprofile
        assert "export UNRELATED=value" in xprofile
        assert xprofile.count("export GTK_IM_MODULE=fcitx") == 1
        assert xprofile.count("export QT_IM_MODULE=fcitx") == 1
        assert xprofile.count("export XMODIFIERS=@im=fcitx") == 1
        assert xprofile.count("# >>> SmartType X11 input method >>>") == 1
        assert xprofile.count("# <<< SmartType X11 input method <<<") == 1


if __name__ == "__main__":
    main()
