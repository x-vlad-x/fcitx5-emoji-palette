#include "emoji_palette/keyboard.hpp"

#include <algorithm>

namespace emoji_palette {

void KeyboardState::setGrid(std::size_t itemCount, std::size_t columns, std::size_t visibleRows) {
    itemCount_ = itemCount;
    columns_ = std::max<std::size_t>(1, columns);
    visibleRows_ = std::max<std::size_t>(1, visibleRows);
    if (itemCount_ == 0) {
        selectedIndex_ = 0;
    } else {
        selectedIndex_ = std::min(selectedIndex_, itemCount_ - 1);
    }
}

void KeyboardState::setCategory(std::size_t index, std::size_t count) {
    categoryCount_ = std::max<std::size_t>(1, count);
    categoryIndex_ = std::min(index, categoryCount_ - 1);
}

NavigationOutcome KeyboardState::dispatch(NavigationCommand command) {
    if (command == NavigationCommand::Select) {
        return itemCount_ == 0 ? NavigationOutcome::Unchanged : NavigationOutcome::Selected;
    }
    if (command == NavigationCommand::Cancel) {
        return NavigationOutcome::Cancelled;
    }
    if (command == NavigationCommand::PreviousCategory ||
        command == NavigationCommand::NextCategory) {
        if (categoryCount_ <= 1) {
            return NavigationOutcome::Unchanged;
        }
        if (command == NavigationCommand::PreviousCategory) {
            categoryIndex_ = (categoryIndex_ + categoryCount_ - 1) % categoryCount_;
        } else {
            categoryIndex_ = (categoryIndex_ + 1) % categoryCount_;
        }
        selectedIndex_ = 0;
        return NavigationOutcome::CategoryChanged;
    }
    if (itemCount_ == 0) {
        return NavigationOutcome::Unchanged;
    }

    const std::size_t previous = selectedIndex_;
    switch (command) {
    case NavigationCommand::Left:
        if (selectedIndex_ > 0) {
            --selectedIndex_;
        }
        break;
    case NavigationCommand::Right:
        selectedIndex_ = std::min(selectedIndex_ + 1, itemCount_ - 1);
        break;
    case NavigationCommand::Up:
        if (selectedIndex_ >= columns_) {
            selectedIndex_ -= columns_;
        }
        break;
    case NavigationCommand::Down:
        selectedIndex_ = std::min(selectedIndex_ + columns_, itemCount_ - 1);
        break;
    case NavigationCommand::Home:
        selectedIndex_ = 0;
        break;
    case NavigationCommand::End:
        selectedIndex_ = itemCount_ - 1;
        break;
    case NavigationCommand::PageUp: {
        const auto step = columns_ * visibleRows_;
        selectedIndex_ = selectedIndex_ > step ? selectedIndex_ - step : 0;
        break;
    }
    case NavigationCommand::PageDown:
        selectedIndex_ = std::min(selectedIndex_ + columns_ * visibleRows_, itemCount_ - 1);
        break;
    default:
        break;
    }
    return selectedIndex_ == previous ? NavigationOutcome::Unchanged : NavigationOutcome::Changed;
}

std::size_t KeyboardState::selectedIndex() const { return selectedIndex_; }

std::size_t KeyboardState::categoryIndex() const { return categoryIndex_; }

} // namespace emoji_palette
