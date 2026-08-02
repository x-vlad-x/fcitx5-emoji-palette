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

The cursor rectangle that does arrive is expressed in the client's device
pixels, paired with `InputContext::scaleFactor()`, while Qt, Wayland and
layer-shell margins are all logical pixels.

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

The addon sanitizes the raw cursor rectangle, transmits it in device pixels
together with the client scale factor, and transmits an empty rectangle at the
origin when no usable position exists. It records the frontend name, the raw
rectangle, the scale factor and the resulting decision in the `emojipalette`
log category, so the capture stage is diagnosable in the field without logging
any search text or selected sequence. The helper records the converted caret,
the chosen output and the final position under the
`org.fcitx.EmojiPalette.placement` Qt logging category.

The helper converts the rectangle to logical pixels, selects the output whose
usable area contains the caret or else the nearest output, places the palette
below the caret, flips it above when it does not fit, keeps the larger visible
remainder when neither side fits, and clamps the result into the usable
geometry of the chosen output.

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
