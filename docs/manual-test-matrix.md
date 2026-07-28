# Manual test matrix

No row may be marked passed without execution on the target desktop.

| Scenario | Result | Evidence |
| --- | --- | --- |
| Kate or KWrite, native Wayland | Not run | |
| Konsole | Not run | |
| Firefox, native Wayland | Not run | |
| Chromium or Brave, native Wayland | Not run | |
| Electron application | Not run | |
| GTK application | Not run | |
| XWayland application using XIM | Not run | |
| Flatpak application configured for Fcitx5 | Not run | |
| Mixed English, German, and Russian text | Not run | |
| Activation shortcut across English and Russian layouts | Not run | |
| Multiple monitors | Not run | |
| Fractional scaling | Not run | |
| Every screen edge and corner | Not run | |
| Rapid repeated open and close | Not run | |
| Source application closes while open | Not run | |
| Fcitx5 restarts while helper runs | Not run | |
| Helper crash and automatic recovery | Not run | |

## Required invariants

For each applicable scenario, record evidence that:

- clipboard content remains byte-for-byte unchanged;
- no clipboard or synthetic-input helper process is invoked;
- search text does not appear in the source application;
- one physical shortcut press activates the palette exactly once before and
  after a live keyboard-layout change;
- selection is never committed to a different window;
- the popup does not take keyboard focus;
- Fcitx5 remains operational after the helper crashes.
