#include "key_router.hpp"

#include "emoji_palette/utf8.hpp"

#include <optional>
#include <utility>

#include <fcitx-utils/keysym.h>

namespace emoji_palette::addon {

namespace {

KeyRoute command(ipc::CommandKind kind) { return {KeyRoute::Action::Command, kind, {}}; }

KeyRoute searchUpdate(std::string text) {
    return {KeyRoute::Action::Search, ipc::CommandKind::SearchText, std::move(text)};
}

bool isPrintable(const fcitx::Key& key) {
    if (key.states()) {
        return false;
    }
    const auto unicode = fcitx::Key::keySymToUnicode(key.sym());
    return unicode >= 0x20U && unicode != 0x7fU && (unicode < 0x80U || unicode > 0x9fU);
}

std::optional<ipc::CommandKind> navigationCommand(const fcitx::Key& key) {
    if (key.check(FcitxKey_Left)) {
        return ipc::CommandKind::Left;
    }
    if (key.check(FcitxKey_Right)) {
        return ipc::CommandKind::Right;
    }
    if (key.check(FcitxKey_Up)) {
        return ipc::CommandKind::Up;
    }
    if (key.check(FcitxKey_Down)) {
        return ipc::CommandKind::Down;
    }
    if (key.check(FcitxKey_Home)) {
        return ipc::CommandKind::Home;
    }
    if (key.check(FcitxKey_End)) {
        return ipc::CommandKind::End;
    }
    if (key.check(FcitxKey_Page_Up)) {
        return ipc::CommandKind::PageUp;
    }
    if (key.check(FcitxKey_Page_Down)) {
        return ipc::CommandKind::PageDown;
    }
    if (key.check(FcitxKey_Tab)) {
        return ipc::CommandKind::NextCategory;
    }
    if (key.check(FcitxKey_Tab, fcitx::KeyState::Shift)) {
        return ipc::CommandKind::PreviousCategory;
    }
    return std::nullopt;
}

}

ActiveKeyRouter::ActiveKeyRouter() {
    favoriteChord_.setShortcuts({fcitx::Key("Control+D")});
    variantsChord_.setShortcuts({fcitx::Key("Control+V")});
}

void ActiveKeyRouter::setLayout(std::string layout) {
    favoriteChord_.setLayout(layout);
    variantsChord_.setLayout(std::move(layout));
}

KeyRoute ActiveKeyRouter::route(const fcitx::Key& key, const fcitx::Key& originalKey,
                                bool isRelease, std::string_view search) const {
    if (isRelease) {
        return {};
    }
    if (key.check(FcitxKey_Escape)) {
        return command(ipc::CommandKind::Cancel);
    }
    if (key.check(FcitxKey_Return) || key.check(FcitxKey_KP_Enter) || key.check(FcitxKey_space)) {
        return command(ipc::CommandKind::Select);
    }
    if (favoriteChord_.matches(key, originalKey)) {
        return command(ipc::CommandKind::ToggleFavorite);
    }
    if (variantsChord_.matches(key, originalKey)) {
        return command(ipc::CommandKind::ShowVariants);
    }
    if (const auto navigation = navigationCommand(key)) {
        return command(*navigation);
    }
    if (key.check(FcitxKey_BackSpace)) {
        std::string result{search};
        if (auto codepoints = decodeUtf8(result); codepoints && !codepoints->empty()) {
            codepoints->pop_back();
            result = encodeUtf8(*codepoints);
        }
        return searchUpdate(std::move(result));
    }
    if (isPrintable(key)) {
        const auto text = fcitx::Key::keySymToUTF8(key.sym());
        if (!text.empty() && search.size() + text.size() <= ipc::maximumSearchSize) {
            std::string result{search};
            result += text;
            return searchUpdate(std::move(result));
        }
    }
    return {};
}

}
