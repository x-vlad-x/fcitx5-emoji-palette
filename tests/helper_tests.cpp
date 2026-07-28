#include "emoji_list_model.hpp"
#include "palette_session.hpp"
#include "palette_window.hpp"

#include "emoji_palette/catalog.hpp"
#include "emoji_palette/ipc/protocol.hpp"
#include "emoji_palette/state.hpp"

#include <QByteArray>
#include <QListView>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QToolButton>
#include <QtTest>

#include <cstdint>
#include <filesystem>
#include <string>

namespace {

QByteArray frame(emoji_palette::ipc::Payload payload) {
    const auto serialized =
        emoji_palette::ipc::serialize({emoji_palette::ipc::protocolVersion, std::move(payload)});
    if (!serialized) {
        return {};
    }
    return {reinterpret_cast<const char*>(serialized->data()),
            static_cast<qsizetype>(serialized->size())};
}

emoji_palette::ipc::ParseResult parse(const QByteArray& value) {
    return emoji_palette::ipc::parse({reinterpret_cast<const std::uint8_t*>(value.constData()),
                                      static_cast<std::size_t>(value.size())});
}

emoji_palette::ipc::TransactionId transaction(std::uint8_t marker) {
    emoji_palette::ipc::TransactionId value;
    value.bytes.front() = marker;
    return value;
}

QToolButton* settingsButton(emoji_palette::ui::PaletteWindow& window) {
    const auto buttons = window.findChildren<QToolButton*>();
    for (auto* button : buttons) {
        if (button->accessibleName() == QStringLiteral("Grid settings")) {
            return button;
        }
    }
    return nullptr;
}

}

class HelperTests final : public QObject {
    Q_OBJECT

  private slots:
    void localizedModelSearch();
    void configurationMigration();
    void settingsPanelStateTransitions();
    void settingsPanelPreservesSession();
    void sessionSelectsOnce();
    void sessionRejectsWrongOwnerAndReplay();
};

void HelperTests::localizedModelSearch() {
    emoji_palette::EmojiCatalog catalog;
    emoji_palette::PersistentState state;
    emoji_palette::ui::EmojiListModel model(catalog, state);
    model.setSearch(QStringLiteral("grinsendes gesicht"), emoji_palette::Locale::German);
    QVERIFY(model.rowCount() > 0);
    QCOMPARE(model.index(0, 0).data(Qt::DisplayRole).toString(), QStringLiteral("😀"));

    emoji_palette::toggleFavorite(state, "🐧");
    model.setSearch({}, emoji_palette::Locale::English);
    model.setCategory(emoji_palette::Category::Favorites);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.index(0, 0).data(Qt::DisplayRole).toString(), QStringLiteral("🐧"));
}

void HelperTests::configurationMigration() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto settingsPath = temporary.filePath(QStringLiteral("settings.ini"));
    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        settings.setValue(QStringLiteral("cellSize"), 64);
        settings.sync();
    }
    emoji_palette::EmojiCatalog catalog;
    emoji_palette::ui::PaletteWindow window(
        catalog, std::filesystem::path(temporary.path().toStdString()) / "state", settingsPath);
    QSettings migrated(settingsPath, QSettings::IniFormat);
    QCOMPARE(migrated.value(QStringLiteral("SchemaVersion")).toInt(), 1);
    QCOMPARE(migrated.value(QStringLiteral("Ui/CellSize")).toInt(), 64);
    QVERIFY(!migrated.contains(QStringLiteral("cellSize")));
}

