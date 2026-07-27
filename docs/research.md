# Research

Research was performed on 2026-07-27 against the current Bazzite host, Fedora
44 packages, Fcitx5 5.1.21 source, Qt 6.11 documentation, and current upstream
project documentation.

## Host environment

| Component | Observed value |
| --- | --- |
| Deployment | Bazzite 44.20260721.0, Kinoite variant |
| Base | Fedora 44 |
| Kernel | 7.1.3-ogc5.1.fc44.x86_64 |
| Session | KDE Plasma 6.7.3 on Wayland |
| Fcitx5 | 5.1.21 |
| Fcitx5 Qt integration | 5.1.14 |
| Qt runtime | 6.11.1 |
| LayerShellQt runtime | 6.7.3 |
| Host GCC | 16.1.1 |

The immutable host does not contain `fcitx5-devel`, `qt6-qtbase-devel`,
`layer-shell-qt-devel`, CMake, Ninja, or Clang. No packages were layered onto
the host. A Fedora 44 OCI build environment was created instead.

## Fedora 44 build environment

| Component | Version |
| --- | --- |
| Fcitx5 development API | 5.1.21 |
| Qt development API | 6.11.1 |
| LayerShellQt development API | 6.7.3 |
| CMake | 4.3.0 |
| Ninja | 1.13.2 |
| GCC | 16.1.1 |
| Clang | 22.1.8 |

Fcitx5 exports CMake packages for Core, Config, Utils, and the DBus module.
With a `/usr` prefix on Fedora 44, its exported installation variables resolve
to:

| Variable | Value |
| --- | --- |
| `FCITX_INSTALL_ADDONDIR` | `/usr/lib64/fcitx5` |
| `FCITX_INSTALL_PKGDATADIR` | `/usr/share/fcitx5` |
| `FCITX_INSTALL_LIBEXECDIR` | `/usr/libexec` |
| `FCITX_INSTALL_LOCALEDIR` | `/usr/share/locale` |

The build must consume these exported variables and GNUInstallDirs instead of
embedding Fedora paths.

## Fcitx5 findings

Fcitx5 models every application client as an `InputContext`. Current upstream
modules register event handlers through `Instance::watchEvent()` and consume
hotkeys in `InputContextKeyEvent`. The clipboard, quickphrase, imselector, and
Unicode modules demonstrate the relevant event phases and filtering behavior.

The required lifecycle events exist in 5.1.21:

- `InputContextFocusOut`
- `InputContextReset`
- `InputContextDestroyed`
- `InputContextSwitchInputMethod`

`InputContext` derives from Fcitx5 tracking support and exposes
`watch()`, which returns `TrackableObjectReference<InputContext>`. Dereferencing
an expired reference returns null. This is the appropriate ownership primitive
for an asynchronous selection transaction.

`InputContext::commitString()` is the native commit path. The addon can consume
events with `filterAndAccept()` while the palette is active so search and
navigation keys do not reach either the active input method or the client.

Sources:

- [Fcitx5 basic concepts](https://fcitx-im.org/wiki/Basic_concept)
- [Fcitx5 addon development](https://fcitx-im.org/wiki/Develop_an_simple_input_method)
- [Fcitx5 5.1.21 source](https://github.com/fcitx/fcitx5/tree/5.1.21)

## Popup and focus findings

Qt documents that XDG Shell does not allow a client to position an ordinary
top-level window on Wayland. Calls that set the top-level position are normally
ignored. A plain `Qt::Tool` window therefore cannot satisfy caret-relative
placement reliably.

The Wayland layer-shell protocol lets a surface select an output, anchors,
margins, layer, and keyboard interactivity. `KeyboardInteractivityNone`
guarantees that keyboard focus is impossible while pointer input remains
available. LayerShellQt exposes this protocol to Qt 6 and is already shipped by
the target Plasma deployment.

The helper will use top-left anchoring and compositor-applied margins calculated
from the selected output's logical geometry. It will use the top layer, a zero
exclusive zone, and no keyboard interactivity. On non-Wayland platforms it will
fall back to a frameless tool window with
`Qt::WindowDoesNotAcceptFocus`; X11 positioning remains available.

Sources:

- [Qt window positioning on Wayland](https://doc.qt.io/qt-6/application-windows.html#wayland-peculiarities)
- [Qt window flags](https://doc.qt.io/qt-6/qt.html#WindowType-enum)
- [wlr layer-shell protocol](https://wayland.app/protocols/wlr-layer-shell-unstable-v1)
- [LayerShellQt source](https://invent.kde.org/plasma/layer-shell-qt)

## Packaging findings

Fedora packaging requires complete BuildRequires, system compiler flags,
standard macros, file ownership, desktop validation, AppStream metadata for
graphical applications, and SPDX license expressions. The package will use
`%cmake`, `%cmake_build`, `%cmake_install`, and `%ctest`.

Bazzite documents local RPM layering through `rpm-ostree install` followed by a
reboot. It warns that layered packages can block future upgrades when
dependencies diverge. Universal Blue's current `image-template` is the
recommended base for custom images; its `build.sh` uses `dnf5` inside the image
build.

Sources:

- [Fedora Packaging Guidelines](https://docs.fedoraproject.org/en-US/packaging-guidelines/)
- [Bazzite package layering](https://docs.bazzite.gg/Installing_and_Managing_Software/rpm-ostree/)
- [Universal Blue image template](https://github.com/ublue-os/image-template)
- [rpm-ostree native containers](https://coreos.github.io/rpm-ostree/container/)

## Unicode and CLDR findings

Unicode Emoji 17.0 is the latest stable emoji release. The normative data files
are available under the versioned Unicode 17.0.0 public directory. CLDR 48.2,
released on 2026-03-17, is the latest published stable CLDR release; the CLDR
49 page remains an incomplete draft and must not be consumed.

The generator will pin source URLs and SHA-256 checksums. Normal builds will
consume committed generated data and will not access the network. Unicode data
files and software are covered by the OSI-approved Unicode License v3 with the
SPDX identifier `Unicode-3.0`.

Sources:

- [Unicode Emoji 17.0 charts](https://www.unicode.org/emoji/charts-17.0/)
- [Unicode 17.0 emoji data](https://www.unicode.org/Public/17.0.0/emoji/)
- [CLDR releases](https://cldr.unicode.org/index/downloads)
- [Unicode licensing policy](https://www.unicode.org/policies/licensing_policy.html)

## Constraints established by research

- The helper process is mandatory for crash isolation.
- LayerShellQt is justified on Wayland by positioning and non-activation needs.
- The addon must not link Qt or KDE libraries.
- The active selection transaction owns only a tracked Fcitx5 reference.
- Focus loss, reset, destruction, or input-method switching invalidates the
  transaction before any commit.
- The helper must never receive the ability to commit text itself.
- Normal builds and RPM builds must be offline with respect to Unicode data.
