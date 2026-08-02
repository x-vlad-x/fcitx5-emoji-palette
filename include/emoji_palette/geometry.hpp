#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace emoji_palette {

struct Point {
    int x;
    int y;

    bool operator==(const Point&) const = default;
};

struct Size {
    int width;
    int height;

    bool operator==(const Size&) const = default;
};

struct Rect {
    int x;
    int y;
    int width;
    int height;

    bool operator==(const Rect&) const = default;
};

struct PopupPlacement {
    Point position;
    bool belowCaret;

    bool operator==(const PopupPlacement&) const = default;
};

// Shared by caret capture and by the wire format, so a rectangle the addon
// accepts can always be serialized.
inline constexpr int coordinateLimit = 1000000;
inline constexpr int dimensionLimit = 32768;
inline constexpr std::uint16_t minimumScalePercent = 50;
inline constexpr std::uint16_t maximumScalePercent = 400;

// An empty rectangle at the origin is the wire representation of "the frontend
// reported no cursor position". Wayland input-method clients never report one,
// so this value is the normal case there rather than an error.
inline constexpr Rect absentCaret{0, 0, 0, 0};

bool isTransportableRect(const Rect& value);

// Clamps a raw Fcitx5 cursor rectangle into the transportable range and
// returns absentCaret when it carries no usable position.
Rect sanitizedCaret(const Rect& raw);

// Fcitx5 reports the caret in the client's device pixels together with a scale
// factor, while Qt, Wayland and layer-shell margins use logical pixels.
std::optional<Rect> logicalCaret(const Rect& caret, std::uint16_t scalePercent);

// Index of the output whose usable area contains the caret, otherwise of the
// nearest output. Ties resolve to the lowest index. Empty outputs are skipped.
std::optional<std::size_t> outputForCaret(const Rect& caret, std::span<const Rect> outputs);

PopupPlacement placePopup(const Rect& caret, const Size& popup, const Rect& available);

// Documented fallback used when no caret rectangle is available.
Point centeredPopup(const Size& popup, const Rect& available);

}
