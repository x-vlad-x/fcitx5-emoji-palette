#include "key_router.hpp"

#include "emoji_palette/ipc/protocol.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include <fcitx-utils/key.h>
#include <fcitx-utils/keysym.h>

namespace {

using emoji_palette::addon::ActiveKeyRouter;
using emoji_palette::addon::KeyRoute;
using emoji_palette::ipc::CommandKind;

constexpr int dKeycode = 40;
constexpr int vKeycode = 55;
constexpr int tKeycode = 28;
constexpr int zKeycode = 52;

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

KeyRoute press(const ActiveKeyRouter& router, fcitx::KeySym symbol, fcitx::KeyStates states,
               int code, std::string_view search) {
    const auto original = fcitx::Key(symbol, states, code);
    return router.route(original.normalize(), original, false, search);
}

KeyRoute press(const ActiveKeyRouter& router, fcitx::KeySym symbol, std::string_view search = {}) {
    return press(router, symbol, fcitx::KeyStates{}, 0, search);
}

bool isSearch(const KeyRoute& route, std::string_view text) {
    return route.action == KeyRoute::Action::Search && route.searchText == text;
}

bool isCommand(const KeyRoute& route, CommandKind command) {
    return route.action == KeyRoute::Action::Command && route.command == command;
}

bool isIgnored(const KeyRoute& route) { return route.action == KeyRoute::Action::Ignore; }

void russianPrintableKeysBecomeSearchText() {
    const ActiveKeyRouter router;
    require(isSearch(press(router, FcitxKey_Cyrillic_ef), "\xD1\x84"),
            "Cyrillic ef did not become search text");
    require(isSearch(press(router, FcitxKey_Cyrillic_a, "\xD1\x84"), "\xD1\x84\xD0\xB0"),
            "Cyrillic a did not append to existing search text");
    require(isSearch(press(router, FcitxKey_Cyrillic_EF, fcitx::KeyStates(fcitx::KeyState::Shift),
                           0, ""),
                     "\xD0\xA4"),
            "shifted Cyrillic EF did not become search text");
}

void germanPrintableKeysBecomeSearchText() {
    const ActiveKeyRouter router;
    require(isSearch(press(router, FcitxKey_adiaeresis), "\xC3\xA4"),
            "a-diaeresis did not become search text");
    require(isSearch(press(router, FcitxKey_ssharp), "\xC3\x9F"),
            "sharp s did not become search text");
    require(isSearch(
                press(router, FcitxKey_Udiaeresis, fcitx::KeyStates(fcitx::KeyState::Shift), 0, ""),
                "\xC3\x9C"),
            "shifted U-diaeresis did not become search text");
}

void englishPrintableKeysStillWork() {
    const ActiveKeyRouter router;
    require(isSearch(press(router, FcitxKey_a), "a"), "plain a did not become search text");
    require(
        isSearch(press(router, FcitxKey_A, fcitx::KeyStates(fcitx::KeyState::Shift), 0, ""), "A"),
        "shifted A did not become search text");
    require(isSearch(press(router, FcitxKey_1), "1"), "digit did not become search text");
    require(isSearch(press(router, FcitxKey_period, "cat"), "cat."),
            "punctuation did not append to search text");
}

void unmatchedKeysAreConsumedWithoutEffect() {
    const ActiveKeyRouter router;
    require(isIgnored(press(router, FcitxKey_F5)), "function key produced output");
    require(isIgnored(press(router, FcitxKey_t,
                            fcitx::KeyStates(fcitx::KeyState::Ctrl) | fcitx::KeyState::Alt,
                            tKeycode, "")),
            "modified chord produced output");
    require(isIgnored(press(router, FcitxKey_dead_acute)), "dead key produced output");
    require(isIgnored(press(router, FcitxKey_Shift_L)), "modifier press produced output");
    require(isIgnored(press(router, FcitxKey_Linefeed)), "control keysym produced output");
    require(
        isIgnored(press(router, FcitxKey_period, fcitx::KeyStates(fcitx::KeyState::Super), 0, "")),
        "trigger-like chord produced output");
}

void releasesAreConsumedWithoutEffect() {
    const ActiveKeyRouter router;
    const auto original = fcitx::Key(FcitxKey_Cyrillic_ef);
    const auto route = router.route(original.normalize(), original, true, "x");
    require(isIgnored(route), "printable release produced output");
    const auto escape = fcitx::Key(FcitxKey_Escape);
    require(isIgnored(router.route(escape.normalize(), escape, true, "")),
            "Escape release produced output");
}

void navigationKeysMapToCommands() {
    const ActiveKeyRouter router;
    require(isCommand(press(router, FcitxKey_Left), CommandKind::Left), "Left mismatch");
    require(isCommand(press(router, FcitxKey_Right), CommandKind::Right), "Right mismatch");
    require(isCommand(press(router, FcitxKey_Up), CommandKind::Up), "Up mismatch");
    require(isCommand(press(router, FcitxKey_Down), CommandKind::Down), "Down mismatch");
    require(isCommand(press(router, FcitxKey_Home), CommandKind::Home), "Home mismatch");
    require(isCommand(press(router, FcitxKey_End), CommandKind::End), "End mismatch");
    require(isCommand(press(router, FcitxKey_Page_Up), CommandKind::PageUp), "PageUp mismatch");
    require(isCommand(press(router, FcitxKey_Page_Down), CommandKind::PageDown),
            "PageDown mismatch");
    require(isCommand(press(router, FcitxKey_Tab), CommandKind::NextCategory), "Tab mismatch");
    require(isCommand(press(router, FcitxKey_Tab, fcitx::KeyStates(fcitx::KeyState::Shift), 0, ""),
                      CommandKind::PreviousCategory),
            "Shift+Tab mismatch");
    require(isCommand(press(router, FcitxKey_ISO_Left_Tab, fcitx::KeyStates(fcitx::KeyState::Shift),
                            0, ""),
                      CommandKind::PreviousCategory),
            "ISO Left Tab mismatch");
}

void selectionAndCancelKeysMapToCommands() {
    const ActiveKeyRouter router;
    require(isCommand(press(router, FcitxKey_Return), CommandKind::Select), "Return mismatch");
    require(isCommand(press(router, FcitxKey_KP_Enter), CommandKind::Select), "KP Enter mismatch");
    require(isCommand(press(router, FcitxKey_space), CommandKind::Select), "space mismatch");
    require(isCommand(press(router, FcitxKey_Escape), CommandKind::Cancel), "Escape mismatch");
}

void editingKeysEditSearchText() {
    const ActiveKeyRouter router;
    require(isSearch(press(router, FcitxKey_BackSpace, "\xD0\xB0\xD0\xB1"), "\xD0\xB0"),
            "Backspace did not remove one multi-byte character");
    require(isSearch(press(router, FcitxKey_BackSpace, "a"), ""),
            "Backspace did not clear the last character");
    require(isSearch(press(router, FcitxKey_BackSpace, ""), ""),
            "Backspace on empty search was not consumed as a search update");
    require(isIgnored(press(router, FcitxKey_Delete, "abc")), "Delete was not consumed as a no-op");
}

void favoriteAndVariantChordsAreLayoutIndependent() {
    ActiveKeyRouter router;
    require(
        isCommand(press(router, FcitxKey_d, fcitx::KeyStates(fcitx::KeyState::Ctrl), dKeycode, ""),
                  CommandKind::ToggleFavorite),
        "Ctrl+D did not toggle favorites on the US layout");
    require(
        isCommand(press(router, FcitxKey_v, fcitx::KeyStates(fcitx::KeyState::Ctrl), vKeycode, ""),
                  CommandKind::ShowVariants),
        "Ctrl+V did not show variants on the US layout");

    router.setLayout("ru");
    require(isCommand(press(router, FcitxKey_Cyrillic_ve, fcitx::KeyStates(fcitx::KeyState::Ctrl),
                            dKeycode, ""),
                      CommandKind::ToggleFavorite),
            "Ctrl on the physical D key did not toggle favorites on the Russian layout");
    require(isCommand(press(router, FcitxKey_Cyrillic_em, fcitx::KeyStates(fcitx::KeyState::Ctrl),
                            vKeycode, ""),
                      CommandKind::ShowVariants),
            "Ctrl on the physical V key did not show variants on the Russian layout");
    require(isIgnored(press(router, FcitxKey_Cyrillic_ya, fcitx::KeyStates(fcitx::KeyState::Ctrl),
                            zKeycode, "")),
            "a chord-free physical key matched an internal chord on the Russian layout");
}

void searchSizeBoundIsEnforced() {
    const ActiveKeyRouter router;
    const std::string almostFull(emoji_palette::ipc::maximumSearchSize - 1, 'a');
    require(isSearch(press(router, FcitxKey_b, almostFull), almostFull + "b"),
            "append below the size bound was rejected");
    require(isIgnored(press(router, FcitxKey_Cyrillic_ef, almostFull)),
            "append beyond the size bound was not consumed as a no-op");
}

}

int main() {
    russianPrintableKeysBecomeSearchText();
    germanPrintableKeysBecomeSearchText();
    englishPrintableKeysStillWork();
    unmatchedKeysAreConsumedWithoutEffect();
    releasesAreConsumedWithoutEffect();
    navigationKeysMapToCommands();
    selectionAndCancelKeysMapToCommands();
    editingKeysEditSearchText();
    favoriteAndVariantChordsAreLayoutIndependent();
    searchSizeBoundIsEnforced();
}
