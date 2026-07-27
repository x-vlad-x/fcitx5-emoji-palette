# ADR 0001: Runtime architecture

- Status: Accepted
- Date: 2026-07-27

## Context

The picker needs privileged access to the active Fcitx5 `InputContext`, a
responsive Qt UI, and fault isolation. Qt failures must not terminate Fcitx5.

## Options

### Single Fcitx5 shared-library addon with Qt UI

This has the shortest call path and no IPC. It also loads Qt, platform plugins,
theme plugins, and all UI state into the Fcitx5 process. A helper crash,
assertion, plugin incompatibility, or event-loop interaction can take down text
input for the entire desktop session. It is rejected.

### Small Fcitx5 addon plus separate Qt helper

This adds a small protocol and lifecycle coordination. It isolates rendering
and theme code, lets either side restart, keeps commit authority in Fcitx5, and
supports focused testing of the boundary. It is selected.

### Fcitx5 UI addon or candidate interface

Fcitx5 UI addons are intended to render common input panels, status, and menus.
The built-in candidate model could provide a basic emoji list but does not
provide the required large virtualized grid, favorites, variants, and
non-activating pointer interaction without replacing the session's normal
Fcitx UI. It remains useful upstream reference material but is not selected.

## Decision

Use a native C++20 Fcitx5 module with a separate Qt 6 Widgets helper. The addon
owns all input-context and commit behavior. The helper owns only presentation
and user choice.

## Consequences

The protocol becomes a security boundary and requires validation, versioning,
reconnect tests, and timeouts. The package ships two binaries but preserves
Fcitx5 reliability if the helper fails.
