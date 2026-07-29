#pragma once

#include "emoji_palette/catalog.hpp"
#include "emoji_palette/glyph_coverage.hpp"
#include "emoji_palette/state.hpp"

#include <QAbstractListModel>

#include <string_view>
#include <unordered_map>
#include <vector>

namespace emoji_palette::ui {

class EmojiListModel final : public QAbstractListModel {
    Q_OBJECT

  public:
    enum Role {
        FavoriteRole = Qt::UserRole,
        RenderableRole,
        MissingGlyphLabelRole,
        SequenceRole,
    };

    EmojiListModel(const EmojiCatalog& catalog, const PersistentState& state,
                   QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;

    void setCategory(Category category);
    void setSearch(QString query, Locale locale);
    // An empty probe restores detection against the current application font.
    void setGlyphProbe(GlyphProbe probe);
    // Re-reads the application font, but never replaces an explicitly set probe.
    void refreshSystemGlyphProbe();
    void refresh();

    const EmojiRecord* record(int row) const;
    bool isFavorite(int row) const;
    Category category() const;
    const QString& query() const;
    GlyphCoverage coverageFor(std::string_view sequence) const;
    QString missingGlyphText(const GlyphCoverage& coverage) const;
    QString annotatedName(const QString& name, const GlyphCoverage& coverage) const;

  private:
    void rebuild();
    const GlyphCoverage& coverageOf(const EmojiRecord& emoji) const;

    const EmojiCatalog& catalog_;
    const PersistentState& state_;
    Category category_ = Category::SmileysEmotion;
    Locale locale_ = Locale::English;
    QString query_;
    std::vector<const EmojiRecord*> records_;
    GlyphProbe glyphProbe_;
    bool usesSystemProbe_ = true;
    // Keyed by catalog record, whose addresses outlive every rebuild.
    mutable std::unordered_map<const EmojiRecord*, GlyphCoverage> coverage_;
};

}
