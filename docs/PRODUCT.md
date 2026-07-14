# SmartType Product Contract

SmartType is a system-wide autocorrector for Linux.

## Required functional parity

The following behavior must work identically in Chrome, Telegram and Kate/KWrite:

1. Characters are never duplicated or lost.
2. Autocorrection produces the same corrected word.
3. Suggestions contain the same candidates.
4. Arrow keys and Tab select candidates.
5. Backspace immediately after correction restores the original word.
6. Case and keyboard layout are preserved.
7. Punctuation and spaces behave consistently.
8. Personal learning gives the same decisions.
9. Terminals and blacklisted applications are not modified.
10. Active composition is displayed in the target application's text field.
    The candidate popup is never used as a separate text editor.

## Visual behavior

Different frontends may use different renderers.

Visual differences are acceptable when core behavior is identical:

- native Wayland may use animated SmartTypeUI;
- DBus client-side UI may initially remain static;
- inline correction flash may only exist where formatted preedit is supported.

Exact visual parity requires owning and modifying every renderer and is a separate future project.
