#include "palette_window.hpp"

#include <LayerShellQt/Window>

#include <QAbstractItemView>
#include <QApplication>
#include <QButtonGroup>
#include <QCursor>
#include <QDateTime>
#include <QFontMetricsF>
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
#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace emoji_palette::ui {

namespace {

Q_LOGGING_CATEGORY(logState, "org.fcitx.EmojiPalette.state")

// Geometry only. Search text and selected sequences are never logged.
Q_LOGGING_CATEGORY(logPlacement, "org.fcitx.EmojiPalette.placement")

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
        const QColor foreground = option.palette.color(option.state.testFlag(QStyle::State_Selected)
                                                           ? QPalette::HighlightedText
                                                           : QPalette::Text);
        const auto renderable = index.data(EmojiListModel::RenderableRole);
        if (renderable.isValid() && !renderable.toBool()) {
            paintMissingGlyph(painter, option, foreground,
                              index.data(EmojiListModel::MissingGlyphLabelRole).toString());
        } else {
            QFont font = option.font;
            font.setPointSizeF(std::max(14.0, static_cast<double>(cellSize_) * 0.43));
            painter->setFont(font);
            painter->setPen(foreground);
            painter->drawText(option.rect, Qt::AlignCenter, index.data(Qt::DisplayRole).toString());
        }
        if (index.data(EmojiListModel::FavoriteRole).toBool()) {
            QFont markerFont = option.font;
            markerFont.setPointSizeF(std::max(7.0, static_cast<double>(cellSize_) * 0.17));
            painter->setFont(markerFont);
            painter->setPen(foreground);
            painter->drawText(option.rect.adjusted(3, 2, -3, -2), Qt::AlignTop | Qt::AlignRight,
                              QStringLiteral("★"));
        }
        painter->restore();
    }

  private:
    // Documented fallback for a code point that no installed font can draw. The
    // tile identifies the entry instead of leaving a missing-glyph box, and never
    // participates in what the picker commits.
    void paintMissingGlyph(QPainter* painter, const QStyleOptionViewItem& option,
                           const QColor& foreground, const QString& label) const {
        const auto inset = std::max(3.0, static_cast<double>(cellSize_) * 0.09);
        const QRectF box = QRectF(option.rect).adjusted(inset, inset, -inset, -inset);
        QPen border(foreground);
        border.setStyle(Qt::DashLine);
        border.setWidthF(1.0);
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(border);
        painter->drawRoundedRect(box, 3.0, 3.0);
        QFont labelFont = option.font;
        labelFont.setPointSizeF(std::max(6.0, static_cast<double>(cellSize_) * 0.185));
        painter->setFont(labelFont);
        painter->setPen(foreground);
        const QFontMetricsF metrics(labelFont);
        painter->drawText(box, Qt::AlignCenter,
                          metrics.elidedText(label, Qt::ElideRight, box.width() - 2.0));
    }

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
    rememberOutputScale();
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

void PaletteWindow::setGlyphProbe(GlyphProbe probe) {
    model_.setGlyphProbe(std::move(probe));
    if (grid_ != nullptr) {
        grid_->viewport()->update();
        updateSelection();
    }
}

void PaletteWindow::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::DevicePixelRatioChange) {
        rememberOutputScale();
        return;
    }
    if (event->type() != QEvent::FontChange) {
        return;
    }
    model_.refreshSystemGlyphProbe();
    if (grid_ != nullptr) {
        grid_->viewport()->update();
    }
}

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
        const auto coverage = model_.coverageFor(variant->sequence);
        const auto name =
            fromUtf8(variant->annotations[static_cast<std::size_t>(request_.locale)].name);
        button->setText(coverage.renderable ? fromUtf8(variant->sequence)
                                            : model_.missingGlyphText(coverage));
        button->setToolTip(model_.annotatedName(name, coverage));
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

namespace {

std::uint16_t scalePercentOf(double ratio) {
    const auto percent = static_cast<int>(std::lround(ratio * 100.0));
    return static_cast<std::uint16_t>(std::clamp(percent, static_cast<int>(minimumScalePercent),
                                                 static_cast<int>(maximumScalePercent)));
}

}

void PaletteWindow::rememberOutputScale() {
    // Only a mapped surface carries the real fractional scale; before that the
    // window still reports the output's integer buffer scale.
    const auto* handle = windowHandle();
    if (handle == nullptr || !handle->isExposed() || handle->screen() == nullptr) {
        return;
    }
    outputScales_.insert(handle->screen()->name(), scalePercentOf(handle->devicePixelRatio()));
}

