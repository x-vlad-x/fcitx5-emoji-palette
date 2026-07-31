# Manual test matrix

No row may be marked passed without execution on the target desktop.

| Scenario | Result | Evidence |
| --- | --- | --- |
| Kate or KWrite, native Wayland | Passed | 2026-07-30, VirtualBox VM Fedora 44 KDE Plasma 6.7.3 Wayland, 0.1.0~rc2-6: Russian, English, and German search consumed with no document leak; U+1F498 and U+1F525 commits verified byte-exact on disk |
| Konsole | Passed | 2026-07-30, VirtualBox VM Fedora 44 KDE Plasma 6.7.3 Wayland, 0.1.0~rc2-6: no keystroke reached the shell while the picker was active; committed U+1F525 appeared at the prompt |
| Firefox, native Wayland | Passed | 2026-07-30, VirtualBox VM Fedora 44 KDE Plasma 6.7.3 Wayland, 0.1.0~rc2-6: search consumed, U+2B50 committed into a focused textarea; first-activation surface placement race noted in issue tracker |
| Chromium or Brave, native Wayland | Not run | |
| Electron application | Not run | |
| GTK application | Blocked | 2026-07-30, VirtualBox VM Fedora 44 KDE Plasma 6.7.3 Wayland, 0.1.0~rc2-6: Meta-based trigger chords never reach the addon from GNOME Text Editor, so no transaction starts; tracked as a follow-up activation issue, no isolation path exists |
| XWayland application using XIM | Not run | |
| Flatpak application configured for Fcitx5 | Not run | |
| Mixed English, German, and Russian text | Passed | 2026-07-30, VirtualBox VM Fedora 44 KDE Plasma 6.7.3 Wayland, 0.1.0~rc2-6: queries typed on us, ru, and de layouts, including umlauts, all captured as search text |
| Activation shortcut across English and Russian layouts | Passed | 2026-07-30, VirtualBox VM Fedora 44 KDE Plasma 6.7.3 Wayland, 0.1.0~rc2-6: Super+Z activated exactly once per press on both layouts with no leaked trigger character |
| Multiple monitors | Not run | |
| Fractional scaling | Not run | |
| Every screen edge and corner | Not run | |
| Rapid repeated open and close | Passed | 2026-07-30, VirtualBox VM Fedora 44 KDE Plasma 6.7.3 Wayland, 0.1.0~rc2-6: 10 trigger/Escape cycles left one helper process and the same Fcitx5 PID |
| Source application closes while open | Not run | |
| Fcitx5 restarts while helper runs | Passed | 2026-07-30, VirtualBox VM Fedora 44 KDE Plasma 6.7.3 Wayland, 0.1.0~rc2-6: addon reloaded once after restart; picker usable after re-trigger |
| Emoji without a glyph in the installed font | Not run | |
| ZWJ sequence whose joiner element has no glyph | Not run | |
| Placeholder tiles in light and dark Plasma themes | Not run | |
| Helper crash and automatic recovery | Passed | 2026-07-30, VirtualBox VM Fedora 44 KDE Plasma 6.7.3 Wayland, 0.1.0~rc2-6: killed helper was reactivated over D-Bus; Fcitx5 stayed up; keys stayed isolated during the recovery window |

## Required invariants

For each applicable scenario, record evidence that:

- clipboard content remains byte-for-byte unchanged;
- no clipboard or synthetic-input helper process is invoked;
- search text does not appear in the source application;
- one physical shortcut press activates the palette exactly once before and
  after a live keyboard-layout change;
- selection is never committed to a different window;
- the popup does not take keyboard focus;
- Fcitx5 remains operational after the helper crashes;
- an emoji the installed font cannot draw shows a labelled placeholder
  rather than a missing-glyph box, and still commits its exact original
  Unicode sequence.
