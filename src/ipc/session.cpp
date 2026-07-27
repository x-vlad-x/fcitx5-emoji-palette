#include "emoji_palette/ipc/session.hpp"

#include <algorithm>
#include <utility>

namespace emoji_palette::ipc {

Envelope PeerSession::begin(std::string owner, std::uint64_t nonce, std::uint32_t capabilities) {
    owner_ = std::move(owner);
    nonce_ = nonce;
    version_.reset();
    state_ = SessionState::Negotiating;
    ++generation_;
    return {
        protocolVersion,
        Hello{protocolVersion, protocolVersion, nonce_, capabilities},
    };
}

bool PeerSession::acceptWelcome(std::string_view sender, const Welcome& welcome) {
    if (state_ != SessionState::Negotiating || sender != owner_ || welcome.nonce != nonce_) {
        return false;
    }
    if (welcome.selectedVersion != protocolVersion) {
        state_ = SessionState::Incompatible;
        version_.reset();
        return false;
    }
    state_ = SessionState::Ready;
    version_ = welcome.selectedVersion;
    return true;
}

bool PeerSession::accepts(std::string_view sender) const {
    return state_ == SessionState::Ready && sender == owner_;
}

void PeerSession::ownerLost(std::string_view owner) {
    if (owner != owner_) {
        return;
    }
    state_ = SessionState::Disconnected;
    owner_.clear();
    nonce_ = 0;
    version_.reset();
    ++generation_;
}

void PeerSession::negotiationExpired() {
    if (state_ != SessionState::Negotiating) {
        return;
    }
    state_ = SessionState::Disconnected;
    owner_.clear();
    nonce_ = 0;
    version_.reset();
    ++generation_;
}

SessionState PeerSession::state() const { return state_; }

std::uint64_t PeerSession::generation() const { return generation_; }

std::optional<std::uint16_t> PeerSession::negotiatedVersion() const { return version_; }

const std::string& PeerSession::owner() const { return owner_; }

std::uint32_t ReconnectBackoff::nextDelayMilliseconds() {
    constexpr std::uint32_t minimumDelay = 100;
    constexpr std::uint32_t maximumDelay = 5000;
    constexpr std::uint32_t maximumShift = 6;
    const auto shift = std::min(attempt_, maximumShift);
    ++attempt_;
    return std::min(minimumDelay << shift, maximumDelay);
}

void ReconnectBackoff::reset() { attempt_ = 0; }

std::uint32_t ReconnectBackoff::attempt() const { return attempt_; }

void SequenceValidator::begin(TransactionId transaction) {
    transaction_ = transaction;
    lastSequence_ = 0;
}

bool SequenceValidator::accept(const TransactionId& transaction, std::uint32_t sequence) {
    if (!transaction_ || *transaction_ != transaction || sequence == 0 ||
        sequence <= lastSequence_) {
        return false;
    }
    lastSequence_ = sequence;
    return true;
}

void SequenceValidator::reset() {
    transaction_.reset();
    lastSequence_ = 0;
}

}
