# Unicode data maintenance

Normal builds use `data/generated/emoji_data.inc` and never access the network.
The generated file currently contains 3,944 fully-qualified RGI emoji from
Unicode Emoji 17.0 in official CLDR order.

English, German, and Russian names and keywords come from CLDR 48.2 annotations
and derived annotations. Isolated emoji components are excluded from the
palette. Fully-qualified ZWJ sequences and variation selectors remain intact.

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
