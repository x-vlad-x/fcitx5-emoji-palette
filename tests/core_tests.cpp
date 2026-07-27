#include "emoji_palette/catalog.hpp"
#include "emoji_palette/geometry.hpp"
#include "emoji_palette/keyboard.hpp"
#include "emoji_palette/state.hpp"
#include "emoji_palette/utf8.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testUtf8() {
    require(emoji_palette::isValidUtf8("😀"), "valid UTF-8 rejected");
    require(!emoji_palette::isValidUtf8(std::string("\xF0\x28\x8C\x28", 4)),
            "invalid UTF-8 accepted");
    require(emoji_palette::normalizeForSearch("  ÜBER-ЁЖ  ") == "über ёж", "search folding failed");
    const auto encoded = emoji_palette::hexEncode("👨‍👩‍👧");
    require(emoji_palette::hexDecode(encoded) == "👨‍👩‍👧", "hex round trip failed");
    require(!emoji_palette::hexDecode("0xz1"), "malformed hex accepted");
}

void testCatalog() {
    emoji_palette::EmojiCatalog catalog;
    require(catalog.records().size() == 3944, "unexpected emoji count");
    std::set<std::string> sequences;
    std::array<std::size_t, 9> counts{};
    const auto categories = emoji_palette::unicodeCategories();
    for (const auto& record : catalog.records()) {
        require(emoji_palette::isValidUtf8(record.sequence), "malformed generated sequence");
        require(sequences.insert(record.sequence).second, "duplicate generated sequence");
        const auto category = std::find(categories.begin(), categories.end(), record.category);
        require(category != categories.end(), "unknown generated category");
        ++counts[static_cast<std::size_t>(std::distance(categories.begin(), category))];
    }
    require(std::all_of(counts.begin(), counts.end(), [](auto count) { return count > 0; }),
            "empty Unicode category");
    require(catalog.contains("👨‍👩‍👧"), "ZWJ sequence missing");
    require(catalog.contains("🏳️‍🌈"), "variation selector sequence missing");
    require(!catalog.contains("A"), "non-emoji sequence accepted");
}

void testSearch() {
    emoji_palette::EmojiCatalog catalog;
    const auto english = catalog.search("grinning face", emoji_palette::Locale::English, 10);
    require(!english.empty() && english.front().emoji->sequence == "😀", "English ranking failed");
    const auto german = catalog.search("grinsendes gesicht", emoji_palette::Locale::German, 10);
    require(!german.empty() && german.front().emoji->sequence == "😀", "German search failed");
    const auto russian = catalog.search("широко улыбается", emoji_palette::Locale::Russian, 10);
    require(!russian.empty() && russian.front().emoji->sequence == "😀", "Russian search failed");
    const auto keyword = catalog.search("aubergine", emoji_palette::Locale::German, 10);
    require(std::any_of(keyword.begin(), keyword.end(),
                        [](const auto& result) { return result.emoji->sequence == "🍆"; }),
            "localized keyword search failed");
    const auto exact = catalog.search("🐧", emoji_palette::Locale::English, 1);
    require(exact.size() == 1 && exact.front().emoji->sequence == "🐧",
            "exact emoji search failed");
}

void testVariants() {
    emoji_palette::EmojiCatalog catalog;
    const auto variants = catalog.variantsFor("👍");
    require(variants.size() == 6, "skin tone variants missing");
    require(variants.front()->sequence == "👍", "base variant is not first");
    require(std::any_of(variants.begin(), variants.end(),
                        [](const auto* record) { return record->sequence == "👍🏿"; }),
            "dark skin tone variant missing");
}

void testState() {
    std::array<char, 64> pattern{};
    const std::string prefix = "/tmp/emoji-palette-state.XXXXXX";
    std::copy(prefix.begin(), prefix.end(), pattern.begin());
    auto* directory = mkdtemp(pattern.data());
    require(directory != nullptr, "temporary directory creation failed");
    const auto path = std::filesystem::path(directory) / "state";
    emoji_palette::StateStore store(path);
    emoji_palette::PersistentState state;
    emoji_palette::toggleFavorite(state, "😀");
    emoji_palette::toggleFavorite(state, "🐧");
    emoji_palette::toggleFavorite(state, "😀");
    require(state.favorites == std::vector<std::string>{"🐧"}, "favorite toggle failed");
    emoji_palette::recordUse(state, "😀", 10);
    emoji_palette::recordUse(state, "🐧", 20);
    emoji_palette::recordUse(state, "😀", 30);
    require(state.recents.front().sequence == "😀" && state.recents.front().useCount == 2,
            "recent ranking failed");
    require(store.save(state), "atomic state save failed");
    const auto loaded = store.load();
    require(!loaded.recoveredFromCorruption, "valid state reported corrupt");
    require(loaded.state.favorites == state.favorites, "favorites did not round trip");
    require(loaded.state.recents.size() == 2 && loaded.state.recents.front().useCount == 2,
            "recents did not round trip");
    {
        std::ofstream output(path, std::ios::trunc);
        output << "not a valid state";
    }
    const auto recovered = store.load();
    require(recovered.recoveredFromCorruption && recovered.state.favorites.empty(),
            "corruption recovery failed");
    std::filesystem::remove_all(directory);
}

void testKeyboard() {
    emoji_palette::KeyboardState state;
    state.setGrid(23, 5, 3);
    state.setCategory(0, 9);
    require(state.dispatch(emoji_palette::NavigationCommand::Right) ==
                emoji_palette::NavigationOutcome::Changed,
            "right navigation failed");
    state.dispatch(emoji_palette::NavigationCommand::Down);
    require(state.selectedIndex() == 6, "down navigation failed");
    state.dispatch(emoji_palette::NavigationCommand::PageDown);
    require(state.selectedIndex() == 21, "page navigation failed");
    state.dispatch(emoji_palette::NavigationCommand::Down);
    require(state.selectedIndex() == 22, "boundary clamp failed");
    state.dispatch(emoji_palette::NavigationCommand::NextCategory);
    require(state.categoryIndex() == 1 && state.selectedIndex() == 0, "category navigation failed");
    require(state.dispatch(emoji_palette::NavigationCommand::Select) ==
                emoji_palette::NavigationOutcome::Selected,
            "selection outcome failed");
    require(state.dispatch(emoji_palette::NavigationCommand::Cancel) ==
                emoji_palette::NavigationOutcome::Cancelled,
            "cancel outcome failed");
}

void testGeometry() {
    const emoji_palette::Rect screen{-1920, 0, 1920, 1080};
    const auto lower = emoji_palette::placePopup({-100, 1000, 2, 20}, {500, 400}, screen);
    require(!lower.belowCaret && lower.position.y == 600, "bottom-edge placement failed");
    require(lower.position.x == -500, "right-edge clamp failed");
    const auto upper = emoji_palette::placePopup({-1910, 10, 2, 20}, {500, 400}, screen);
    require(upper.belowCaret && upper.position.x == -1910 && upper.position.y == 30,
            "negative-origin placement failed");
    const auto oversized = emoji_palette::placePopup({100, 100, 1, 1}, {3000, 2000}, screen);
    require(oversized.position.x == screen.x && oversized.position.y == screen.y,
            "oversized popup clamp failed");
}

}

int main() {
    try {
        testUtf8();
        testCatalog();
        testSearch();
        testVariants();
        testState();
        testKeyboard();
        testGeometry();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
