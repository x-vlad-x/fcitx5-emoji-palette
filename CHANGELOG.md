# Changelog

All notable user-visible changes are documented here.

## Unreleased

- Replaced missing-glyph boxes with labelled placeholder tiles for emoji the
  installed font cannot draw, keeping the catalog complete and committing the
  exact original Unicode sequence.
- Kept the picker and its source input transaction active while the inline grid
  settings are open, including staged Escape and outside-click dismissal.
- Made configured activation shortcuts follow the physical key across keyboard
  layout changes, including shifted letters, without leaking the translated
  character to the focused application.
- Prevented the addon from terminating Fcitx5 when it loads before
  input-method groups are initialized.
- Made local test RPM output commit-specific while retaining the reviewed
  package release and clean RPM lint results.

## 0.1.0-rc.2 - 2026-07-27

- Published release assets with filenames that remain stable on GitHub.
- Added public-asset checksum verification to the release process.

## 0.1.0-rc.1 - 2026-07-27

- Added direct insertion through the original Fcitx 5 input context.
- Added a non-focusable Qt 6 and LayerShellQt palette for KDE Plasma Wayland.
- Added the official Emoji 17.0 catalog with English, German, and Russian CLDR
  48.2 search data.
- Added categories, variants, favorites, recent selections, configurable cell
  size, and keyboard navigation.
- Added a configurable `Super+.` trigger and close-after-selection option.
- Added caret-aware active-screen placement with edge clamping and
  multi-monitor fallback.
- Added transaction cancellation on focus loss, reset, input-method switch,
  context destruction, helper loss, and protocol errors.
- Added a bounded, versioned D-Bus contract with peer-owner binding, replay
  protection, negotiation, and reconnect behavior.
- Added atomic state storage and recovery from invalid state files.
- Added Fedora 44 RPM and SRPM packaging, desktop metadata, AppStream metadata,
  package validation, and Bazzite installation guidance.
