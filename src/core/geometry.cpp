#include "emoji_palette/geometry.hpp"

#include <algorithm>

namespace emoji_palette {

PopupPlacement placePopup(const Rect& caret, const Size& popup, const Rect& available) {
    const int maximumX = available.x + std::max(0, available.width - popup.width);
    const int maximumY = available.y + std::max(0, available.height - popup.height);
    int x = std::clamp(caret.x, available.x, maximumX);
    const int below = caret.y + std::max(0, caret.height);
    const int above = caret.y - popup.height;
    const bool fitsBelow = below + popup.height <= available.y + available.height;
    const bool fitsAbove = above >= available.y;
    bool belowCaret = fitsBelow || !fitsAbove;
    int y = belowCaret ? below : above;
    y = std::clamp(y, available.y, maximumY);
    return {{x, y}, belowCaret};
}

}
