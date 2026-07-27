# Emoji Palette for Fcitx 5

Emoji Palette for Fcitx 5 is a native, keyboard-driven emoji picker for Linux
desktops. It commits the selected text through the original Fcitx input
context, without changing the clipboard or simulating keyboard input.

The first public prerelease targets Bazzite 44 and Fedora 44 with KDE Plasma 6
on Wayland. It is not yet recommended for production use because the complete
desktop manual test matrix has not been executed.

![Palette screenshot placeholder](docs/images/screenshot-placeholder.svg)

## Features

- Search across 3,944 fully-qualified Unicode Emoji 17.0 sequences.
- English, German, and Russian CLDR 48.2 names and keywords.
- Categories, recent selections, favorites, and skin-tone variants.
- Keyboard navigation while focus remains in the source application.
- Native `InputContext::commitString()` insertion through Fcitx 5.
- Clipboard-independent operation with no synthetic input.
- Non-activating LayerShellQt palette on Wayland.
- Active-screen placement with caret-relative positioning when geometry is
  available.
- Bounded, versioned, replay-resistant D-Bus protocol between the addon and
  the crash-isolated Qt helper.
- Atomic user-state writes and recovery from a damaged state file.

## Requirements

- Fcitx 5.1 or newer
- Qt 6.6 or newer
- LayerShellQt 6.6 or newer
- Noto Color Emoji
- A Wayland compositor with `wlr-layer-shell` support for the primary path

The X11 fallback uses a non-focusable tool window, but the first prerelease is
validated primarily for KDE Plasma on Wayland.

## Install on Fedora

Download the binary RPM for your architecture from the
[latest release](https://github.com/x-vlad-x/fcitx5-emoji-palette/releases),
then install it:

```bash
sudo dnf install ./fcitx5-emoji-palette-0.1.0-rc.2-1.fc44.x86_64.rpm
fcitx5 -r
```

Log out and back in if the running Fcitx instance does not discover the addon
after a restart.

## Install on Bazzite

This addon must run inside the host Fcitx process, so a Flatpak or Distrobox
package cannot provide the required integration. Layer the downloaded RPM:

```bash
sudo rpm-ostree install ./fcitx5-emoji-palette-0.1.0-rc.2-1.fc44.x86_64.rpm
systemctl reboot
```

Local RPMs do not update automatically. Package layering can also delay or
block an operating-system update if dependencies become incompatible. See the
[Bazzite packaging guide](docs/packaging.md#bazzite) for updates, removal,
rollback, and the custom-image alternative.

## Use

1. Place the text cursor in an application supported by Fcitx 5.
2. Press `Super+.` to open the palette.
3. Type to search, use the arrow keys to move, and press `Enter` or `Space` to
   insert.
4. Press `Escape` to cancel.

Additional controls:

| Key | Action |
| --- | --- |
| `Tab` and `Shift+Tab` | Move between categories |
| `Home` and `End` | Move to the first or last result |
| `Page Up` and `Page Down` | Move by one visible page |
| `Ctrl+D` | Toggle the selected emoji as a favorite |
| `Ctrl+V` | Show variants for the selected emoji |
| `Backspace` | Remove the final search character |

Pointer selection is supported without transferring keyboard focus to the
palette. The trigger and close-after-selection behavior are configurable in
the Fcitx 5 configuration tool under **Addons → Emoji Palette**.

## Privacy and security model

The helper can request only a Unicode string associated with the active
transaction. It cannot access an Fcitx input context or commit text directly.
The addon binds each transaction to a tracked source context and cancels it on
focus loss, reset, input-method switch, context destruction, helper loss, or
protocol failure.

No network access is required at runtime. Normal builds use committed Unicode
data and do not download data during configuration or compilation.

## Build from source

Fedora 44 development dependencies:

```bash
sudo dnf install cmake ninja-build gcc-c++ python3 fcitx5-devel \
  qt6-qtbase-devel qt6-linguist qt6-qttools-devel layer-shell-qt-devel
```

Build and test:

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Install to a staging directory before a system installation:

```bash
DESTDIR="$PWD/stage" cmake --install build/debug
find "$PWD/stage" -type f -print
```

See [Contributing](CONTRIBUTING.md) for quality gates,
[Unicode data maintenance](docs/unicode-data.md) for the explicit data update
process, and [Architecture](docs/architecture.md) for runtime design.

## Status

Automated compiler, sanitizer, static-analysis, protocol, Unicode generator,
UI-session, package validation, and RPM build gates are provided in CI. The
target-desktop scenarios in the [manual test matrix](docs/manual-test-matrix.md)
remain release evidence that must be collected on real systems.

Bug reports and compatibility evidence are welcome in
[GitHub Issues](https://github.com/x-vlad-x/fcitx5-emoji-palette/issues).
Security reports must follow [SECURITY.md](SECURITY.md).

## License

Project code is licensed under GPL-3.0-or-later. Generated emoji data is
licensed under Unicode-3.0. Desktop and AppStream metadata are licensed under
CC0-1.0. See [third-party notices](THIRD_PARTY_NOTICES.md) and the complete
license texts in `LICENSES/`.
