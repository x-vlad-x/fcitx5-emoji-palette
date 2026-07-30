# ADR 0005: Font coverage and the missing-glyph fallback

- Status: Accepted
- Date: 2026-07-29

## Context

The catalog is generated from Unicode Emoji 17.0. Emoji fonts ship on their own
schedule, and every emoji font available on the supported Fedora 44 and Bazzite
44 targets still stops at Emoji 16.0. `google-noto-color-emoji-fonts-20250623`
covers `1fa8f-1fac6`, `1face-1fadc`, `1fadf-1fae9` and `1faf0-1faf8`, so the
Emoji 17.0 additions U+1F6D8, U+1FA8A, U+1FA8E, U+1FAC8, U+1FACD, U+1FAEA and
U+1FAEF have no glyph. Sixty-seven catalog entries reference them, because
U+1FAEF is the joiner element of every wrestling sequence.

Qt is not at fault. `QFontMetricsF::inFontUcs4()` resolves emoji through the
fontconfig fallback chain and correctly reports these code points as absent,
after which Qt paints `.notdef` — the missing-glyph box users see.

## Options

### Remove uncovered emoji from the catalog

Rejected. The sequences are valid, the data is authoritative, and coverage is a
property of the machine rather than of the project. Removing them would delete
correct data from every system, including systems whose fonts gain the glyphs
later, and would make the catalog non-deterministic across hosts.

### Pin the catalog to the newest fully covered Emoji version

Rejected for the same reason, with the added cost that the supported version
would regress whenever a distribution shipped an older font.

### Bundle an emoji font

Rejected. No released font carries these glyphs, so bundling would not fix the
defect. It would also add a large binary asset and a licensing and attribution
obligation for no benefit.

### Detect coverage at runtime and paint a documented fallback

Selected.

## Decision

`inspectGlyphCoverage()` in the core library reports the first code point of a
UTF-8 emoji sequence that a supplied probe cannot draw. Zero-width joiners,
variation selectors and tag characters shape their neighbours rather than
carrying glyphs and are never counted as missing.

The helper supplies the real probe through `QFontMetricsF::inFontUcs4()`, cached
per code point. `QRawFont::supportsCharacter()` is deliberately not used: it
inspects only the primary family and reports every emoji as unsupported.

Entries the installed font cannot draw are painted as a dashed rounded tile
carrying the uppercase hexadecimal identity of the missing code point, for
example `1FAEA`. The tile is drawn from the active palette, so it follows Breeze
light and dark themes, and it scales with the configured cell size. Tooltips,
the status line and accessible text keep the localized CLDR name and gain an
explicit note that the installed emoji font has no glyph.

The fallback is presentation only. Selection continues to commit the exact
original Unicode sequence through `InputContext::commitString()`.

## Consequences

Nothing is removed from the catalog and no font is bundled, so no new licensing
or attribution obligation arises. Affected entries stay searchable, selectable
and correct. As emoji fonts gain Emoji 17.0 coverage the placeholders disappear
by themselves with no project change.

Coverage is evaluated per code point. A font that has every code point of a ZWJ
sequence but lacks the ligature renders the parts side by side rather than as a
missing-glyph box; that is a different presentation and is out of scope.

The RPM keeps `Requires: google-noto-color-emoji-fonts`. Fedora 44 publishes no
generic `font(:lang=und-zsye)` provide that could express "any emoji font", and
without an emoji font the picker would show placeholders for the whole catalog,
so the dependency is genuine.
