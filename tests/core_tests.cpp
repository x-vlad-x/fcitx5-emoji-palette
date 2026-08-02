#include "emoji_palette/catalog.hpp"
#include "emoji_palette/geometry.hpp"
#include "emoji_palette/glyph_coverage.hpp"
#include "emoji_palette/keyboard.hpp"
#include "emoji_palette/state.hpp"
#include "emoji_palette/utf8.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unistd.h>
#include <utility>

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

void testGlyphCoverage() {
    // Mirrors google-noto-color-emoji-fonts-20250623, which still stops at Emoji 16.0.
    const auto emoji16Font = [](std::uint32_t codepoint) {
        static const std::array<std::pair<std::uint32_t, std::uint32_t>, 8> ranges = {{
            {0x1F300, 0x1F321},
            {0x1F3F3, 0x1F3FF},
            {0x1F466, 0x1F469},
            {0x1F600, 0x1F64F},
            {0x1F9D1, 0x1F9DD},
            {0x1FA8F, 0x1FAC6},
            {0x1FADF, 0x1FAE9},
            {0x1FAF0, 0x1FAF8},
        }};
        return std::any_of(ranges.begin(), ranges.end(), [codepoint](const auto& range) {
            return codepoint >= range.first && codepoint <= range.second;
        });
    };

    const auto covered = emoji_palette::inspectGlyphCoverage("😀", emoji16Font);
    require(covered.renderable, "covered emoji reported as missing");
    require(!covered.missingCodepoint, "covered emoji reported a missing code point");
    require(emoji_palette::missingGlyphLabel(covered).empty(), "covered emoji produced a label");

    const std::array<std::pair<std::string_view, std::uint32_t>, 3> reported = {
        std::pair<std::string_view, std::uint32_t>{"🫪", 0x1FAEA},
        std::pair<std::string_view, std::uint32_t>{"🫯", 0x1FAEF},
        std::pair<std::string_view, std::uint32_t>{"🫈", 0x1FAC8},
    };
    for (const auto& [sequence, codepoint] : reported) {
        const auto report = emoji_palette::inspectGlyphCoverage(sequence, emoji16Font);
        require(!report.renderable, "Emoji 17.0 addition reported as renderable");
        require(report.missingCodepoint == codepoint, "wrong missing code point reported");
    }
    require(emoji_palette::missingGlyphLabel(
                emoji_palette::inspectGlyphCoverage("🫪", emoji16Font)) == "1FAEA",
            "missing glyph label formatting failed");

    // U+1FAEF is the joiner element of every wrestling sequence.
    const auto wrestling =
        emoji_palette::inspectGlyphCoverage("🧑🏻‍🫯‍🧑🏼", emoji16Font);
    require(!wrestling.renderable, "uncovered ZWJ element reported as renderable");
    require(wrestling.missingCodepoint == 0x1FAEF, "wrong ZWJ element reported as missing");

    // Joiners, variation selectors and tag characters never carry a glyph of their own.
    const auto baseOnly = [](std::uint32_t codepoint) {
        return codepoint == 0x1F3F3 || codepoint == 0x1F308;
    };
    require(emoji_palette::inspectGlyphCoverage("🏳️‍🌈", baseOnly).renderable,
            "variation selector treated as a missing glyph");
    const auto flagOnly = [](std::uint32_t codepoint) { return codepoint == 0x1F3F4; };
    require(
        emoji_palette::inspectGlyphCoverage("🏴󠁧󠁢󠁥󠁮󠁧󠁿", flagOnly).renderable,
        "tag characters treated as missing glyphs");

    emoji_palette::EmojiCatalog catalog;
    std::size_t unrenderable = 0;
    for (const auto& record : catalog.records()) {
        const auto report = emoji_palette::inspectGlyphCoverage(record.sequence, emoji16Font);
        require(report.renderable != report.missingCodepoint.has_value(),
                "inconsistent glyph coverage report");
        if (!report.renderable) {
            require(!emoji_palette::missingGlyphLabel(report).empty(),
                    "unrenderable entry produced no label");
            ++unrenderable;
        }
    }
    require(unrenderable > 0, "an Emoji 16.0 font reported full Emoji 17.0 coverage");

    const auto always = [](std::uint32_t) { return true; };
    for (const auto& record : catalog.records()) {
        require(emoji_palette::inspectGlyphCoverage(record.sequence, always).renderable,
                "complete font coverage still reported a missing glyph");
    }
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

void testCaretConversion() {
    using emoji_palette::absentCaret;
    using emoji_palette::logicalCaret;
    using emoji_palette::Rect;
    using emoji_palette::sanitizedCaret;

    require(!logicalCaret(absentCaret, 100).has_value(),
            "an absent caret rectangle must not be treated as a caret at the origin");
    require(!logicalCaret({100, 100, 2, 20}, 49).has_value(), "a scale below the floor was used");
    require(!logicalCaret({100, 100, 2, 20}, 401).has_value(),
            "a scale above the ceiling was used");
    require(!logicalCaret({emoji_palette::coordinateLimit + 1, 0, 2, 20}, 100).has_value(),
            "an out-of-range caret rectangle was accepted");

    require(logicalCaret({100, 200, 2, 20}, 100) == Rect{100, 200, 2, 20},
            "unscaled conversion changed the rectangle");
    require(logicalCaret({125, 250, 3, 25}, 125) == Rect{100, 200, 2, 20},
            "125 percent conversion failed");
    require(logicalCaret({150, 300, 3, 30}, 150) == Rect{100, 200, 2, 20},
            "150 percent conversion failed");
    require(logicalCaret({200, 400, 4, 40}, 200) == Rect{100, 200, 2, 20},
            "200 percent conversion failed");
    require(logicalCaret({-1920, -300, 3, 30}, 150) == Rect{-1280, -200, 2, 20},
            "negative-origin conversion failed");
    require(logicalCaret({151, 0, 0, 0}, 150) == Rect{101, 0, 0, 0},
            "conversion did not round to the nearest logical pixel");

    require(sanitizedCaret({10, 20, 2, 20}) == Rect{10, 20, 2, 20},
            "a valid caret rectangle was altered");
    // A frontend that reports an unknown extent as a zero bottom-right corner
    // yields negative dimensions the wire format would reject.
    require(sanitizedCaret({10, 20, -10, -20}) == Rect{10, 20, 0, 0},
            "negative caret dimensions were not clamped");
    require(sanitizedCaret({emoji_palette::coordinateLimit + 1, 0, 2, 20}) == absentCaret,
            "an out-of-range caret rectangle was not rejected");
    require(sanitizedCaret({0, 0, 0, 0}) == absentCaret, "the absent sentinel was not preserved");
}

void testOutputSelection() {
    using emoji_palette::outputForCaret;
    using emoji_palette::Rect;

    const std::array<Rect, 3> outputs{Rect{-1920, 0, 1920, 1080}, Rect{0, 0, 2560, 1440},
                                      Rect{2560, 200, 1920, 1080}};
    const std::span<const Rect> view{outputs};

    require(outputForCaret({-100, 500, 2, 20}, view) == std::size_t{0},
            "a caret on the negative-origin output selected another output");
    require(outputForCaret({10, 10, 2, 20}, view) == std::size_t{1},
            "a caret on the primary output selected another output");
    require(outputForCaret({3000, 400, 2, 20}, view) == std::size_t{2},
            "a caret on the right-hand output selected another output");
    // A caret above the right-hand output lies in no output at all.
    require(outputForCaret({3000, 0, 2, 20}, view) == std::size_t{2},
            "a caret in a gap did not select the nearest output");
    require(outputForCaret({9000, 9000, 2, 20}, view) == std::size_t{2},
            "a caret beyond every output did not select the nearest output");
    require(outputForCaret({-9000, 500, 2, 20}, view) == std::size_t{0},
            "a caret left of every output did not select the nearest output");

    const std::array<Rect, 2> empty{Rect{0, 0, 0, 0}, Rect{0, 0, 1920, 1080}};
    require(outputForCaret({10, 10, 2, 20}, std::span<const Rect>{empty}) == std::size_t{1},
            "an empty output was selected");
    require(!outputForCaret({10, 10, 2, 20}, std::span<const Rect>{}).has_value(),
            "an output was selected from an empty list");
}

void testPopupEdges() {
    using emoji_palette::centeredPopup;
    using emoji_palette::placePopup;
    using emoji_palette::Point;
    using emoji_palette::Rect;
    using emoji_palette::Size;

    const Rect screen{0, 0, 1920, 1080};
    const Size popup{500, 400};

    require(placePopup({0, 0, 2, 20}, popup, screen) ==
                emoji_palette::PopupPlacement{{0, 20}, true},
            "top-left corner placement failed");
    require(placePopup({1919, 0, 2, 20}, popup, screen).position == Point{1420, 20},
            "top-right corner placement failed");
    require(placePopup({0, 1079, 2, 20}, popup, screen).position == Point{0, 679},
            "bottom-left corner placement failed");
    require(placePopup({1919, 1079, 2, 20}, popup, screen).position == Point{1420, 679},
            "bottom-right corner placement failed");
    require(!placePopup({500, 900, 2, 20}, popup, screen).belowCaret,
            "a caret near the bottom edge did not flip the popup above it");
    require(placePopup({500, 900, 2, 20}, popup, screen).position == Point{500, 500},
            "the flipped popup was not placed directly above the caret");

    // Neither side fits: the larger remainder wins instead of always going down.
    const Rect narrow{0, 0, 1920, 500};
    require(placePopup({100, 460, 2, 20}, popup, narrow).belowCaret == false,
            "the popup did not keep the larger visible remainder above the caret");
    require(placePopup({100, 20, 2, 20}, popup, narrow).belowCaret,
            "the popup did not keep the larger visible remainder below the caret");

    require(centeredPopup(popup, screen) == Point{710, 340}, "centered fallback failed");
    require(centeredPopup(popup, {-1920, 0, 1920, 1080}) == Point{-1210, 340},
            "centered fallback on a negative-origin output failed");
    require(centeredPopup({3000, 2000}, screen) == Point{0, 0},
            "an oversized popup was not clamped to the output origin");
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
        testGlyphCoverage();
        testGeometry();
        testCaretConversion();
        testOutputSelection();
        testPopupEdges();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
