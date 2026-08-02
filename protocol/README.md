# D-Bus protocol

The helper owns `org.fcitx.Fcitx5.EmojiPalette1` and exports
`/org/fcitx/Fcitx5/EmojiPalette1`. The introspection contract is in
`org.fcitx.Fcitx5.EmojiPalette1.xml`.

The `Exchange` method and `Frame` signal carry a byte array. The byte array is a
stable binary envelope so neither Qt nor Fcitx5 C++ types become part of the
protocol ABI.

## Envelope

All integers use network byte order.

| Offset | Size | Meaning |
| --- | --- | --- |
| 0 | 4 | ASCII magic `EPAL` |
| 4 | 2 | Protocol version |
| 6 | 1 | Message type |
| 7 | 1 | Reserved, must be zero |
| 8 | 4 | Payload length |
| 12 | variable | Type-specific payload |

The complete envelope is limited to 64 KiB. Search text is limited to 256 UTF-8
bytes and a selection to 128 UTF-8 bytes. Rectangles use signed 32-bit
coordinates with explicit safe bounds.

`Show` carries an absolute caret rectangle in the device pixels of the output
that holds it. The receiver resolves the output in that same space and converts
with the output scale factor, not with the client scale. An empty rectangle at
the origin means no usable absolute position exists, which is the normal case
for native Wayland clients and for any client whose rectangle is relative to
its own window. `Show.scalePercent` carries the scale factor the client reports
for itself; it is diagnostic only and is never used for placement.
`Show.screen` is reserved and always empty: Fcitx5 exposes no output geometry,
so the helper resolves the output itself.

## Handshake and ownership

The addon starts with `Hello`, containing its supported version range, random
nonce, and capability bits. The helper returns `Welcome` with the selected
version, the same nonce, and the capability intersection.

After negotiation, each side binds the session to the peer's unique D-Bus owner.
Frames from any other owner are rejected. Owner loss invalidates the session and
all active transactions. Reconnection uses bounded exponential backoff from
100 ms to 5 seconds.

## Transactions

Selection messages use a nonzero 128-bit transaction identifier. Its text form
is exactly 32 lowercase hexadecimal characters. Command sequence numbers are
strictly increasing and nonzero; stale, replayed, or cross-transaction commands
are rejected.

`Selected` carries a candidate string but does not authorize a commit. The
Fcitx5 addon must additionally validate the active transaction, source context,
focus, UTF-8, size, and membership in the generated emoji catalog.

## Compatibility rules

- adding an optional capability is backward-compatible;
- changing a message layout requires a new protocol version;
- unknown capabilities are ignored;
- unknown commands, malformed values, invalid UTF-8, reserved bits, and
  oversized payloads are rejected;
- no implementation silently downgrades across major versions.
