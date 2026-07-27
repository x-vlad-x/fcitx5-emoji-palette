#pragma once

#include "palette_window.hpp"

#include "emoji_palette/ipc/protocol.hpp"
#include "emoji_palette/ipc/session.hpp"

#include <QByteArray>
#include <QObject>
#include <QString>

#include <optional>

namespace emoji_palette::ui {

class PaletteSession final : public QObject {
    Q_OBJECT

  public:
    explicit PaletteSession(PaletteWindow& window, QObject* parent = nullptr);

    QByteArray exchange(const QByteArray& frame, const QString& sender);
    void peerDisconnected(const QString& owner);
    const QString& peerOwner() const;

  signals:
    void outgoingFrame(QByteArray frame);
    void peerChanged(QString owner);

  private:
    QByteArray response(ipc::Payload payload) const;
    void reset(ipc::CancelReason reason, bool notify);

    PaletteWindow& window_;
    QString peerOwner_;
    bool negotiated_ = false;
    std::optional<ipc::TransactionId> transaction_;
    ipc::SequenceValidator sequenceValidator_;
};

}
