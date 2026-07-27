# Emoji Palette for Fcitx 5

Emoji Palette for Fcitx 5 is a native emoji picker for Linux desktops. It is
being designed for direct text insertion through Fcitx5 without changing the
clipboard or simulating keyboard input.

The initial target is Bazzite 44 and Fedora 44 with KDE Plasma 6 on Wayland.

Development is in an early prerelease stage. The dependency-free core model is
implemented; desktop runtime integration is still in progress.

## Development build

The core library and tests require CMake, Ninja, a C++20 compiler, and Python 3:

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Normal builds use committed generated Unicode data and do not access the
network. See `docs/unicode-data.md` for the explicit update process.

## License

Project code is licensed under GPL-3.0-or-later.
