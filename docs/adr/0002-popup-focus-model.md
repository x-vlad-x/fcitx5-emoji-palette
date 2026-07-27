# ADR 0002: Popup focus model

- Status: Accepted
- Date: 2026-07-27

## Context

The source application must keep keyboard focus so Fcitx5 continues receiving
keys for search and navigation. The popup must also appear near the caret on
Plasma Wayland.

Qt's ordinary XDG top-level windows cannot be positioned by clients on Wayland.
A focusable search field would transfer focus and invalidate the source input
context.

## Options

### Focusable Qt tool window

Rejected because it invalidates the source context and makes safe direct commit
impossible.

### Non-activating ordinary Qt window

This protects focus where compositor hints are honored, but XDG Shell does not
support reliable absolute positioning. It is retained only as the X11 fallback.

### Layer-shell surface with no keyboard interactivity

Layer shell permits output selection, edge anchors, margins, and a compositor
guarantee that keyboard focus is impossible. Pointer interaction remains
available. It is selected for Wayland.

## Decision

Use LayerShellQt with top-left anchors, computed logical margins, top layer,
zero exclusive zone, and `KeyboardInteractivityNone`. The addon handles every
keyboard command. The helper renders a visual search field but it is not an
editable focus owner.

## Consequences

LayerShellQt is a required runtime dependency on Wayland/Plasma. Geometry must
be tested in logical coordinates across fractional scaling and negative monitor
origins. Other Wayland compositors must implement the layer-shell protocol or
will receive a safe active-screen fallback with documented limitations.
