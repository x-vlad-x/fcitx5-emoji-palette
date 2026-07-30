#include "emoji_palette/glyph_coverage.hpp"

#include "emoji_palette/utf8.hpp"

#include <array>

namespace emoji_palette {
namespace {

constexpr std::uint32_t zeroWidthJoiner = 0x200D;
constexpr std::uint32_t variationSelectorFirst = 0xFE00;
constexpr std::uint32_t variationSelectorLast = 0xFE0F;
constexpr std::uint32_t tagFirst = 0xE0020;
constexpr std::uint32_t tagLast = 0xE007F;

}

bool isNonRenderingCodepoint(std::uint32_t codepoint) {
    if (codepoint == zeroWidthJoiner) {
        return true;
    }
    if (codepoint >= variationSelectorFirst && codepoint <= variationSelectorLast) {
        return true;
    }
    return codepoint >= tagFirst && codepoint <= tagLast;
}

GlyphCoverage inspectGlyphCoverage(std::string_view sequence, const GlyphProbe& probe) {
    if (!probe) {
        return {};
    }
    const auto decoded = decodeUtf8(sequence);
    if (!decoded || decoded->empty()) {
        return {false, std::nullopt};
    }
    for (const auto codepoint : *decoded) {
        if (isNonRenderingCodepoint(codepoint)) {
            continue;
        }
        if (!probe(codepoint)) {
            return {false, codepoint};
        }
    }
    return {};
}

std::string missingGlyphLabel(const GlyphCoverage& coverage) {
    if (!coverage.missingCodepoint) {
        return {};
    }
    static constexpr std::array<char, 16> digits = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
    };
    auto value = *coverage.missingCodepoint;
    std::string result;
    do {
        result.insert(result.begin(), digits[value & 0xF]);
        value >>= 4;
    } while (value != 0);
    while (result.size() < 4) {
        result.insert(result.begin(), '0');
    }
    return result;
}

}
