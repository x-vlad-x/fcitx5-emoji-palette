#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace emoji_palette {

// Reports whether a font can draw a single code point. The probe must answer for
// the exact code point it is given and must not apply its own substitutions.
using GlyphProbe = std::function<bool(std::uint32_t)>;

struct GlyphCoverage {
    bool renderable = true;
    std::optional<std::uint32_t> missingCodepoint;
};

// Joiners, variation selectors and tag characters shape neighbouring code points
// instead of carrying a glyph, so a font that omits them still renders the
// sequence. Treating them as missing would mark almost every ZWJ sequence and
// every subdivision flag unrenderable.
bool isNonRenderingCodepoint(std::uint32_t codepoint);

// Reports the first code point of a fully-qualified UTF-8 emoji sequence that the
// probe cannot draw. Malformed input is reported as unrenderable without a code
// point rather than being passed to the probe.
GlyphCoverage inspectGlyphCoverage(std::string_view sequence, const GlyphProbe& probe);

// Uppercase hexadecimal identity of the missing code point, empty when the
// sequence is renderable.
std::string missingGlyphLabel(const GlyphCoverage& coverage);

}
