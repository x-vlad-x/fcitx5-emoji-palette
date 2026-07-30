#include "font_coverage.hpp"

#include <QFontMetricsF>

#include <memory>

namespace emoji_palette::ui {

FontCoverage::FontCoverage(const QFont& font) : font_(font) {}

bool FontCoverage::covers(std::uint32_t codepoint) const {
    const auto cached = cache_.find(codepoint);
    if (cached != cache_.end()) {
        return cached->second;
    }
    const QFontMetricsF metrics(font_);
    const bool supported = metrics.inFontUcs4(static_cast<uint>(codepoint));
    cache_.emplace(codepoint, supported);
    return supported;
}

GlyphProbe systemGlyphProbe(const QFont& font) {
    auto coverage = std::make_shared<FontCoverage>(font);
    return [coverage](std::uint32_t codepoint) { return coverage->covers(codepoint); };
}

}
