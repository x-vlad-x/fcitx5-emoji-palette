#pragma once

#include "emoji_palette/catalog.hpp"

#include <span>
#include <string_view>

namespace emoji_palette::detail {

struct GeneratedEmoji {
    std::string_view sequence;
    Category category;
    std::string_view subgroup;
    std::string_view englishName;
    std::string_view englishKeywords;
    std::string_view germanName;
    std::string_view germanKeywords;
    std::string_view russianName;
    std::string_view russianKeywords;
    std::string_view baseSequence;
};

std::span<const GeneratedEmoji> generatedEmojis() noexcept;

}
