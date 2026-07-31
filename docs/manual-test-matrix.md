# Manual test matrix

No row may be marked passed without execution on the target desktop.

| Scenario | Result | Evidence |
| --- | --- | --- |
| Kate or KWrite, native Wayland | Passed | 2026-07-31, VirtualBox VM Fedora 44 KDE Plasma 6.7.3 Wayland, 0.1.0~rc2-7: German, Russian, and English queries all returned their expected emoji under a ru_RU session; U+2764 U+FE0F committed with no search text in the document |
| Konsole | Passed | 2026-07-31, VirtualBox VM Fedora 44 KDE Plasma 6.7.3 Wayland, 0.1.0~rc2-7: the German query Katze opened the picker with no keystroke reaching the shell; U+1F408 appeared at the prompt |
| Firefox, native Wayland | Passed | 2026-07-30, VirtualBox VM Fedora 44 KDE Plasma 6.7.3 Wayland, 0.1.0~rc2-6: search consumed, U+2B50 committed into a focused textarea; first-activation surface placement race noted in issue tracker |
| Chromium or Brave, native Wayland | Not run | |
| Electron application | Not run | |
| GTK application | Blocked | 2026-07-30, VirtualBox VM Fedora 44 KDE Plasma 6.7.3 Wayland, 0.1.0~rc2-6: Meta-based trigger chords never reach the addon from GNOME Text Editor, so no transaction starts; tracked as a follow-up activation issue, no isolation path exists |
| XWayland application using XIM | Not run | |
| Flatpak application configured for Fcitx5 | Not run | |
| Mixed English, German, and Russian text | Passed | 2026-07-31, VirtualBox VM Fedora 44 KDE Plasma 6.7.3 Wayland, 0.1.0~rc2-7: heart/Herz/сердце, cat/Katze/кот and fire/Feuer/огонь each returned their expected emoji from a single ru_RU session; herz typed on the us and de layouts produced identical result sets |
| Activation shortcut on the German layout | Blocked | 2026-07-31, VirtualBox VM Fedora 44 KDE Plasma 6.7.3 Wayland, 0.1.0~rc2-7: the physical US-Z key inserted y instead of activating the picker; only the key producing z on the German layout activated it; tracked as issue #30 |
| Activation shortcut across English and Russian layouts | Passed | 2026-07-30, VirtualBox VM Fedora 44 KDE Plasma 6.7.3 Wayland, 0.1.0~rc2-6: Super+Z activated exactly once per press on both layouts with no leaked trigger character |
| Multiple monitors | Not run | |
| Fractional scaling | Not run | |
| Every screen edge and corner | Not run | |
| Rapid repeated open and close | Passed | 2026-07-31, VirtualBox VM Fedora 44 KDE Plasma 6.7.3 Wayland, 0.1.0~rc2-7: 10 trigger/Escape cycles left one helper process and the same Fcitx5 PID |
| Source application closes while open | Not run | |
| Fcitx5 restarts while helper runs | Passed | 2026-07-31, VirtualBox VM Fedora 44 KDE Plasma 6.7.3 Wayland, 0.1.0~rc2-7: addon reloaded after a full Fcitx5 restart; the German query returned the same results after re-trigger |
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
- an English, German, or Russian query finds its emoji without changing the
  desktop language or the active keyboard layout.
