#include "palette_session.hpp"
#include "palette_window.hpp"

#include "emoji_palette/catalog.hpp"

#include <QApplication>
#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusError>
#include <QDBusMessage>
#include <QDBusServiceWatcher>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <QTranslator>

#include <filesystem>

namespace {

constexpr char serviceName[] = "org.fcitx.Fcitx5.EmojiPalette1";
constexpr char objectPath[] = "/org/fcitx/Fcitx5/EmojiPalette1";

Q_LOGGING_CATEGORY(logIpc, "org.fcitx.EmojiPalette.ipc")

class PaletteDbusObject final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.fcitx.Fcitx5.EmojiPalette1")

  public:
    explicit PaletteDbusObject(emoji_palette::ui::PaletteSession& session,
                               QObject* parent = nullptr)
        : QObject(parent), session_(session) {
        connect(&session_, &emoji_palette::ui::PaletteSession::outgoingFrame, this,
                &PaletteDbusObject::Frame);
    }

    Q_SLOT QByteArray Exchange(const QByteArray& frame) {
        return session_.exchange(frame, message().service());
    }

  signals:
    void Frame(QByteArray frame);

  private:
    emoji_palette::ui::PaletteSession& session_;
};

}

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Fcitx"));
    QCoreApplication::setApplicationName(QStringLiteral("EmojiPalette"));
    QCoreApplication::setApplicationVersion(QString::fromLatin1(EMOJI_PALETTE_VERSION));
    application.setQuitOnLastWindowClosed(false);

    QTranslator translator;
    if (translator.load(QLocale(), QStringLiteral("emoji-palette"), QStringLiteral("_"),
                        QStringLiteral(":/i18n"))) {
        application.installTranslator(&translator);
    }

    const auto stateRoot = QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation);
    const auto configRoot = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    emoji_palette::EmojiCatalog catalog;
    emoji_palette::ui::PaletteWindow window(
        catalog, std::filesystem::path(stateRoot.toStdString()) / "fcitx5-emoji-palette" / "state",
        configRoot + QStringLiteral("/fcitx5-emoji-palette/settings.ini"));
    emoji_palette::ui::PaletteSession session(window);
    PaletteDbusObject dbusObject(session);

    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qCCritical(logIpc) << "The session D-Bus is unavailable";
        return 1;
    }
    if (!bus.registerObject(QString::fromLatin1(objectPath), &dbusObject,
                            QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals)) {
        qCCritical(logIpc) << "Could not register the palette D-Bus object:"
                           << bus.lastError().message();
        return 1;
    }
    if (!bus.registerService(QString::fromLatin1(serviceName))) {
        qCCritical(logIpc) << "Could not own the palette D-Bus service:"
                           << bus.lastError().message();
        return 1;
    }

    QDBusServiceWatcher peerWatcher;
    peerWatcher.setConnection(bus);
    peerWatcher.setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
    QObject::connect(&session, &emoji_palette::ui::PaletteSession::peerChanged, &peerWatcher,
                     [&peerWatcher](const QString& owner) {
                         peerWatcher.setWatchedServices(owner.isEmpty() ? QStringList{}
                                                                        : QStringList{owner});
                     });
    QObject::connect(&peerWatcher, &QDBusServiceWatcher::serviceUnregistered, &session,
                     &emoji_palette::ui::PaletteSession::peerDisconnected);

    return application.exec();
}

#include "main.moc"
