# Unicode data maintenance

Normal builds use `data/generated/emoji_data.inc` and never access the network.
The generated file currently contains 3,944 fully-qualified RGI emoji from
Unicode Emoji 17.0 in official CLDR order.

English, German, and Russian names and keywords come from CLDR 48.2 annotations
and derived annotations. Isolated emoji components are excluded from the
palette. Fully-qualified ZWJ sequences and variation selectors remain intact.

## Search

All three annotation sets are indexed for every entry and every query is scored
against all of them. The requested locale only orders the results: a match in
that language outranks English, which outranks the remaining language. An entry
without a localized annotation simply contributes no match for that language.

Index and query text are folded to lower case and the combining marks that
occur in the bundled languages are composed, so a decomposed query matches the
precomposed generated data. The index is built from the committed file at
startup, which keeps search deterministic and offline.

## Font coverage

The supported catalog version is a property of this repository, not of the
machine. Emoji fonts lag behind Unicode, so entries may exist that the installed
font cannot draw. Those entries are kept, and the helper paints a labelled
placeholder for them instead of a missing-glyph box.

Never drop generated entries because the current font lacks a glyph. Coverage is
resolved at runtime and improves on its own when the font is updated.

## Updating

First update every version, URL, and independently obtained SHA-256 checksum in
`data/unicode-versions.json`. Then run:

```bash
./tools/update-unicode-data \
  --emoji-version 17.0 \
  --cldr-version 48.2
```

The updater downloads into a temporary directory, verifies every source before
parsing, and atomically replaces the generated output. It refuses requested
versions that are absent from the checksum manifest.

Run the generator twice and compare the outputs, build the core library, and run
all tests before submitting the update. The Unicode data workflow independently
downloads the pinned sources and compares the regenerated file byte for byte.

Do not update to beta, alpha, snapshot, or draft data for a stable project
release.
