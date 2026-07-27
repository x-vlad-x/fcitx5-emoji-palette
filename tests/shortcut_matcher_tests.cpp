#include "shortcut_matcher.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

#include <fcitx-utils/key.h>
#include <fcitx-utils/keysym.h>

namespace {

constexpr int zKeycode = 52;
constexpr int xKeycode = 53;
constexpr int germanZKeycode = 29;

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

fcitx::Key key(fcitx::KeySym symbol, fcitx::KeyStates states, int code) {
    return fcitx::Key(symbol, states, code);
}

bool matches(emoji_palette::addon::ShortcutMatcher& matcher, fcitx::KeySym symbol,
             fcitx::KeyStates states, int code) {
    const auto original = key(symbol, states, code);
    return matcher.matches(original.normalize(), original);
}

void matchesSamePhysicalKeyAcrossLayouts() {
    emoji_palette::addon::ShortcutMatcher matcher("us");
    matcher.setShortcuts({fcitx::Key("Super+Z")});

    const auto super = fcitx::KeyStates(fcitx::KeyState::Super);
    require(matches(matcher, FcitxKey_Z, super, zKeycode), "English uppercase Z did not match");
    require(matches(matcher, FcitxKey_z, super, zKeycode), "English lowercase z did not match");
    require(matches(matcher, FcitxKey_Cyrillic_YA, super, zKeycode),
            "Russian uppercase Ya did not match the physical Z key");
    require(matches(matcher, FcitxKey_Cyrillic_ya, super, zKeycode),
            "Russian lowercase ya did not match the physical Z key");
}

void preservesConfigurableShortcutBehavior() {
    emoji_palette::addon::ShortcutMatcher matcher("us");
    matcher.setShortcuts({fcitx::Key("Control+X"), fcitx::Key("Super+Z")});

    const auto control = fcitx::KeyStates(fcitx::KeyState::Ctrl);
    const auto super = fcitx::KeyStates(fcitx::KeyState::Super);
    require(matches(matcher, FcitxKey_X, control, xKeycode),
            "alternative configurable shortcut did not match");
    require(!matches(matcher, FcitxKey_Cyrillic_YA, control, zKeycode),
            "wrong modifiers matched the physical shortcut");
    require(!matches(matcher, FcitxKey_Cyrillic_YA, super, xKeycode),
            "wrong physical key matched the shortcut");
    require(!matches(matcher, FcitxKey_Cyrillic_YA, super, 0),
            "layout-independent match succeeded without a hardware keycode");
}

void supportsExplicitKeycodeShortcuts() {
    emoji_palette::addon::ShortcutMatcher matcher("us");
    matcher.setShortcuts({fcitx::Key("Super+<52>")});

    const auto super = fcitx::KeyStates(fcitx::KeyState::Super);
    require(matches(matcher, FcitxKey_Cyrillic_YA, super, zKeycode),
            "explicit keycode shortcut did not match");
}

void followsConfiguredReferenceLayout() {
    emoji_palette::addon::ShortcutMatcher matcher("us");
    matcher.setShortcuts({fcitx::Key("Super+Z")});
    matcher.setLayout("de");

    const auto super = fcitx::KeyStates(fcitx::KeyState::Super);
    require(matches(matcher, FcitxKey_Cyrillic_YA, super, germanZKeycode),
            "German physical Z position did not match after a layout reload");
    require(!matches(matcher, FcitxKey_Cyrillic_YA, super, zKeycode),
            "old US physical Z position remained active after a layout reload");
}

}

int main() {
    matchesSamePhysicalKeyAcrossLayouts();
    preservesConfigurableShortcutBehavior();
    supportsExplicitKeycodeShortcuts();
    followsConfiguredReferenceLayout();
}
