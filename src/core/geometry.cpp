#include "emoji_palette/geometry.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace emoji_palette {
namespace {

int scaleUp(int value, std::uint16_t scalePercent) {
    const auto scaled = static_cast<std::int64_t>(value) * scalePercent;
    return static_cast<int>(scaled >= 0 ? (scaled + 50) / 100 : -((-scaled + 50) / 100));
}

int scaleDown(int value, std::uint16_t scalePercent) {
    const auto scaled = static_cast<std::int64_t>(value) * 100;
    const auto divisor = static_cast<std::int64_t>(scalePercent);
    return static_cast<int>(scaled >= 0 ? (scaled + divisor / 2) / divisor
                                        : -((-scaled + divisor / 2) / divisor));
}

std::int64_t axisDistance(int value, int origin, int extent) {
    const auto low = static_cast<std::int64_t>(origin);
    const auto high = low + extent - 1;
    if (value < low) {
        return low - value;
    }
    if (value > high) {
        return value - high;
    }
    return 0;
}

std::int64_t squaredDistance(const Rect& caret, const Rect& output) {
    const auto horizontal = axisDistance(caret.x, output.x, output.width);
    const auto vertical = axisDistance(caret.y, output.y, output.height);
    return horizontal * horizontal + vertical * vertical;
}

int clampToAvailable(int value, int origin, int extent, int size) {
    return std::clamp(value, origin, origin + std::max(0, extent - size));
}

}

bool isTransportableRect(const Rect& value) {
    return value.x >= -coordinateLimit && value.x <= coordinateLimit &&
           value.y >= -coordinateLimit && value.y <= coordinateLimit && value.width >= 0 &&
           value.width <= dimensionLimit && value.height >= 0 && value.height <= dimensionLimit;
}

Rect sanitizedCaret(const Rect& raw) {
    // Some Fcitx5 frontends report an unknown extent as a bottom-right corner
    // of zero, which yields a negative width and height rather than an empty
    // rectangle at a known position.
    const Rect candidate{raw.x, raw.y, std::max(0, raw.width), std::max(0, raw.height)};
    return isTransportableRect(candidate) ? candidate : absentCaret;
}

Rect nativeOutputBounds(const Rect& logicalOutput, std::uint16_t scalePercent) {
    return {logicalOutput.x, logicalOutput.y, scaleUp(logicalOutput.width, scalePercent),
            scaleUp(logicalOutput.height, scalePercent)};
}

Rect logicalFromNative(const Rect& native, const Rect& logicalOutput, std::uint16_t scalePercent) {
    return {logicalOutput.x + scaleDown(native.x - logicalOutput.x, scalePercent),
            logicalOutput.y + scaleDown(native.y - logicalOutput.y, scalePercent),
            scaleDown(native.width, scalePercent), scaleDown(native.height, scalePercent)};
}

std::optional<std::size_t> outputForCaret(const Rect& caret, std::span<const Rect> outputs) {
    std::optional<std::size_t> best;
    auto bestDistance = std::numeric_limits<std::int64_t>::max();
    for (std::size_t index = 0; index < outputs.size(); ++index) {
        const auto& output = outputs[index];
        if (output.width <= 0 || output.height <= 0) {
            continue;
        }
        const auto distance = squaredDistance(caret, output);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = index;
        }
        if (bestDistance == 0) {
            break;
        }
    }
    return best;
}

PopupPlacement placePopup(const Rect& caret, const Size& popup, const Rect& available) {
    const int below = caret.y + std::max(0, caret.height);
    const int above = caret.y - popup.height;
    const bool fitsBelow = below + popup.height <= available.y + available.height;
    const bool fitsAbove = above >= available.y;
    bool belowCaret = true;
    if (!fitsBelow) {
        // Neither side fits only on very small outputs; keep the larger
        // remainder visible instead of always covering the text below.
        belowCaret =
            fitsAbove ? false : (available.y + available.height) - below >= caret.y - available.y;
    }
    return {
        {clampToAvailable(caret.x, available.x, available.width, popup.width),
         clampToAvailable(belowCaret ? below : above, available.y, available.height, popup.height)},
        belowCaret};
}

Point centeredPopup(const Size& popup, const Rect& available) {
    return {clampToAvailable(available.x + (available.width - popup.width) / 2, available.x,
                             available.width, popup.width),
            clampToAvailable(available.y + (available.height - popup.height) / 2, available.y,
                             available.height, popup.height)};
}

}