void HelperTests::settingsPanelStateTransitions() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto settingsPath = temporary.filePath(QStringLiteral("settings.ini"));
    emoji_palette::EmojiCatalog catalog;
    emoji_palette::ui::PaletteWindow window(
        catalog, std::filesystem::path(temporary.path().toStdString()) / "state", settingsPath);
    QSignalSpy selected(&window, &emoji_palette::ui::PaletteWindow::selectionRequested);
    QSignalSpy cancelled(&window, &emoji_palette::ui::PaletteWindow::cancellationRequested);
    window.showPalette({transaction(3),
                        {100, 100, 2, 20},
                        {0, 0, 1920, 1080},
                        emoji_palette::Locale::English,
                        100,
                        true});

    auto* button = settingsButton(window);
    QVERIFY(button != nullptr);
    QVERIFY2(button->menu() == nullptr,
             "the settings control must not create a top-level popup window");
    auto* panel = window.findChild<QWidget*>(QStringLiteral("settingsPanel"));
    QVERIFY(panel != nullptr);
    QVERIFY(!panel->isVisible());

    for (int cycle = 0; cycle < 20; ++cycle) {
        QTest::mouseClick(button, Qt::LeftButton);
        QVERIFY(panel->isVisible());
        QVERIFY(window.isVisible());
        window.handleCommand({transaction(3),
                              static_cast<std::uint32_t>(cycle + 1),
                              emoji_palette::ipc::CommandKind::Cancel,
                              {}});
        QVERIFY(!panel->isVisible());
        QVERIFY(window.isVisible());
        QCOMPARE(selected.count(), 0);
        QCOMPARE(cancelled.count(), 0);
    }

    QTest::mouseClick(button, Qt::LeftButton);
    auto* largeButton = window.findChild<QToolButton*>(QStringLiteral("settingsSizeLarge"));
    QVERIFY(largeButton != nullptr);
    QTest::mouseClick(largeButton, Qt::LeftButton);
    QVERIFY(panel->isVisible());
    const auto screenshotPath = qEnvironmentVariable("EMOJI_PALETTE_SETTINGS_TEST_SCREENSHOT");
    if (!screenshotPath.isEmpty()) {
        QVERIFY(window.grab().save(screenshotPath));
    }
    QCOMPARE(
        QSettings(settingsPath, QSettings::IniFormat).value(QStringLiteral("Ui/CellSize")).toInt(),
        64);
    QCOMPARE(selected.count(), 0);
    QCOMPARE(cancelled.count(), 0);

    auto* grid = window.findChild<QListView*>();
    QVERIFY(grid != nullptr);
    QVERIFY(grid->currentIndex().isValid());
    QTest::mouseClick(grid->viewport(), Qt::LeftButton, Qt::NoModifier,
                      grid->visualRect(grid->currentIndex()).center());
    QVERIFY(!panel->isVisible());
    QVERIFY(window.isVisible());
    QCOMPARE(selected.count(), 0);
    QCOMPARE(cancelled.count(), 0);

    window.handleCommand({transaction(3), 22, emoji_palette::ipc::CommandKind::Cancel, {}});
    QCOMPARE(cancelled.count(), 1);
}

void HelperTests::settingsPanelPreservesSession() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    emoji_palette::EmojiCatalog catalog;
    emoji_palette::ui::PaletteWindow window(
        catalog, std::filesystem::path(temporary.path().toStdString()) / "state",
        temporary.filePath(QStringLiteral("settings.ini")));
    emoji_palette::ui::PaletteSession session(window);
    QSignalSpy outgoing(&session, &emoji_palette::ui::PaletteSession::outgoingFrame);
    const QString sender = QStringLiteral(":1.41");
    session.exchange(frame(emoji_palette::ipc::Hello{1, 1, 13, 0}), sender);

    const auto id = transaction(4);
    QVERIFY(!session
                 .exchange(frame(emoji_palette::ipc::Show{id,
                                                          {100, 100, 2, 20},
                                                          {0, 0, 1920, 1080},
                                                          emoji_palette::Locale::English,
                                                          100,
                                                          true}),
                           sender)
                 .isEmpty());
    auto* button = settingsButton(window);
    QVERIFY(button != nullptr);
    QVERIFY2(button->menu() == nullptr,
             "the settings control must not create a top-level popup window");
    QTest::mouseClick(button, Qt::LeftButton);

    QVERIFY(!session
                 .exchange(frame(emoji_palette::ipc::Command{
                               id, 1, emoji_palette::ipc::CommandKind::Cancel, {}}),
                           sender)
                 .isEmpty());
    QCOMPARE(outgoing.count(), 0);
    QVERIFY(window.isVisible());
    QVERIFY(!session
                 .exchange(frame(emoji_palette::ipc::Command{
                               id, 2, emoji_palette::ipc::CommandKind::Right, {}}),
                           sender)
                 .isEmpty());
    QCOMPARE(outgoing.count(), 0);

    QVERIFY(!session
                 .exchange(frame(emoji_palette::ipc::Command{
                               id, 3, emoji_palette::ipc::CommandKind::Cancel, {}}),
                           sender)
                 .isEmpty());
    QCOMPARE(outgoing.count(), 1);
    QVERIFY(!window.isVisible());
}

