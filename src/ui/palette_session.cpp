#include "palette_session.hpp"

#include <QByteArray>

#include <span>
#include <string>
#include <utility>
#include <vector>

namespace emoji_palette::ui {

namespace {

std::span<const std::uint8_t> bytes(const QByteArray& value) {
    return {reinterpret_cast<const std::uint8_t*>(value.constData()),
            static_cast<std::size_t>(value.size())};
}

QByteArray byteArray(const std::vector<std::uint8_t>& value) {
    return {reinterpret_cast<const char*>(value.data()), static_cast<qsizetype>(value.size())};
}

}

PaletteSession::PaletteSession(PaletteWindow& window, QObject* parent)
    : QObject(parent), window_(window) {
    connect(&window_, &PaletteWindow::selectionRequested, this, [this](const QString& sequence) {
        if (!transaction_) {
            return;
        }
        const auto transaction = *transaction_;
        transaction_.reset();
        sequenceValidator_.reset();
        const auto utf8 = sequence.toUtf8();
        const auto envelope = ipc::serialize(
            {ipc::protocolVersion,
             ipc::Selected{transaction,
                           std::string(utf8.constData(), static_cast<std::size_t>(utf8.size()))}});
        if (envelope) {
            emit outgoingFrame(byteArray(*envelope));
        }
    });
    connect(&window_, &PaletteWindow::cancellationRequested, this,
            [this](ipc::CancelReason reason) { reset(reason, true); });
}

QByteArray PaletteSession::exchange(const QByteArray& frame, const QString& sender) {
    if (sender.isEmpty()) {
        return {};
    }
    const auto parsed = ipc::parse(bytes(frame));
    if (!parsed.envelope) {
        return {};
    }
    if (const auto* hello = std::get_if<ipc::Hello>(&parsed.envelope->payload)) {
        const auto welcome = ipc::negotiate(
            *hello, static_cast<std::uint32_t>(ipc::Capability::PointerSelection) |
                        static_cast<std::uint32_t>(ipc::Capability::LayerShell) |
                        static_cast<std::uint32_t>(ipc::Capability::LocalizedSearch));
        if (!welcome) {
            return {};
        }
        if (peerOwner_ != sender) {
            reset(ipc::CancelReason::HelperDisconnected, false);
            peerOwner_ = sender;
            emit peerChanged(peerOwner_);
        }
        negotiated_ = true;
        return response(*welcome);
    }
    if (!negotiated_ || sender != peerOwner_) {
        return {};
    }
    if (const auto* show = std::get_if<ipc::Show>(&parsed.envelope->payload)) {
        transaction_ = show->transaction;
        sequenceValidator_.begin(show->transaction);
        window_.showPalette(*show);
        return response(ipc::Pong{0});
    }
    if (const auto* command = std::get_if<ipc::Command>(&parsed.envelope->payload)) {
        if (!transaction_ || command->transaction != *transaction_ ||
            !sequenceValidator_.accept(command->transaction, command->sequence)) {
            return {};
        }
        window_.handleCommand(*command);
        return response(ipc::Pong{command->sequence});
    }
    if (const auto* hide = std::get_if<ipc::Hide>(&parsed.envelope->payload)) {
        if (!transaction_ || hide->transaction != *transaction_) {
            return {};
        }
        reset(hide->reason, false);
        return response(ipc::Pong{0});
    }
    if (const auto* ping = std::get_if<ipc::Ping>(&parsed.envelope->payload)) {
        return response(ipc::Pong{ping->nonce});
    }
    return {};
}

void PaletteSession::peerDisconnected(const QString& owner) {
    if (owner == peerOwner_) {
        reset(ipc::CancelReason::HelperDisconnected, false);
        peerOwner_.clear();
        negotiated_ = false;
        emit peerChanged({});
    }
}

const QString& PaletteSession::peerOwner() const { return peerOwner_; }

QByteArray PaletteSession::response(ipc::Payload payload) const {
    const auto envelope = ipc::serialize({ipc::protocolVersion, std::move(payload)});
    return envelope ? byteArray(*envelope) : QByteArray{};
}

void PaletteSession::reset(ipc::CancelReason reason, bool notify) {
    const auto transaction = transaction_;
    transaction_.reset();
    sequenceValidator_.reset();
    window_.hidePalette();
    if (notify && transaction) {
        const auto envelope =
            ipc::serialize({ipc::protocolVersion, ipc::Cancelled{*transaction, reason}});
        if (envelope) {
            emit outgoingFrame(byteArray(*envelope));
        }
    }
}

}
