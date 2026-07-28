#pragma once

#include "emoji_list_model.hpp"

#include "emoji_palette/geometry.hpp"
#include "emoji_palette/ipc/protocol.hpp"
#include "emoji_palette/keyboard.hpp"
#include "emoji_palette/state.hpp"

#include <QSettings>
#include <QWidget>

#include <filesystem>
#include <memory>
#include <vector>

class QButtonGroup;
class QFrame;
class QLabel;
class QListView;
class QModelIndex;
class QToolButton;

namespace emoji_palette::ui {

class PaletteWindow final : public QWidget {
    Q_OBJECT

  public:
    PaletteWindow(const EmojiCatalog& catalog, std::filesystem::path statePath,
                  const QString& settingsPath, QWidget* parent = nullptr);

    void showPalette(const ipc::Show& request);
    void hidePalette();
    void handleCommand(const ipc::Command& command);

    const EmojiListModel& model() const;

  signals:
    void selectionRequested(QString sequence);
    void cancellationRequested(ipc::CancelReason reason);

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    void buildUi();
    void setSettingsPanelVisible(bool visible);
    void setCategory(std::size_t index);
    void updateSearch(const QString& query);
    void refreshGrid(bool resetSelection);
    void updateSelection();
    void activateCurrent();
    void toggleCurrentFavorite();
    void showCurrentVariants();
    void chooseSequence(std::string_view sequence);
    void positionFor(const ipc::Show& request);
    void applyCellSize(int size);
    void saveState();
    QString categoryLabel(Category category) const;

    const EmojiCatalog& catalog_;
    StateStore stateStore_;
    PersistentState state_;
    std::unique_ptr<QSettings> settings_;
    EmojiListModel model_;
    ipc::Show request_{};
    KeyboardState keyboard_;
    std::vector<Category> categories_;
    std::vector<QToolButton*> categoryButtons_;
    std::size_t categoryIndex_ = 2;
    QListView* grid_ = nullptr;
    QLabel* searchLabel_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QButtonGroup* categoryGroup_ = nullptr;
    QFrame* variantsPanel_ = nullptr;
    QFrame* settingsPanel_ = nullptr;
    QToolButton* settingsButton_ = nullptr;
    QToolButton* favoriteButton_ = nullptr;
    QToolButton* variantsButton_ = nullptr;
    int cellSize_ = 52;
    int columns_ = 9;
    int rows_ = 7;
};

}
