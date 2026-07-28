#include "palette_window.hpp"

#include <LayerShellQt/Window>

#include <QAbstractItemView>
#include <QApplication>
#include <QButtonGroup>
#include <QCursor>
#include <QDateTime>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QLoggingCategory>
#include <QPainter>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>

#include <algorithm>
#include <array>
#include <chrono>
#include <string>

namespace emoji_palette::ui {

namespace {

Q_LOGGING_CATEGORY(logState, "org.fcitx.EmojiPalette.state")

QString fromUtf8(std::string_view value) {
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

class EmojiDelegate final : public QStyledItemDelegate {
  public:
    explicit EmojiDelegate(int cellSize, QObject* parent = nullptr)
        : QStyledItemDelegate(parent), cellSize_(cellSize) {}

    void setCellSize(int size) { cellSize_ = size; }

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override {
        return {cellSize_, cellSize_};
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        QStyleOptionViewItem item(option);
        initStyleOption(&item, index);
        item.text.clear();
        const auto* widget = option.widget;
        const auto* style = widget == nullptr ? QApplication::style() : widget->style();
        style->drawControl(QStyle::CE_ItemViewItem, &item, painter, widget);

        painter->save();
        QFont font = option.font;
        font.setPointSizeF(std::max(14.0, static_cast<double>(cellSize_) * 0.43));
        painter->setFont(font);
        painter->setPen(option.palette.color(option.state.testFlag(QStyle::State_Selected)
                                                 ? QPalette::HighlightedText
                                                 : QPalette::Text));
        painter->drawText(option.rect, Qt::AlignCenter, index.data(Qt::DisplayRole).toString());
        if (index.data(Qt::UserRole).toBool()) {
            QFont markerFont = option.font;
            markerFont.setPointSizeF(std::max(7.0, static_cast<double>(cellSize_) * 0.17));
            painter->setFont(markerFont);
            painter->drawText(option.rect.adjusted(3, 2, -3, -2), Qt::AlignTop | Qt::AlignRight,
                              QStringLiteral("★"));
        }
        painter->restore();
    }

  private:
    int cellSize_;
};

constexpr std::array<std::string_view, 12> categoryIcons = {
    "🕘", "★", "😀", "🧑", "🐻", "🍎", "🚗", "⚽", "💡", "♥️", "🏳️", "ツ",
};

void migrateSettings(QSettings& settings) {
    const int version = settings.value(QStringLiteral("SchemaVersion"), 0).toInt();
    if (version != 0) {
        return;
    }
    const std::array<std::pair<QString, QString>, 3> keys = {
        {{QStringLiteral("cellSize"), QStringLiteral("Ui/CellSize")},
         {QStringLiteral("columns"), QStringLiteral("Ui/Columns")},
         {QStringLiteral("rows"), QStringLiteral("Ui/Rows")}}};
    for (const auto& [oldKey, newKey] : keys) {
        if (settings.contains(oldKey) && !settings.contains(newKey)) {
            settings.setValue(newKey, settings.value(oldKey));
        }
        settings.remove(oldKey);
    }
    settings.setValue(QStringLiteral("SchemaVersion"), 1);
    settings.sync();
}

}

PaletteWindow::PaletteWindow(const EmojiCatalog& catalog, std::filesystem::path statePath,
                             const QString& settingsPath, QWidget* parent)
    : QWidget(parent), catalog_(catalog), stateStore_(std::move(statePath)),
      settings_(std::make_unique<QSettings>(settingsPath, QSettings::IniFormat)),
      model_(catalog_, state_, this),
      categories_{Category::RecentlyUsed, Category::Favorites,     Category::SmileysEmotion,
                  Category::PeopleBody,   Category::AnimalsNature, Category::FoodDrink,
                  Category::TravelPlaces, Category::Activities,    Category::Objects,
                  Category::Symbols,      Category::Flags,         Category::Kaomoji} {
    const auto loadedState = stateStore_.load();
    state_ = loadedState.state;
    model_.refresh();
    if (loadedState.recoveredFromCorruption) {
        qCWarning(logState) << "Recovered from an invalid state file";
    }
    setWindowTitle(tr("Emoji Palette"));
    setWindowFlag(Qt::Tool);
    setWindowFlag(Qt::FramelessWindowHint);
    setWindowFlag(Qt::WindowDoesNotAcceptFocus);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_QuitOnClose, false);
    settings_->setAtomicSyncRequired(true);
    migrateSettings(*settings_);
    if (settings_->status() != QSettings::NoError) {
        qCWarning(logState) << "Could not migrate the UI settings";
    }
    cellSize_ = std::clamp(settings_->value(QStringLiteral("Ui/CellSize"), 52).toInt(), 40, 72);
    columns_ = std::clamp(settings_->value(QStringLiteral("Ui/Columns"), 9).toInt(), 6, 14);
    rows_ = std::clamp(settings_->value(QStringLiteral("Ui/Rows"), 7).toInt(), 4, 10);
    buildUi();
    qApp->installEventFilter(this);
    setCategory(categoryIndex_);
}

void PaletteWindow::showPalette(const ipc::Show& request) {
    const bool preserveSearch = isVisible();
    request_ = request;
    if (!preserveSearch) {
        updateSearch({});
    }
    positionFor(request);
    show();
    raise();
}

void PaletteWindow::hidePalette() {
    setSettingsPanelVisible(false);
    hide();
    variantsPanel_->hide();
}

void PaletteWindow::handleCommand(const ipc::Command& command) {
    if (command.kind == ipc::CommandKind::SearchText) {
        updateSearch(
            QString::fromUtf8(command.text.data(), static_cast<qsizetype>(command.text.size())));
        return;
    }
    if (command.kind == ipc::CommandKind::ToggleFavorite) {
        toggleCurrentFavorite();
        return;
    }
    if (command.kind == ipc::CommandKind::ShowVariants) {
        showCurrentVariants();
        return;
    }
    if (command.kind == ipc::CommandKind::Cancel) {
        if (settingsPanel_->isVisible()) {
            setSettingsPanelVisible(false);
            return;
        }
        emit cancellationRequested(ipc::CancelReason::User);
        return;
    }

    NavigationCommand navigation;
    switch (command.kind) {
    case ipc::CommandKind::Left:
        navigation = NavigationCommand::Left;
        break;
    case ipc::CommandKind::Right:
        navigation = NavigationCommand::Right;
        break;
    case ipc::CommandKind::Up:
        navigation = NavigationCommand::Up;
        break;
    case ipc::CommandKind::Down:
        navigation = NavigationCommand::Down;
        break;
    case ipc::CommandKind::Home:
        navigation = NavigationCommand::Home;
        break;
    case ipc::CommandKind::End:
        navigation = NavigationCommand::End;
        break;
    case ipc::CommandKind::PageUp:
        navigation = NavigationCommand::PageUp;
        break;
    case ipc::CommandKind::PageDown:
        navigation = NavigationCommand::PageDown;
        break;
    case ipc::CommandKind::PreviousCategory:
        navigation = NavigationCommand::PreviousCategory;
        break;
    case ipc::CommandKind::NextCategory:
        navigation = NavigationCommand::NextCategory;
        break;
    case ipc::CommandKind::Select:
        navigation = NavigationCommand::Select;
        break;
    default:
        return;
    }

    const auto outcome = keyboard_.dispatch(navigation);
    if (outcome == NavigationOutcome::CategoryChanged) {
        setCategory(keyboard_.categoryIndex());
    } else if (outcome == NavigationOutcome::Selected) {
        activateCurrent();
    } else if (outcome == NavigationOutcome::Changed) {
        updateSelection();
    }
}

const EmojiListModel& PaletteWindow::model() const { return model_; }

bool PaletteWindow::eventFilter(QObject* watched, QEvent* event) {
    if (settingsPanel_ != nullptr && settingsPanel_->isVisible() &&
        event->type() == QEvent::MouseButtonPress) {
        auto* widget = qobject_cast<QWidget*>(watched);
        const bool insidePanel =
            widget != nullptr && (widget == settingsPanel_ || settingsPanel_->isAncestorOf(widget));
        const bool onButton = widget != nullptr &&
                              (widget == settingsButton_ || settingsButton_->isAncestorOf(widget));
        if (widget != nullptr && !insidePanel && !onButton) {
            setSettingsPanelVisible(false);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void PaletteWindow::buildUi() {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 8, 8, 8);
    outer->setSpacing(6);

    auto* surface = new QFrame(this);
    surface->setFrameShape(QFrame::StyledPanel);
    auto* layout = new QVBoxLayout(surface);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    outer->addWidget(surface);

    auto* searchRow = new QHBoxLayout;
    searchLabel_ = new QLabel(tr("Type to search"), surface);
    searchLabel_->setAccessibleName(tr("Search query"));
    searchRow->addWidget(searchLabel_, 1);

    settingsButton_ = new QToolButton(surface);
    settingsButton_->setText(QStringLiteral("⚙ ▾"));
    settingsButton_->setToolTip(tr("Grid settings"));
    settingsButton_->setAccessibleName(tr("Grid settings"));
    settingsButton_->setCheckable(true);
    settingsButton_->setFocusPolicy(Qt::NoFocus);
    searchRow->addWidget(settingsButton_);
    layout->addLayout(searchRow);

    settingsPanel_ = new QFrame(surface);
    settingsPanel_->setObjectName(QStringLiteral("settingsPanel"));
    settingsPanel_->setAccessibleName(tr("Grid settings"));
    settingsPanel_->setFrameShape(QFrame::StyledPanel);
    auto* settingsLayout = new QHBoxLayout(settingsPanel_);
    settingsLayout->setContentsMargins(6, 4, 6, 4);
    settingsLayout->setSpacing(4);
    settingsLayout->addWidget(new QLabel(tr("Grid size:"), settingsPanel_));
    auto* sizeGroup = new QButtonGroup(settingsPanel_);
    sizeGroup->setExclusive(true);
    const auto sizes = std::array{
        std::tuple{tr("Compact"), 44, QStringLiteral("settingsSizeCompact")},
        std::tuple{tr("Comfortable"), 52, QStringLiteral("settingsSizeComfortable")},
        std::tuple{tr("Large"), 64, QStringLiteral("settingsSizeLarge")},
    };
    for (const auto& [label, size, objectName] : sizes) {
        auto* button = new QToolButton(settingsPanel_);
        button->setObjectName(objectName);
        button->setText(label);
        button->setAccessibleName(label);
        button->setCheckable(true);
        button->setChecked(cellSize_ == size);
        button->setFocusPolicy(Qt::NoFocus);
        sizeGroup->addButton(button);
        settingsLayout->addWidget(button);
        connect(button, &QToolButton::clicked, this, [this, size]() { applyCellSize(size); });
    }
    settingsLayout->addStretch(1);
    settingsPanel_->hide();
    layout->addWidget(settingsPanel_);
    connect(settingsButton_, &QToolButton::clicked, this,
            [this](bool checked) { setSettingsPanelVisible(checked); });

    auto* categoryScroll = new QScrollArea(surface);
    categoryScroll->setWidgetResizable(true);
    categoryScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    categoryScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    categoryScroll->setFrameShape(QFrame::NoFrame);
    categoryScroll->setFocusPolicy(Qt::NoFocus);
    auto* categoryWidget = new QWidget(categoryScroll);
    auto* categoryLayout = new QHBoxLayout(categoryWidget);
    categoryLayout->setContentsMargins(0, 0, 0, 0);
    categoryLayout->setSpacing(2);
    categoryGroup_ = new QButtonGroup(this);
    categoryGroup_->setExclusive(true);
    for (std::size_t index = 0; index < categories_.size(); ++index) {
        auto* button = new QToolButton(categoryWidget);
        button->setText(fromUtf8(categoryIcons[index]));
        button->setCheckable(true);
        button->setFocusPolicy(Qt::NoFocus);
        button->setToolTip(categoryLabel(categories_[index]));
        button->setAccessibleName(categoryLabel(categories_[index]));
        categoryGroup_->addButton(button, static_cast<int>(index));
        categoryLayout->addWidget(button);
        categoryButtons_.push_back(button);
    }
    categoryLayout->addStretch(1);
    categoryScroll->setWidget(categoryWidget);
    layout->addWidget(categoryScroll);
    connect(categoryGroup_, &QButtonGroup::idClicked, this,
            [this](int index) { setCategory(static_cast<std::size_t>(index)); });

    grid_ = new QListView(surface);
    grid_->setModel(&model_);
    grid_->setItemDelegate(new EmojiDelegate(cellSize_, grid_));
    grid_->setViewMode(QListView::IconMode);
    grid_->setFlow(QListView::LeftToRight);
    grid_->setWrapping(true);
    grid_->setResizeMode(QListView::Adjust);
    grid_->setMovement(QListView::Static);
    grid_->setUniformItemSizes(true);
    grid_->setSpacing(2);
    grid_->setSelectionMode(QAbstractItemView::SingleSelection);
    grid_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    grid_->setFocusPolicy(Qt::NoFocus);
    grid_->setAccessibleName(tr("Emoji grid"));
    grid_->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(grid_, 1);

    connect(grid_, &QListView::clicked, this, [this](const QModelIndex& index) {
        grid_->setCurrentIndex(index);
        if (const auto* emoji = model_.record(index.row())) {
            chooseSequence(emoji->sequence);
        }
    });
    connect(grid_, &QListView::customContextMenuRequested, this, [this](const QPoint& point) {
        const auto index = grid_->indexAt(point);
        if (index.isValid()) {
            grid_->setCurrentIndex(index);
            showCurrentVariants();
        }
    });
    variantsPanel_ = new QFrame(surface);
    variantsPanel_->setFrameShape(QFrame::StyledPanel);
    variantsPanel_->setLayout(new QHBoxLayout);
    variantsPanel_->layout()->setContentsMargins(4, 4, 4, 4);
    variantsPanel_->hide();
    layout->addWidget(variantsPanel_);

    auto* actionRow = new QHBoxLayout;
    statusLabel_ = new QLabel(surface);
    statusLabel_->setAccessibleName(tr("Selected emoji"));
    actionRow->addWidget(statusLabel_, 1);
    favoriteButton_ = new QToolButton(surface);
    favoriteButton_->setText(QStringLiteral("☆"));
    favoriteButton_->setToolTip(tr("Toggle favorite"));
    favoriteButton_->setAccessibleName(tr("Toggle favorite"));
    favoriteButton_->setFocusPolicy(Qt::NoFocus);
    actionRow->addWidget(favoriteButton_);
    variantsButton_ = new QToolButton(surface);
    variantsButton_->setText(QStringLiteral("◌"));
    variantsButton_->setToolTip(tr("Show variants"));
    variantsButton_->setAccessibleName(tr("Show variants"));
    variantsButton_->setFocusPolicy(Qt::NoFocus);
    actionRow->addWidget(variantsButton_);
    layout->addLayout(actionRow);

    connect(favoriteButton_, &QToolButton::clicked, this, &PaletteWindow::toggleCurrentFavorite);
    connect(variantsButton_, &QToolButton::clicked, this, &PaletteWindow::showCurrentVariants);

    resize(columns_ * (cellSize_ + 2) + 38, rows_ * (cellSize_ + 2) + 154);
}

void PaletteWindow::setSettingsPanelVisible(bool visible) {
    settingsPanel_->setVisible(visible);
    const QSignalBlocker blocker(settingsButton_);
    settingsButton_->setChecked(visible);
    settingsButton_->setText(visible ? QStringLiteral("⚙ ▴") : QStringLiteral("⚙ ▾"));
}

void PaletteWindow::setCategory(std::size_t index) {
    if (index >= categories_.size()) {
        return;
    }
    categoryIndex_ = index;
    model_.setCategory(categories_[index]);
    categoryButtons_[index]->setChecked(true);
    refreshGrid(true);
}

void PaletteWindow::updateSearch(const QString& query) {
    searchLabel_->setText(query.isEmpty() ? tr("Type to search") : tr("Search: %1").arg(query));
    model_.setSearch(query, request_.locale);
    refreshGrid(true);
}

void PaletteWindow::refreshGrid(bool resetSelection) {
    if (resetSelection) {
        keyboard_ = KeyboardState{};
    }
    const auto columns =
        std::max(1, grid_->viewport()->width() / std::max(1, cellSize_ + grid_->spacing()));
    const auto visibleRows =
        std::max(1, grid_->viewport()->height() / std::max(1, cellSize_ + grid_->spacing()));
    keyboard_.setGrid(static_cast<std::size_t>(model_.rowCount()),
                      static_cast<std::size_t>(columns), static_cast<std::size_t>(visibleRows));
    keyboard_.setCategory(categoryIndex_, categories_.size());
    updateSelection();
}

void PaletteWindow::updateSelection() {
    if (model_.rowCount() == 0) {
        grid_->setCurrentIndex({});
        statusLabel_->setText(tr("No matching emoji"));
        favoriteButton_->setEnabled(false);
        variantsButton_->setEnabled(false);
        return;
    }
    const auto row = static_cast<int>(keyboard_.selectedIndex());
    const auto index = model_.index(row, 0);
    grid_->setCurrentIndex(index);
    grid_->scrollTo(index, QAbstractItemView::EnsureVisible);
    statusLabel_->setText(index.data(Qt::ToolTipRole).toString());
    favoriteButton_->setEnabled(true);
    favoriteButton_->setText(model_.isFavorite(row) ? QStringLiteral("★") : QStringLiteral("☆"));
    const auto* emoji = model_.record(row);
    variantsButton_->setEnabled(
        emoji != nullptr &&
        catalog_.variantsFor(emoji->baseSequence.empty() ? emoji->sequence : emoji->baseSequence)
                .size() > 1);
}

void PaletteWindow::activateCurrent() {
    const auto* emoji = model_.record(grid_->currentIndex().row());
    if (emoji != nullptr) {
        chooseSequence(emoji->sequence);
    }
}

void PaletteWindow::toggleCurrentFavorite() {
    const auto* emoji = model_.record(grid_->currentIndex().row());
    if (emoji == nullptr) {
        return;
    }
    toggleFavorite(state_, emoji->sequence);
    saveState();
    model_.refresh();
    refreshGrid(false);
}

void PaletteWindow::showCurrentVariants() {
    const auto* emoji = model_.record(grid_->currentIndex().row());
    if (emoji == nullptr) {
        return;
    }
    const auto variants =
        catalog_.variantsFor(emoji->baseSequence.empty() ? emoji->sequence : emoji->baseSequence);
    if (variants.size() <= 1) {
        variantsPanel_->hide();
        return;
    }
    while (auto* item = variantsPanel_->layout()->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    for (const auto* variant : variants) {
        auto* button = new QToolButton(variantsPanel_);
        button->setText(fromUtf8(variant->sequence));
        button->setToolTip(
            fromUtf8(variant->annotations[static_cast<std::size_t>(request_.locale)].name));
        button->setAccessibleName(button->toolTip());
        button->setFocusPolicy(Qt::NoFocus);
        connect(button, &QToolButton::clicked, this,
                [this, sequence = variant->sequence]() { chooseSequence(sequence); });
        variantsPanel_->layout()->addWidget(button);
    }
    variantsPanel_->show();
}

void PaletteWindow::chooseSequence(std::string_view sequence) {
    recordUse(state_, sequence, QDateTime::currentSecsSinceEpoch());
    saveState();
    model_.refresh();
    refreshGrid(false);
    emit selectionRequested(fromUtf8(sequence));
    if (request_.closeAfterSelection) {
        hidePalette();
    }
}

void PaletteWindow::positionFor(const ipc::Show& request) {
    const bool hasCaret = request.caret.x != 0 || request.caret.y != 0 || request.caret.width > 0 ||
                          request.caret.height > 0;
    QScreen* screen = hasCaret ? QGuiApplication::screenAt({request.caret.x, request.caret.y})
                               : QGuiApplication::screenAt(QCursor::pos());
    if (screen == nullptr) {
        screen = QGuiApplication::primaryScreen();
    }
    if (screen == nullptr) {
        return;
    }
    const QRect available = screen->availableGeometry();
    const Size popup{std::min(width(), available.width()), std::min(height(), available.height())};
    resize(popup.width, popup.height);
    const Rect caret =
        hasCaret
            ? Rect{request.caret.x, request.caret.y, request.caret.width, request.caret.height}
            : Rect{available.x() + (available.width() - popup.width) / 2, available.y() + 24, 0, 0};
    const Rect bounds{available.x(), available.y(), available.width(), available.height()};
    const auto placement = placePopup(caret, popup, bounds);

    winId();
    if (QGuiApplication::platformName().startsWith(QStringLiteral("wayland"))) {
        if (auto* layer = LayerShellQt::Window::get(windowHandle())) {
            layer->setLayer(LayerShellQt::Window::LayerTop);
            layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
            layer->setActivateOnShow(false);
            layer->setExclusiveZone(0);
            layer->setAnchors(hasCaret
                                  ? LayerShellQt::Window::Anchors{LayerShellQt::Window::AnchorTop} |
                                        LayerShellQt::Window::AnchorLeft
                                  : LayerShellQt::Window::Anchors{LayerShellQt::Window::AnchorTop});
            if (hasCaret) {
                layer->setScreen(screen);
            } else {
                layer->setWantsToBeOnActiveScreen(true);
            }
            layer->setDesiredSize(size());
            layer->setMargins(hasCaret
                                  ? QMargins{placement.position.x - screen->geometry().x(),
                                             placement.position.y - screen->geometry().y(), 0, 0}
                                  : QMargins{0, 24, 0, 0});
            layer->setScope(QStringLiteral("fcitx5-emoji-palette"));
        }
    } else {
        move(placement.position.x, placement.position.y);
    }
}

void PaletteWindow::applyCellSize(int size) {
    cellSize_ = std::clamp(size, 40, 72);
    settings_->setValue(QStringLiteral("Ui/CellSize"), cellSize_);
    settings_->sync();
    if (settings_->status() != QSettings::NoError) {
        qCWarning(logState) << "Could not save the UI settings";
    }
    static_cast<EmojiDelegate*>(grid_->itemDelegate())->setCellSize(cellSize_);
    resize(columns_ * (cellSize_ + 2) + 38, rows_ * (cellSize_ + 2) + 154);
    grid_->doItemsLayout();
    refreshGrid(false);
    if (isVisible()) {
        positionFor(request_);
    }
}

void PaletteWindow::saveState() {
    if (!stateStore_.save(state_)) {
        qCWarning(logState) << "Could not save favorites and recent emoji";
    }
}

QString PaletteWindow::categoryLabel(Category category) const {
    switch (category) {
    case Category::RecentlyUsed:
        return tr("Recently Used");
    case Category::Favorites:
        return tr("Favorites");
    case Category::SmileysEmotion:
        return tr("Smileys & Emotion");
    case Category::PeopleBody:
        return tr("People & Body");
    case Category::AnimalsNature:
        return tr("Animals & Nature");
    case Category::FoodDrink:
        return tr("Food & Drink");
    case Category::TravelPlaces:
        return tr("Travel & Places");
    case Category::Activities:
        return tr("Activities");
    case Category::Objects:
        return tr("Objects");
    case Category::Symbols:
        return tr("Symbols");
    case Category::Flags:
        return tr("Flags");
    case Category::Kaomoji:
        return tr("Kaomoji");
    }
    return {};
}

}
