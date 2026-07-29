#pragma once

#include "emoji_palette/glyph_coverage.hpp"

#include <QFont>

#include <cstdint>
#include <unordered_map>

namespace emoji_palette::ui {

// Resolves glyph availability through the Qt font fallback chain. QFontMetricsF
// consults every fallback family that fontconfig offers, which is what actually
// draws emoji; QRawFont deliberately reports the primary family only and would
// mark every emoji unrenderable.
class FontCoverage {
  public:
    explicit FontCoverage(const QFont& font);

    bool covers(std::uint32_t codepoint) const;

  private:
    QFont font_;
    mutable std::unordered_map<std::uint32_t, bool> cache_;
};

// Builds a probe that keeps the coverage object alive for as long as any model
// still holds the probe.
GlyphProbe systemGlyphProbe(const QFont& font);

}
