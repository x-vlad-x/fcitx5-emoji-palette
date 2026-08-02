# ADR 0006: Caret-relative placement and its Wayland fallback

- Status: Accepted
- Date: 2026-08-02

## Context

The palette should open next to the caret of the source text field, like an
input-method candidate window. ADR 0002 chose a layer-shell surface in the
separate helper process, which can be placed anywhere on a chosen output but
only in absolute output coordinates. The helper therefore depends entirely on
the caret rectangle that the addon reads from the source `InputContext`.

Fcitx5 populates `InputContext::cursorRect()` from its `dbusfrontend`,
`fcitx4frontend`, `ibusfrontend` and `xim` frontends only. The `waylandim`
frontend, which serves every native Wayland client through `text-input-v3`,
never calls `setCursorRect()`. This is a property of the Wayland input-method
protocols rather than an omission: a Wayland client is not told where its own
surface is, and an input method is not told where the client's caret is in
global coordinates. Fcitx5's own candidate window solves this with
`zwp_input_popup_surface_v2`, where the compositor performs the placement.
That object is created from the Wayland connection that owns
`zwp_input_method_v2`, so it is only reachable inside the Fcitx5 process, and
the surface it positions must be rendered there too.

A cursor rectangle that does arrive is not always absolute. A client that sets
`CapabilityFlag::RelativeRect`, which the Fcitx5 Qt module does for every
Wayland client, reports the caret relative to its own window; only a compositor
can resolve that. An absolute rectangle is expressed in the device pixels of
the output that holds it, while Qt, Wayland and layer-shell margins are all
logical pixels.

## Options

### Move rendering into the Fcitx5 process to use an input popup surface

This is the only way to obtain compositor-performed caret placement for native
Wayland clients. It reverses ADR 0001 and puts Qt, theme plugins and the whole
UI back into the Fcitx5 process, where a helper fault takes down text input for
the session. Rejected here; it needs its own issue and decision record.

### Guess the caret position from the pointer or the focused window

Rejected. The pointer is unrelated to the caret, and a Wayland client's surface
position is not available to the helper either. A guess that is usually wrong is
worse than a predictable fallback.

### Use the caret wherever a frontend provides one, and a documented fallback
### otherwise

Selected.

## Decision

The addon transmits an absolute cursor rectangle in device pixels, and an empty
rectangle at the origin whenever the client sets `RelativeRect` or reports no
usable position. It also transmits the scale factor the client reports for
itself, which is diagnostic only: an absolute rectangle already lives on the
output's pixel grid, so the output scale governs the conversion. It records the frontend name, the raw
rectangle, the scale factor and the resulting decision in the `emojipalette`
log category, so the capture stage is diagnosable in the field without logging
any search text or selected sequence. The helper records the converted caret,
the chosen output and the final position under the
`org.fcitx.EmojiPalette.placement` Qt logging category.

The helper resolves the output in device pixels first, using each output's
logical geometry scaled by its own device pixel ratio, and picks the output
containing the caret or else the nearest one. It then converts the rectangle
into that output's logical pixels, places the palette below the caret, flips it
above when it does not fit, keeps the larger visible remainder when neither
side fits, and clamps the result into the usable geometry of that output.

Qt places an output's origin in logical pixels and counts its native pixels
from that same origin, so both conversions keep the origin and scale only the
offset and the extent. On a multi-output layout whose outputs do not share one
scale factor, the device-pixel layout an X11 or XWayland client sees is not a
simple product of the logical layout; the nearest-output rule keeps such a
caret on a real output rather than off-screen.

Qt reports the integer buffer scale of a Wayland output, not its fractional
scale, and the fractional scale of an output only reaches a client once one of
its surfaces has been mapped there. A reported ratio of exactly one is
therefore the only value usable without a mapped surface, because no larger
fractional scale rounds down to it. The helper records the real scale of every
output the palette has appeared on and uses the documented fallback while an
output's scale is still only an upper bound, rather than converting with a
value it knows may be wrong.

When no caret rectangle is available, the palette is centered on the active
output. On Wayland this uses an unanchored layer surface, which the compositor
centers on the output it assigns, combined with the active-output request; the
primary output is never forced. On X11 the output under the pointer is used.

`Show.screen` remains reserved. Fcitx5 exposes no output geometry, so the
helper always resolves the output itself.

## Consequences

Caret-relative placement works for X11 and XWayland clients, for clients using
the Fcitx5 Qt or GTK input-method modules, and for any future frontend that
reports a cursor rectangle. Native Wayland clients receive the documented
centered fallback on the active output until an input popup surface becomes
reachable, which would require reversing ADR 0001.

The device-pixel to logical conversion, the output selection and the edge
clamping are pure functions in the core library and are covered by tests at
100, 125, 150 and 200 percent scaling, across multiple outputs, negative
origins, gaps between outputs and every screen edge and corner.

Clients that reach Fcitx5 through the Qt or GTK input-method modules on Wayland
report a window-relative rectangle and therefore also receive the centered
fallback, even though a rectangle is present.
