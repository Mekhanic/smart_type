#!/usr/bin/env python3
"""Apply the small SmartType interaction overlay to pinned Kimpanel sources."""

from __future__ import annotations

import pathlib
import sys


def patch(extension_dir: pathlib.Path) -> None:
    panel_path = extension_dir / "panel.js"
    stylesheet_path = extension_dir / "stylesheet.css"
    panel = panel_path.read_text(encoding="utf-8")
    old = """            if (label[i].length == 0)
                lookupTable[i].ignore_focus = true;
            else
                lookupTable[i].ignore_focus = false;
"""
    new = """            // Candidate labels are optional presentation. SmartType intentionally
            // uses clean, unnumbered candidates; they must remain reactive.
            lookupTable[i].ignore_focus = false;
"""
    if old in panel:
        panel = panel.replace(old, new, 1)
    elif new not in panel:
        raise RuntimeError("Pinned Kimpanel candidate-focus block changed unexpectedly")
    panel_path.write_text(panel, encoding="utf-8")

    marker = "/* SmartType candidate interaction overlay */"
    overlay = f"""{marker}
.kimpanel-candidate-item {{
  border-radius: 12px;
}}

.kimpanel-candidate-item:hover {{
  background-color: rgba(128, 128, 128, 0.22);
}}

.kimpanel-candidate-item:active,
.kimpanel-candidate-item:active:hover {{
  background-color: rgba(128, 128, 128, 0.34);
}}
"""
    stylesheet = stylesheet_path.read_text(encoding="utf-8")
    if marker in stylesheet:
        stylesheet = stylesheet[: stylesheet.index(marker)].rstrip() + "\n\n" + overlay
    else:
        stylesheet = stylesheet.rstrip() + "\n\n" + overlay
    stylesheet_path.write_text(stylesheet, encoding="utf-8")


def main() -> int:
    if len(sys.argv) != 2:
        print("Usage: patch-kimpanel.py EXTENSION_DIR", file=sys.stderr)
        return 2
    patch(pathlib.Path(sys.argv[1]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
