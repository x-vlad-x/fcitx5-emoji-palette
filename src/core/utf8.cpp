#include "emoji_palette/utf8.hpp"

#include <array>
#include <cctype>
#include <charconv>
#include <limits>

namespace emoji_palette {
namespace {

bool isScalar(std::uint32_t value) {
    return value <= 0x10FFFF && !(value >= 0xD800 && value <= 0xDFFF);
}

std::uint32_t foldCodepoint(std::uint32_t value) {
    if (value >= 'A' && value <= 'Z') {
        return value + 0x20;
    }
    if (value >= 0x0410 && value <= 0x042F) {
        return value + 0x20;
    }
    switch (value) {
    case 0x00C4:
        return 0x00E4;
    case 0x00D6:
        return 0x00F6;
    case 0x00DC:
        return 0x00FC;
    case 0x1E9E:
        return 0x00DF;
    case 0x0401:
        return 0x0451;
    case '-':
    case '_':
        return ' ';
    default:
        return value;
    }
}

int hexValue(char value) {
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    return -1;
}

} // namespace

std::optional<std::vector<std::uint32_t>> decodeUtf8(std::string_view value) {
    std::vector<std::uint32_t> result;
    result.reserve(value.size());
    for (std::size_t index = 0; index < value.size();) {
        const auto first = static_cast<unsigned char>(value[index]);
        std::uint32_t codepoint = 0;
        std::size_t length = 0;
        std::uint32_t minimum = 0;
        if (first < 0x80) {
            codepoint = first;
            length = 1;
        } else if ((first & 0xE0) == 0xC0) {
            codepoint = first & 0x1F;
            length = 2;
            minimum = 0x80;
        } else if ((first & 0xF0) == 0xE0) {
            codepoint = first & 0x0F;
            length = 3;
            minimum = 0x800;
        } else if ((first & 0xF8) == 0xF0) {
            codepoint = first & 0x07;
            length = 4;
            minimum = 0x10000;
        } else {
            return std::nullopt;
        }
        if (index + length > value.size()) {
            return std::nullopt;
        }
        for (std::size_t offset = 1; offset < length; ++offset) {
            const auto continuation = static_cast<unsigned char>(value[index + offset]);
            if ((continuation & 0xC0) != 0x80) {
                return std::nullopt;
            }
            codepoint = (codepoint << 6) | (continuation & 0x3F);
        }
        if (codepoint < minimum || !isScalar(codepoint)) {
            return std::nullopt;
        }
        result.push_back(codepoint);
        index += length;
    }
    return result;
}

bool isValidUtf8(std::string_view value) { return decodeUtf8(value).has_value(); }

std::string encodeUtf8(const std::vector<std::uint32_t>& codepoints) {
    std::string result;
    for (const auto codepoint : codepoints) {
        if (!isScalar(codepoint)) {
            return {};
        }
        if (codepoint <= 0x7F) {
            result.push_back(static_cast<char>(codepoint));
        } else if (codepoint <= 0x7FF) {
            result.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
            result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else if (codepoint <= 0xFFFF) {
            result.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
            result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else {
            result.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
            result.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        }
    }
    return result;
}

std::string normalizeForSearch(std::string_view value) {
    const auto decoded = decodeUtf8(value);
    if (!decoded) {
        return {};
    }
    std::vector<std::uint32_t> normalized;
    normalized.reserve(decoded->size());
    bool previousSpace = true;
    for (const auto original : *decoded) {
        auto codepoint = foldCodepoint(original);
        const bool isSpace =
            codepoint == ' ' || codepoint == '\t' || codepoint == '\n' || codepoint == '\r';
        if (isSpace) {
            if (!previousSpace) {
                normalized.push_back(' ');
            }
            previousSpace = true;
            continue;
        }
        normalized.push_back(codepoint);
        previousSpace = false;
    }
    if (!normalized.empty() && normalized.back() == ' ') {
        normalized.pop_back();
    }
    return encodeUtf8(normalized);
}

std::string hexEncode(std::string_view value) {
    static constexpr std::array<char, 16> digits = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f',
    };
    std::string result;
    result.reserve(value.size() * 2);
    for (const auto character : value) {
        const auto byte = static_cast<unsigned char>(character);
        result.push_back(digits[byte >> 4]);
        result.push_back(digits[byte & 0x0F]);
    }
    return result;
}

std::optional<std::string> hexDecode(std::string_view value) {
    if (value.size() % 2 != 0) {
        return std::nullopt;
    }
    std::string result;
    result.reserve(value.size() / 2);
    for (std::size_t index = 0; index < value.size(); index += 2) {
        const int high = hexValue(value[index]);
        const int low = hexValue(value[index + 1]);
        if (high < 0 || low < 0) {
            return std::nullopt;
        }
        result.push_back(static_cast<char>((high << 4) | low));
    }
    if (!isValidUtf8(result)) {
        return std::nullopt;
    }
    return result;
}

} // namespace emoji_palette