std::uint16_t PaletteWindow::knownOutputScale(const QScreen& screen) const {
    // Qt reports the integer buffer scale of a Wayland output; its fractional
    // scale only arrives once a surface of ours has been mapped on it. A
    // reported ratio of exactly one needs no surface, because no larger
    // fractional scale rounds down to it.
    if (scalePercentOf(screen.devicePixelRatio()) == 100) {
        return 100;
    }
    return outputScales_.value(screen.name(), 0);
}

void PaletteWindow::positionFor(const ipc::Show& request) {
    rememberOutputScale();
    const auto screens = QGuiApplication::screens();
    if (screens.isEmpty()) {
        return;
    }

    // Fcitx5 reports an absolute caret in device pixels, so the output is
    // resolved in that same space before anything is converted.
    std::vector<Rect> nativeOutputs;
    nativeOutputs.reserve(static_cast<std::size_t>(screens.size()));
    for (const auto* candidate : screens) {
        const QRect logical = candidate->geometry();
        const auto known = knownOutputScale(*candidate);
        nativeOutputs.push_back(
            nativeOutputBounds({logical.x(), logical.y(), logical.width(), logical.height()},
                               known != 0 ? known : scalePercentOf(candidate->devicePixelRatio())));
    }

    const bool hasCaret = request.caret != absentCaret && isTransportableRect(request.caret);
    QScreen* screen = nullptr;
    if (hasCaret) {
        if (const auto index = outputForCaret(request.caret, nativeOutputs)) {
            screen = screens.at(static_cast<qsizetype>(*index));
        }
    }
    if (screen == nullptr) {
        // No caret rectangle, or no usable output near it. On Wayland the
        // compositor picks the active output below; the pointer position is
        // only meaningful on X11.
        screen = QGuiApplication::screenAt(QCursor::pos());
    }
    if (screen == nullptr) {
        screen = QGuiApplication::primaryScreen();
    }
    if (screen == nullptr) {
        return;
    }

    const QRect geometry = screen->geometry();
    const QRect available = screen->availableGeometry();
    const Size popup{std::min(width(), available.width()), std::min(height(), available.height())};
    resize(popup.width, popup.height);
    const Rect bounds{available.x(), available.y(), available.width(), available.height()};
    // Converting with a scale that is only an upper bound would put the picker
    // somewhere it does not belong, so the documented fallback is used until
    // this output's real scale is known.
    const auto outputScale = knownOutputScale(*screen);
    std::optional<Rect> caret;
    if (hasCaret && outputScale != 0) {
        caret = logicalFromNative(request.caret,
                                  {geometry.x(), geometry.y(), geometry.width(), geometry.height()},
                                  outputScale);
    }
    const Point position =
        caret ? placePopup(*caret, popup, bounds).position : centeredPopup(popup, bounds);

    qCDebug(logPlacement).nospace()
        << "platform=" << QGuiApplication::platformName() << " rawCaret=" << request.caret.x << ","
        << request.caret.y << "," << request.caret.width << "," << request.caret.height
        << " clientScalePercent=" << request.scalePercent << " output=" << screen->name()
        << " outputScalePercent=" << outputScale << " geometry=" << geometry
        << " available=" << available << " logicalCaret="
        << (caret ? QStringLiteral("%1,%2,%3,%4")
                        .arg(caret->x)
                        .arg(caret->y)
                        .arg(caret->width)
                        .arg(caret->height)
                  : QStringLiteral("absent"))
        << " popup=" << popup.width << "x" << popup.height << " position=" << position.x << ","
        << position.y;

    winId();
    if (QGuiApplication::platformName().startsWith(QStringLiteral("wayland"))) {
        if (auto* layer = LayerShellQt::Window::get(windowHandle())) {
            layer->setLayer(LayerShellQt::Window::LayerTop);
            layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
            layer->setActivateOnShow(false);
            layer->setExclusiveZone(0);
            layer->setScope(QStringLiteral("fcitx5-emoji-palette"));
            layer->setDesiredSize(size());
            if (caret) {
                layer->setAnchors(LayerShellQt::Window::Anchors{LayerShellQt::Window::AnchorTop} |
                                  LayerShellQt::Window::AnchorLeft);
                layer->setScreen(screen);
                // Layer-shell margins are measured from the anchored edges of
                // the output itself, not from the panel-adjusted area.
                layer->setMargins({position.x - geometry.x(), position.y - geometry.y(), 0, 0});
            } else {
                // An unanchored layer surface is centered by the compositor on
                // the output it is assigned to.
                layer->setAnchors(LayerShellQt::Window::Anchors{});
                layer->setMargins({});
                layer->setWantsToBeOnActiveScreen(true);
            }
        }
    } else {
        move(position.x, position.y);
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
