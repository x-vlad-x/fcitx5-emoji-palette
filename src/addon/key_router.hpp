#pragma once

#include "shortcut_matcher.hpp"

#include "emoji_palette/ipc/protocol.hpp"

#include <cstdint>
#include <string>
#include <string_view>

#include <fcitx-utils/key.h>

namespace emoji_palette::addon {

// Routing decision for a key event that arrives while a selection transaction
// is active. Every such event is consumed by the addon; the route only
// selects the effect of the consumed key.
struct KeyRoute {
    enum class Action : std::uint8_t {
        Ignore,
        Command,
        Search,
    };

    Action action = Action::Ignore;
    ipc::CommandKind command = ipc::CommandKind::Cancel;
    std::string searchText;
};

class ActiveKeyRouter {
  public:
    ActiveKeyRouter();

    void setLayout(std::string layout);

    KeyRoute route(const fcitx::Key& key, const fcitx::Key& originalKey, bool isRelease,
                   std::string_view search) const;

  private:
    ShortcutMatcher favoriteChord_;
    ShortcutMatcher variantsChord_;
};

}
