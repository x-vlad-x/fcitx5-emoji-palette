#include "emoji_list_model.hpp"

#include "font_coverage.hpp"

#include <QGuiApplication>
#include <QString>

#include <algorithm>
#include <string>
#include <utility>

namespace emoji_palette::ui {

namespace {

QString fromUtf8(std::string_view value) {
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

}

EmojiListModel::EmojiListModel(const EmojiCatalog& catalog, const PersistentState& state,
                               QObject* parent)
    : QAbstractListModel(parent), catalog_(catalog), state_(state),
      glyphProbe_(systemGlyphProbe(QGuiApplication::font())) {
    rebuild();
}

int EmojiListModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(records_.size());
}

QVariant EmojiListModel::data(const QModelIndex& index, int role) const {
    const auto* emoji = record(index.row());
    if (emoji == nullptr) {
        return {};
    }
    const auto localeIndex = static_cast<std::size_t>(locale_);
    if (role == Qt::DisplayRole || role == SequenceRole) {
        return fromUtf8(emoji->sequence);
    }
    if (role == Qt::ToolTipRole || role == Qt::AccessibleTextRole) {
        return annotatedName(fromUtf8(emoji->annotations[localeIndex].name), coverageOf(*emoji));
    }
    if (role == Qt::AccessibleDescriptionRole) {
        QStringList keywords;
        for (const auto& keyword : emoji->annotations[localeIndex].keywords) {
            keywords.push_back(fromUtf8(keyword));
        }
        return keywords.join(QStringLiteral(", "));
    }
    if (role == FavoriteRole) {
        return isFavorite(index.row());
    }
    if (role == RenderableRole) {
        return coverageOf(*emoji).renderable;
    }
    if (role == MissingGlyphLabelRole) {
        return missingGlyphText(coverageOf(*emoji));
    }
    return {};
}

void EmojiListModel::setCategory(Category category) {
    if (category_ == category && query_.isEmpty()) {
        return;
    }
    category_ = category;
    rebuild();
}

void EmojiListModel::setSearch(QString query, Locale locale) {
    if (query_ == query && locale_ == locale) {
        return;
    }
    query_ = std::move(query);
    locale_ = locale;
    rebuild();
}

void EmojiListModel::setGlyphProbe(GlyphProbe probe) {
    usesSystemProbe_ = !probe;
    glyphProbe_ = probe ? std::move(probe) : systemGlyphProbe(QGuiApplication::font());
    coverage_.clear();
    if (!records_.empty()) {
        emit dataChanged(index(0, 0), index(static_cast<int>(records_.size()) - 1, 0));
    }
}

void EmojiListModel::refreshSystemGlyphProbe() {
    if (!usesSystemProbe_) {
        return;
    }
    setGlyphProbe({});
}

void EmojiListModel::refresh() { rebuild(); }

const EmojiRecord* EmojiListModel::record(int row) const {
    if (row < 0 || static_cast<std::size_t>(row) >= records_.size()) {
        return nullptr;
    }
    return records_[static_cast<std::size_t>(row)];
}

bool EmojiListModel::isFavorite(int row) const {
    const auto* emoji = record(row);
    return emoji != nullptr && std::find(state_.favorites.begin(), state_.favorites.end(),
                                         emoji->sequence) != state_.favorites.end();
}

Category EmojiListModel::category() const { return category_; }

const QString& EmojiListModel::query() const { return query_; }

GlyphCoverage EmojiListModel::coverageFor(std::string_view sequence) const {
    return inspectGlyphCoverage(sequence, glyphProbe_);
}

QString EmojiListModel::missingGlyphText(const GlyphCoverage& coverage) const {
    if (coverage.renderable) {
        return {};
    }
    const auto label = missingGlyphLabel(coverage);
    return label.empty() ? QStringLiteral("?") : QString::fromStdString(label);
}

QString EmojiListModel::annotatedName(const QString& name, const GlyphCoverage& coverage) const {
    if (coverage.renderable) {
        return name;
    }
    return tr("%1 (no glyph in the installed emoji font)").arg(name);
}

const GlyphCoverage& EmojiListModel::coverageOf(const EmojiRecord& emoji) const {
    const auto cached = coverage_.find(&emoji);
    if (cached != coverage_.end()) {
        return cached->second;
    }
    return coverage_.emplace(&emoji, inspectGlyphCoverage(emoji.sequence, glyphProbe_))
        .first->second;
}

void EmojiListModel::rebuild() {
    beginResetModel();
    records_.clear();
    if (!query_.isEmpty()) {
        const auto encoded = query_.toUtf8();
        for (const auto& result : catalog_.search(
                 std::string_view(encoded.constData(), static_cast<std::size_t>(encoded.size())),
                 locale_, 500)) {
            records_.push_back(result.emoji);
        }
    } else if (category_ == Category::RecentlyUsed) {
        for (const auto& recent : state_.recents) {
            if (const auto* emoji = catalog_.find(recent.sequence)) {
                records_.push_back(emoji);
            }
        }
    } else if (category_ == Category::Favorites) {
        for (const auto& favorite : state_.favorites) {
            if (const auto* emoji = catalog_.find(favorite)) {
                records_.push_back(emoji);
            }
        }
    } else {
        for (const auto& emoji : catalog_.records()) {
            if (emoji.category == category_) {
                records_.push_back(&emoji);
            }
        }
    }
    endResetModel();
}

}
