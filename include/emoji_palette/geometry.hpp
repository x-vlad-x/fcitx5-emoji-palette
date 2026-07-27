#pragma once

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

PopupPlacement placePopup(const Rect& caret, const Size& popup, const Rect& available);

} // namespace emoji_palette