void HelperTests::sessionSelectsOnce() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    emoji_palette::EmojiCatalog catalog;
    emoji_palette::ui::PaletteWindow window(
        catalog, std::filesystem::path(temporary.path().toStdString()) / "state",
        temporary.filePath(QStringLiteral("settings.ini")));
    emoji_palette::ui::PaletteSession session(window);
    QSignalSpy outgoing(&session, &emoji_palette::ui::PaletteSession::outgoingFrame);
    const QString sender = QStringLiteral(":1.42");

    const auto welcome = session.exchange(frame(emoji_palette::ipc::Hello{1, 1, 17, 0}), sender);
    const auto parsedWelcome = parse(welcome);
    const auto* welcomeEnvelope = parsedWelcome.envelope ? &*parsedWelcome.envelope : nullptr;
    QVERIFY(welcomeEnvelope != nullptr);
    if (welcomeEnvelope == nullptr) {
        return;
    }
    QVERIFY(std::holds_alternative<emoji_palette::ipc::Welcome>(welcomeEnvelope->payload));

    const auto id = transaction(7);
    const auto shown = session.exchange(
        frame(emoji_palette::ipc::Show{
            id, {100, 100, 2, 20}, {0, 0, 1920, 1080}, emoji_palette::Locale::English, 100, true}),
        sender);
    QVERIFY(parse(shown).envelope.has_value());
    const auto screenshotPath = qEnvironmentVariable("EMOJI_PALETTE_TEST_SCREENSHOT");
    if (!screenshotPath.isEmpty()) {
        QVERIFY(window.grab().save(screenshotPath));
    }

    session.exchange(
        frame(emoji_palette::ipc::Command{id, 1, emoji_palette::ipc::CommandKind::Select, {}}),
        sender);
    QCOMPARE(outgoing.count(), 1);
    const auto selectedFrame = outgoing.takeFirst().at(0).toByteArray();
    const auto parsedSelected = parse(selectedFrame);
    const auto* selectedEnvelope = parsedSelected.envelope ? &*parsedSelected.envelope : nullptr;
    QVERIFY(selectedEnvelope != nullptr);
    if (selectedEnvelope == nullptr) {
        return;
    }
    const auto* selected = std::get_if<emoji_palette::ipc::Selected>(&selectedEnvelope->payload);
    QVERIFY(selected != nullptr);
    QCOMPARE(selected->transaction, id);
    QVERIFY(catalog.contains(selected->sequence));

    session.exchange(
        frame(emoji_palette::ipc::Command{id, 2, emoji_palette::ipc::CommandKind::Select, {}}),
        sender);
    QCOMPARE(outgoing.count(), 0);
}

void HelperTests::sessionRejectsWrongOwnerAndReplay() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    emoji_palette::EmojiCatalog catalog;
    emoji_palette::ui::PaletteWindow window(
        catalog, std::filesystem::path(temporary.path().toStdString()) / "state",
        temporary.filePath(QStringLiteral("settings.ini")));
    emoji_palette::ui::PaletteSession session(window);
    const QString sender = QStringLiteral(":1.50");
    session.exchange(frame(emoji_palette::ipc::Hello{1, 1, 31, 0}), sender);

    const auto id = transaction(9);
    const auto show = frame(emoji_palette::ipc::Show{
        id, {10, 10, 2, 10}, {0, 0, 1280, 720}, emoji_palette::Locale::Russian, 125, false});
    QVERIFY(session.exchange(show, QStringLiteral(":1.51")).isEmpty());
    QVERIFY(!session.exchange(show, sender).isEmpty());

    const auto command =
        frame(emoji_palette::ipc::Command{id, 1, emoji_palette::ipc::CommandKind::Right, {}});
    QVERIFY(!session.exchange(command, sender).isEmpty());
    QVERIFY(session.exchange(command, sender).isEmpty());
    session.peerDisconnected(sender);
    QVERIFY(session.peerOwner().isEmpty());
}

QTEST_MAIN(HelperTests)

#include "helper_tests.moc"
