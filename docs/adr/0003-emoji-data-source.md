# ADR 0003: Emoji data source

- Status: Accepted
- Date: 2026-07-27

## Context

The picker needs complete emoji sequences, official ordering, skin-tone and ZWJ
handling, and English, German, and Russian search terms. Hand-maintained tables
are incomplete and difficult to audit.

## Options

### Third-party picker tables

Rejected because provenance, freshness, translation quality, and license
obligations vary.

### Runtime system emoji database

Rejected because no stable cross-distribution API provides all required
ordering and localized annotations, and results would vary between systems.

### Pinned Unicode Emoji and CLDR data

Selected because the files are authoritative, versioned, machine-readable, and
covered by Unicode-3.0.

## Decision

Generate committed deterministic data from Unicode Emoji 17.0 and CLDR 48.2.
Pin every download URL and SHA-256 checksum in `data/unicode-versions.json`.
Normal builds are offline. The explicit updater downloads into a temporary
directory, verifies checksums before parsing, and emits stable sorted output.

## Consequences

Generated data and Unicode license notices are distributed in releases.
Updating Unicode is a reviewed source change with deterministic-generation
tests. Beta and draft data are rejected.
