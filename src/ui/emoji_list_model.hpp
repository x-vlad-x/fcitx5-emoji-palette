#pragma once

#include "emoji_palette/catalog.hpp"
#include "emoji_palette/state.hpp"

#include <QAbstractListModel>

#include <vector>

namespace emoji_palette::ui {

class EmojiListModel final : public QAbstractListModel {
    Q_OBJECT

  public:
    EmojiListModel(const EmojiCatalog& catalog, const PersistentState& state,
                   QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;

    void setCategory(Category category);
    void setSearch(QString query, Locale locale);
    void refresh();

    const EmojiRecord* record(int row) const;
    bool isFavorite(int row) const;
    Category category() const;
    const QString& query() const;

  private:
    void rebuild();

    const EmojiCatalog& catalog_;
    const PersistentState& state_;
    Category category_ = Category::SmileysEmotion;
    Locale locale_ = Locale::English;
    QString query_;
    std::vector<const EmojiRecord*> records_;
};

}
