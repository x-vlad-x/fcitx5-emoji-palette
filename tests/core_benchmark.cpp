#include "emoji_palette/catalog.hpp"

#include <array>
#include <chrono>
#include <iostream>
#include <string_view>

int main() {
    const auto loadBegin = std::chrono::steady_clock::now();
    emoji_palette::EmojiCatalog catalog;
    const auto loadEnd = std::chrono::steady_clock::now();
    constexpr std::array<std::string_view, 6> queries = {
        "smile", "family", "flag", "пингвин", "grinsen", "coffee",
    };
    const auto searchBegin = std::chrono::steady_clock::now();
    std::size_t total = 0;
    for (int iteration = 0; iteration < 100; ++iteration) {
        for (const auto query : queries) {
            total += catalog.search(query, emoji_palette::Locale::English, 50).size();
        }
    }
    const auto searchEnd = std::chrono::steady_clock::now();
    const auto loadMilliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(loadEnd - loadBegin).count();
    const auto searchMilliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(searchEnd - searchBegin).count();
    std::cout << "records=" << catalog.records().size() << " load_ms=" << loadMilliseconds
              << " searches=600 search_ms=" << searchMilliseconds << " results=" << total << '\n';
    return 0;
}
