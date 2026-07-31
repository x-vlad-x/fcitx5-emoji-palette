#include "emoji_palette/catalog.hpp"
#include "emoji_palette/geometry.hpp"
#include "emoji_palette/glyph_coverage.hpp"
#include "emoji_palette/keyboard.hpp"
#include "emoji_palette/state.hpp"
#include "emoji_palette/utf8.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unistd.h>
#include <utility>
#include <vector>

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
    require(emoji_palette::normalizeForSearch("LO\u0308WE") ==
                emoji_palette::normalizeForSearch("LÖWE"),
            "decomposed German folding failed");
    require(emoji_palette::normalizeForSearch("И\u0306ОГА") == "йога",
            "decomposed Russian folding failed");
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

void testLocalizedSearch() {
    emoji_palette::EmojiCatalog catalog;

    const std::array<emoji_palette::Locale, 3> locales = {
        emoji_palette::Locale::English,
        emoji_palette::Locale::German,
        emoji_palette::Locale::Russian,
    };

    const auto sequences = [&catalog](std::string_view query, emoji_palette::Locale locale,
                                      std::size_t limit) {
        std::vector<std::string> found;
        for (const auto& entry : catalog.search(query, locale, limit)) {
            found.emplace_back(entry.emoji->sequence);
        }
        return found;
    };
    const auto listed = [](const std::vector<std::string>& results, std::string_view sequence) {
        return std::find(results.begin(), results.end(), sequence) != results.end();
    };

    struct LocalizedCase {
        std::string_view query;
        std::string_view sequence;
    };
    const std::array<LocalizedCase, 9> criteria = {{
        {"heart", "❤️"},
        {"Herz", "❤️"},
        {"сердце", "❤️"},
        {"cat", "🐈"},
        {"Katze", "🐈"},
        {"кот", "🐈"},
        {"fire", "🔥"},
        {"Feuer", "🔥"},
        {"огонь", "🔥"},
    }};
    for (const auto locale : locales) {
        for (const auto& entry : criteria) {
            require(listed(sequences(entry.query, locale, 50), entry.sequence),
                    "localized query " + std::string(entry.query) + " found no match");
        }
    }

    // Localized keywords, not only localized names, are searchable everywhere.
    require(listed(sequences("Flamme", emoji_palette::Locale::Russian, 20), "🔥"),
            "German keyword was not searchable under a Russian locale");
    require(listed(sequences("костер", emoji_palette::Locale::German, 20), "🔥"),
            "Russian keyword was not searchable under a German locale");
    require(listed(sequences("litaf", emoji_palette::Locale::German, 20), "🔥"),
            "English keyword was not searchable under a German locale");

    // An exact localized name still ranks first for a foreign requested locale.
    const auto katze = sequences("Katze", emoji_palette::Locale::English, 10);
    require(!katze.empty() && katze.front() == "🐈", "German name ranking failed");
    const auto fire = sequences("огонь", emoji_palette::Locale::English, 10);
    require(!fire.empty() && fire.front() == "🔥", "Russian name ranking failed");
    const auto feuer = sequences("Feuer", emoji_palette::Locale::Russian, 10);
    require(!feuer.empty() && feuer.front() == "🔥", "German name ranking failed");

    // Case folding and normalization are independent of the requested locale.
    const auto identical = [&sequences](std::string_view left, std::string_view right,
                                        emoji_palette::Locale locale) {
        const auto folded = sequences(left, locale, 20);
        return !folded.empty() && folded == sequences(right, locale, 20);
    };
    require(identical("HERZ", "herz", emoji_palette::Locale::English),
            "German case folding failed");
    require(identical("СЕРДЦЕ", "сердце", emoji_palette::Locale::English),
            "Russian case folding failed");
    require(identical("FIRE", "fire", emoji_palette::Locale::Russian),
            "English case folding failed");
    require(identical("LÖWE", "löwe", emoji_palette::Locale::Russian),
            "German umlaut case folding failed");
    require(identical("lo\u0308we", "löwe", emoji_palette::Locale::English),
            "decomposed German query was not normalized");
    require(identical("воздушныи\u0306", "воздушный", emoji_palette::Locale::German),
            "decomposed Russian query was not normalized");

    // A missing localized annotation falls back instead of blocking a match.
    const std::array<emoji_palette::LocalizedAnnotation, emoji_palette::localeCount> partial = {{
        {"red heart", {"emotion", "heart"}},
        {"", {}},
        {"", {""}},
    }};
    const auto sparse = emoji_palette::buildSearchDocument(partial);
    const auto heartToken = emoji_palette::normalizeForSearch("heart");
    const auto herzToken = emoji_palette::normalizeForSearch("herz");
    for (const auto locale : locales) {
        require(emoji_palette::scoreSearchToken(heartToken, sparse, locale) >= 0,
                "a missing localized annotation blocked the English fallback");
        require(emoji_palette::scoreSearchToken(herzToken, sparse, locale) < 0,
                "an empty localized annotation produced a match");
    }

    // The requested locale ranks matches; it never hides them.
    const std::array<emoji_palette::LocalizedAnnotation, emoji_palette::localeCount> complete = {{
        {"red heart", {"emotion", "heart", "love", "red"}},
        {"rotes Herz", {"Herz", "rotes Herz"}},
        {"алое сердце", {"красное", "любовь", "сердце"}},
    }};
    const auto document = emoji_palette::buildSearchDocument(complete);
    const auto serdceToken = emoji_palette::normalizeForSearch("сердце");
    require(
        emoji_palette::scoreSearchToken(herzToken, document, emoji_palette::Locale::German) >
            emoji_palette::scoreSearchToken(herzToken, document, emoji_palette::Locale::English),
        "the requested locale did not rank ahead of a foreign locale");
    for (const auto locale : locales) {
        require(emoji_palette::scoreSearchToken(serdceToken, document, locale) >= 0,
                "a Russian token did not match under every locale");
        require(emoji_palette::scoreSearchToken(emoji_palette::normalizeForSearch("umbrella"),
                                                document, locale) < 0,
                "an unrelated token matched");
    }

    // Index construction and ranking are deterministic and need no network access.
    const auto repeated = emoji_palette::buildSearchDocument(complete);
    for (std::size_t locale = 0; locale < emoji_palette::localeCount; ++locale) {
        require(repeated.locales[locale].name == document.locales[locale].name &&
                    repeated.locales[locale].keywords == document.locales[locale].keywords,
                "search index construction is not deterministic");
    }
    const emoji_palette::EmojiCatalog rebuilt;
    for (const auto locale : locales) {
        for (const auto& entry : criteria) {
            const auto original = catalog.search(entry.query, locale, 50);
            const auto again = rebuilt.search(entry.query, locale, 50);
            require(original.size() == again.size(), "search results are not reproducible");
            for (std::size_t index = 0; index < original.size(); ++index) {
                require(original[index].emoji->sequence == again[index].emoji->sequence &&
                            original[index].score == again[index].score,
                        "search ranking is not reproducible");
            }
        }
    }
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

}

int main() {
    try {
        testUtf8();
        testCatalog();
        testSearch();
        testLocalizedSearch();
        testVariants();
        testState();
        testKeyboard();
        testGlyphCoverage();
        testGeometry();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
