#pragma once

#include <cstddef>

namespace emoji_palette {

enum class NavigationCommand {
    Left,
    Right,
    Up,
    Down,
    Home,
    End,
    PageUp,
    PageDown,
    PreviousCategory,
    NextCategory,
    Select,
    Cancel,
};

enum class NavigationOutcome {
    Unchanged,
    Changed,
    CategoryChanged,
    Selected,
    Cancelled,
};

class KeyboardState {
  public:
    void setGrid(std::size_t itemCount, std::size_t columns, std::size_t visibleRows);
    void setCategory(std::size_t index, std::size_t count);
    NavigationOutcome dispatch(NavigationCommand command);

    std::size_t selectedIndex() const;
    std::size_t categoryIndex() const;

  private:
    std::size_t itemCount_ = 0;
    std::size_t columns_ = 1;
    std::size_t visibleRows_ = 1;
    std::size_t selectedIndex_ = 0;
    std::size_t categoryIndex_ = 0;
    std::size_t categoryCount_ = 1;
};

} // namespace emoji_palette
