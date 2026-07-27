#include "generated_catalog.hpp"

namespace emoji_palette::detail {

#include "data/generated/emoji_data.inc"

std::span<const GeneratedEmoji> generatedEmojis() noexcept {
    return {kGeneratedEmoji, kGeneratedEmojiCount};
}

}
