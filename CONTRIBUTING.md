# Contributing to SmartType

Thank you for helping improve SmartType.

## Reporting a problem

Use the built-in tray action **Создать отчёт о проблеме…** and attach the
generated Markdown report to a GitHub issue. Do not include passwords, private
messages, tokens, or clipboard contents.

Please include the Linux distribution, desktop environment, Wayland/X11
session type, affected application, and exact text needed to reproduce the
problem.

## Development checks

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

Changes to typing behavior should include a regression test. A successful
build is not enough for input-method changes: install the result, restart the
affected SmartType/Fcitx components, and verify behavior in a real application.

## Pull requests

Keep changes focused, explain their user-visible effect, and list automated
and live checks separately. By submitting a contribution, you agree that it
may be distributed under the repository's GPL-3.0 license.
