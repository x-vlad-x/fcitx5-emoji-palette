#pragma once

#include <string>

#include <fcitx-utils/key.h>

namespace emoji_palette::addon {

class ShortcutMatcher {
  public:
    explicit ShortcutMatcher(std::string layout = "us");

    void setLayout(std::string layout);
    void setShortcuts(fcitx::KeyList shortcuts);
    bool matches(const fcitx::Key& key, const fcitx::Key& originalKey) const;

  private:
    void rebuildPhysicalShortcuts();

    std::string layout_;
    std::string variant_;
    fcitx::KeyList shortcuts_;
    fcitx::KeyList physicalShortcuts_;
};

}
