#include "shortcut_matcher.hpp"

#include <limits>
#include <memory>
#include <utility>

#include <xkbcommon/xkbcommon.h>

namespace emoji_palette::addon {

namespace {

std::pair<std::string, std::string> splitLayout(std::string layout) {
    const auto separator = layout.find('-');
    if (separator == std::string::npos) {
        return std::make_pair(std::move(layout), std::string{});
    }
    auto variant = layout.substr(separator + 1);
    layout.resize(separator);
    return {std::move(layout), std::move(variant)};
}

using Keymap = std::unique_ptr<xkb_keymap, decltype(&xkb_keymap_unref)>;

Keymap createKeymap(xkb_context* context, const std::string& layout, const std::string& variant) {
    const xkb_rule_names names{
        .rules = nullptr,
        .model = nullptr,
        .layout = layout.c_str(),
        .variant = variant.empty() ? nullptr : variant.c_str(),
        .options = nullptr,
    };
    return Keymap(xkb_keymap_new_from_names(context, &names, XKB_KEYMAP_COMPILE_NO_FLAGS),
                  xkb_keymap_unref);
}

bool appendKeycodes(xkb_keymap* keymap, const fcitx::Key& shortcut,
                    fcitx::KeyList& physicalShortcuts) {
    if (keymap == nullptr) {
        return false;
    }
    bool matched = false;
    const auto maximumCode = xkb_keymap_max_keycode(keymap);
    for (xkb_keycode_t code = xkb_keymap_min_keycode(keymap);; ++code) {
        bool found = false;
        const auto layoutCount = xkb_keymap_num_layouts_for_key(keymap, code);
        for (xkb_layout_index_t layout = 0; layout < layoutCount && !found; ++layout) {
            const auto levelCount = xkb_keymap_num_levels_for_key(keymap, code, layout);
            for (xkb_level_index_t level = 0; level < levelCount && !found; ++level) {
                const xkb_keysym_t* symbols = nullptr;
                const auto symbolCount =
                    xkb_keymap_key_get_syms_by_level(keymap, code, layout, level, &symbols);
                for (int index = 0; index < symbolCount && !found; ++index) {
                    found = symbols[index] == static_cast<xkb_keysym_t>(shortcut.sym());
                }
            }
        }
        if (found && code <= static_cast<xkb_keycode_t>(std::numeric_limits<int>::max())) {
            const auto keycode = static_cast<int>(code);
            physicalShortcuts.push_back(fcitx::Key::fromKeyCode(keycode, shortcut.states()));
            const auto symbol = static_cast<xkb_keysym_t>(shortcut.sym());
            if (xkb_keysym_to_lower(symbol) != xkb_keysym_to_upper(symbol) &&
                !shortcut.states().test(fcitx::KeyState::Shift)) {
                physicalShortcuts.push_back(
                    fcitx::Key::fromKeyCode(keycode, shortcut.states() | fcitx::KeyState::Shift));
            }
            matched = true;
        }
        if (code == maximumCode) {
            break;
        }
    }
    return matched;
}

}

ShortcutMatcher::ShortcutMatcher(std::string layout) { setLayout(std::move(layout)); }

void ShortcutMatcher::setLayout(std::string layout) {
    auto [name, variant] = splitLayout(std::move(layout));
    if (name.empty()) {
        name = "us";
    }
    if (layout_ == name && variant_ == variant) {
        return;
    }
    layout_ = std::move(name);
    variant_ = std::move(variant);
    rebuildPhysicalShortcuts();
}

void ShortcutMatcher::setShortcuts(fcitx::KeyList shortcuts) {
    shortcuts_ = std::move(shortcuts);
    rebuildPhysicalShortcuts();
}

bool ShortcutMatcher::matches(const fcitx::Key& key, const fcitx::Key& originalKey) const {
    return key.checkKeyList(shortcuts_) || originalKey.checkKeyList(physicalShortcuts_);
}

void ShortcutMatcher::rebuildPhysicalShortcuts() {
    physicalShortcuts_.clear();
    if (shortcuts_.empty()) {
        return;
    }

    const std::unique_ptr<xkb_context, decltype(&xkb_context_unref)> context(
        xkb_context_new(XKB_CONTEXT_NO_FLAGS), xkb_context_unref);
    if (!context) {
        return;
    }
    const auto keymap = createKeymap(context.get(), layout_, variant_);
    const auto fallback =
        layout_ == "us" ? Keymap(nullptr, xkb_keymap_unref) : createKeymap(context.get(), "us", {});
    if (!keymap && !fallback) {
        return;
    }

    for (const auto& shortcut : shortcuts_) {
        if (shortcut.code() != 0) {
            physicalShortcuts_.push_back(shortcut);
            continue;
        }
        if (!appendKeycodes(keymap.get(), shortcut, physicalShortcuts_)) {
            appendKeycodes(fallback.get(), shortcut, physicalShortcuts_);
        }
    }
}

}
