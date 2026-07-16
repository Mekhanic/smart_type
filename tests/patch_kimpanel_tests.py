#!/usr/bin/env python3

from __future__ import annotations

import pathlib
import subprocess
import sys
import tempfile


def main() -> int:
    patcher = pathlib.Path(sys.argv[1])
    with tempfile.TemporaryDirectory() as directory:
        root = pathlib.Path(directory)
        (root / "panel.js").write_text(
            """        for (let i = 0; i < lookupTable.length; i++) {
            if (label[i].length == 0)
                lookupTable[i].ignore_focus = true;
            else
                lookupTable[i].ignore_focus = false;
            lookupTable[i].candidate_index = i;
        }
""",
            encoding="utf-8",
        )
        (root / "stylesheet.css").write_text(
            ".kimpanel-candidate-item { cursor: pointer; }\n", encoding="utf-8"
        )

        subprocess.run([sys.executable, patcher, root], check=True)
        subprocess.run([sys.executable, patcher, root], check=True)

        panel = (root / "panel.js").read_text(encoding="utf-8")
        css = (root / "stylesheet.css").read_text(encoding="utf-8")
        assert "lookupTable[i].ignore_focus = false;" in panel
        assert "label[i].length == 0" not in panel
        assert css.count("SmartType candidate interaction overlay") == 1
        assert ".kimpanel-candidate-item:hover" in css
        assert "rgba(128, 128, 128, 0.22)" in css
        assert "rgba(255, 255, 255" not in css
    print("Kimpanel interaction patch test passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
