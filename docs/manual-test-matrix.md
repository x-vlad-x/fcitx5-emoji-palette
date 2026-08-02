# Manual test matrix

No row may be marked passed without execution on the target desktop.

| Scenario | Result | Evidence |
| --- | --- | --- |
| Kate or KWrite, native Wayland | Passed | 2026-08-02, VirtualBox VM Fedora 44 KDE Plasma 6.7.3 Wayland, 0.1.0~rc2-13: picker centered on the active output, search consumed, U+1F525 committed directly and the Klipper sentinel stayed unchanged; 2026-07-30, VirtualBox VM Fedora 44 KDE Plasma 6.7.3 Wayland, 0.1.0~rc2-6: Russian, English, and German search consumed with no document leak; U+1F498 and U+1F525 commits verified byte-exact on disk |
| Konsole | Passed | 2026-08-02, VirtualBox VM Fedora 44 KDE Plasma 6.7.3 Wayland, 0.1.0~rc2-13: picker centered on the active output for this native Wayland client; 2026-07-30, VirtualBox VM Fedora 44 KDE Plasma 6.7.3 Wayland, 0.1.0~rc2-6: no keystroke reached the shell while the picker was active; committed U+1F525 appeared at the prompt |
| Firefox, native Wayland | Passed | 2026-07-30, VirtualBox VM Fedora 44 KDE Plasma 6.7.3 Wayland, 0.1.0~rc2-6: search consumed, U+2B50 committed into a focused textarea; first-activation surface placement race noted in issue tracker |
| Chromium or Brave, native Wayland | Not run | |
| Electron application | Not run | |
| GTK application | Blocked | 2026-07-30, VirtualBox VM Fedora 44 KDE Plasma 6.7.3 Wayland, 0.1.0~rc2-6: Meta-based trigger chords never reach the addon from GNOME Text Editor, so no transaction starts; tracked as a follow-up activation issue, no isolation path exists |
| XWayland application using XIM | Not run | |
| Flatpak application configured for Fcitx5 | Not run | |
| Mixed English, German, and Russian text | Passed | 2026-07-30, VirtualBox VM Fedora 44 KDE Plasma 6.7.3 Wayland, 0.1.0~rc2-6: queries typed on us, ru, and de layouts, including umlauts, all captured as search text |
| Activation shortcut across English and Russian layouts | Passed | 2026-07-30, VirtualBox VM Fedora 44 KDE Plasma 6.7.3 Wayland, 0.1.0~rc2-6: Super+Z activated exactly once per press on both layouts with no leaked trigger character |
| Caret-relative placement, client reporting an absolute caret | Passed | 2026-08-02, VirtualBox VM Fedora 44 KDE Plasma 6.7.3 Wayland, 0.1.0~rc2-13: KWrite under XWayland with `QT_IM_MODULE=fcitx`; addon logged `frontend=dbus rawCaret=590,281,2,26 relativeRect=0 caretUsable=1` and the picker opened directly below the caret; re-verified on 0.1.0~rc2-15 after the layout-measurement fix |
| Documented fallback, native Wayland client | Passed | 2026-08-02, VirtualBox VM Fedora 44 KDE Plasma 6.7.3 Wayland, 0.1.0~rc2-13: KWrite, Konsole; addon logged `frontend=wayland rawCaret=0,0,0,0 caretUsable=0` and the picker was centered on the active output instead of pinned near the top of the desktop |
| Multiple monitors | Blocked | 2026-08-02, VirtualBox VM Fedora 44 KDE Plasma 6.7.3 Wayland, 0.1.0~rc2-13: the headless VirtualBox VM never connects a second virtual output. `setscreenlayout` fails with NS_ERROR_INVALID_ARG and `setvideomodehint` leaves `card0-Virtual-2` disconnected, so no second output reaches KWin. Output selection is covered deterministically by `core-tests` over three outputs, negative origins, gaps between outputs and carets beyond every output |
| Fractional scaling | Passed | 2026-08-02, VirtualBox VM Fedora 44 KDE Plasma 6.7.3 Wayland, 0.1.0~rc2-13: output scale 1.5 (1920x1080 physical, 1280x720 logical). XWayland caret 590,281 converted to logical 393,187 and the picker placed at 393,204, directly below the caret. The first activation on the output used the documented centered fallback because Wayland reports a fractional scale only after a surface has been mapped there; every later activation was caret-relative. At scale 1.0 the first activation was already caret-relative |
| Every screen edge and corner | Partly passed | 2026-08-02, VirtualBox VM Fedora 44 KDE Plasma 6.7.3 Wayland, 0.1.0~rc2-13: caret near the top placed the picker below it, caret on the last visible line flipped it above, and a caret past the right-hand limit clamped it to the right edge of the usable area. The remaining corners are covered deterministically by `core-tests` |
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
