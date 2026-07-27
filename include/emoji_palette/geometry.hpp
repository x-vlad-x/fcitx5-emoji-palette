#pragma once

namespace emoji_palette {

struct Point {
    int x;
    int y;
};

struct Size {
    int width;
    int height;
};

struct Rect {
    int x;
    int y;
    int width;
    int height;
};

struct PopupPlacement {
    Point position;
    bool belowCaret;
};

PopupPlacement placePopup(const Rect& caret, const Size& popup, const Rect& available);

} // namespace emoji_palette
