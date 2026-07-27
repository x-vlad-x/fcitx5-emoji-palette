# Architecture

Emoji Palette for Fcitx 5 uses a small Fcitx5 module and a separate Qt 6 helper
process. The processes communicate over a private, versioned session D-Bus
interface.

## Components

### Core

The core libraries contain the generated emoji model, category ordering, search
ranking, localized annotations, variants, state persistence, popup geometry,
and keyboard state machine. They do not depend on Fcitx5 or Qt widgets.

### Fcitx5 addon

The `emojipalette` module:

- recognizes the configured trigger key;
- starts one selection transaction for the triggering input context;
- stores the source context as
  `fcitx::TrackableObjectReference<fcitx::InputContext>`;
- consumes keyboard events while the transaction is active;
- forwards validated navigation and search commands to the helper;
- commits only a validated selection received for the current transaction;
- cancels on focus loss, reset, destruction, input-method switch, timeout,
  helper disconnect, or protocol error.

The addon is single-threaded on the Fcitx event loop. It does not contain Qt UI
code and never blocks while waiting for the helper.

### Qt 6 helper

The helper owns presentation, pointer interaction, accessibility objects,
theme integration, screen selection, and layer-shell configuration. It cannot
commit text and does not need keyboard focus. It receives logical key commands
from the addon.

On Wayland, LayerShellQt creates a top-layer surface with no exclusive zone and
`KeyboardInteractivityNone`. On X11, a non-activating frameless tool window is
used.

### Data

Pinned Unicode Emoji and CLDR inputs are converted into deterministic generated
data committed to the repository. Search indexes are built once at startup and
shared by category and query models.

User configuration, favorites, and recents live below the XDG config and state
directories. Writes use `QSaveFile` or an equivalent same-directory
write-and-rename implementation. Invalid files are quarantined or ignored and
replaced with safe defaults.

## Selection transaction

1. A key event matching the configured shortcut creates a random 128-bit
   transaction identifier and a tracked reference to the triggering context.
2. The addon captures the caret rectangle and sends `Show` to the helper.
3. While active, relevant keys are filtered before the input method and sent as
   bounded commands. Search text is never logged.
4. The helper sends one `Select` response containing the transaction identifier
   and UTF-8 sequence.
5. The addon verifies the identifier, message size, UTF-8 validity, membership
   in the generated emoji set, context liveness, and current focus.
6. The transaction enters a terminal state before `commitString()` is called.
   This ordering prevents a second commit.
7. Any lifecycle invalidation moves the transaction to cancelled and sends
   `Hide`; late responses are ignored.

## Dependency boundaries

| Target | Allowed dependencies |
| --- | --- |
| Core and model | C++20 standard library |
| IPC contract | C++20, Fcitx5 Utils DBus on addon side |
| Addon | Fcitx5 Core, Config, Utils, DBus module |
| Helper | Qt 6 Core, Gui, Widgets, DBus; LayerShellQt on Wayland |

KDE Frameworks are not required. LayerShellQt is a small Plasma component used
for a protocol capability that Qt's ordinary Wayland windows do not provide.

## Security boundaries

- No TCP listener exists.
- The session bus limits access to the user's session.
- The helper accepts calls only from the unique bus owner selected during the
  handshake.
- Every message has a protocol version, transaction identifier, and size bound.
- Selection strings must be valid UTF-8 and present in the generated allowlist.
- No message contains a command line or filesystem path to execute.
- The helper has no clipboard or synthetic-input integration.
- Logs exclude search queries and selections at normal verbosity.

## Failure behavior

Helper absence or crash cancels the active transaction without affecting
Fcitx5. Repeated activation is rate-limited and uses D-Bus activation or one
managed process, so it cannot create an unbounded process tree. An addon restart
causes the helper to hide and discard all transactions. Version mismatch is a
visible, actionable error and never falls back to an unsafe protocol.
