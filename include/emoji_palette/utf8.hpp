#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace emoji_palette {

bool isValidUtf8(std::string_view value);
std::optional<std::vector<std::uint32_t>> decodeUtf8(std::string_view value);
std::string encodeUtf8(const std::vector<std::uint32_t>& codepoints);
std::string normalizeForSearch(std::string_view value);
std::string hexEncode(std::string_view value);
std::optional<std::string> hexDecode(std::string_view value);

}
