# ADR 0004: IPC protocol

- Status: Accepted
- Date: 2026-07-27

## Context

The addon and helper need asynchronous, same-session communication with
reconnect behavior, peer identity, bounded messages, and no unstable C++ ABI.

## Options

### Private Unix-domain socket

A socket offers full framing control but requires secure runtime-directory
creation, permissions, peer credential checks, stale-socket cleanup, activation,
and a custom event-loop bridge on both sides.

### Private session D-Bus interface

The session bus already provides same-session routing, unique peer names,
activation, message framing, disconnection signals, and native integrations in
Fcitx5 Utils and QtDBus. Policy still requires strict application validation.
It is selected.

## Decision

Use `org.fcitx.Fcitx5.EmojiPalette1` at
`/org/fcitx/Fcitx5/EmojiPalette1` with an explicit protocol version.

The initial contract contains:

- `Hello(version, capabilities)` and a negotiated response;
- `Show(transaction, caret, screen, locale, configuration)`;
- `Command(transaction, sequence, kind, text)`;
- `Hide(transaction, reason)`;
- `Selected(transaction, emoji)`;
- `Cancelled(transaction, reason)`;
- `Ping(nonce)` for bounded liveness checks.

Limits are 64 KiB per D-Bus payload, 256 UTF-8 bytes per search command, and
128 UTF-8 bytes per selected sequence. Transaction identifiers are 128-bit
random values represented as 32 lowercase hexadecimal characters.

The addon binds a session to the helper's unique bus owner after successful
negotiation. The helper similarly rejects calls from other owners while bound.
Version mismatch and owner changes terminate all active transactions.

## Consequences

The public D-Bus signature is stable within a major protocol version. Internal
C++ types never cross the process boundary. Tests must cover malformed UTF-8,
oversized messages, stale transactions, replay, owner changes, timeouts, and
reconnection.
