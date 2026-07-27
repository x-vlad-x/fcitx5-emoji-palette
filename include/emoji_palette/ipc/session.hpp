#pragma once

#include "emoji_palette/ipc/protocol.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace emoji_palette::ipc {

enum class SessionState {
    Disconnected,
    Negotiating,
    Ready,
    Incompatible,
};

class PeerSession {
  public:
    Envelope begin(std::string owner, std::uint64_t nonce, std::uint32_t capabilities);
    bool acceptWelcome(std::string_view sender, const Welcome& welcome);
    bool accepts(std::string_view sender) const;
    void ownerLost(std::string_view owner);
    void negotiationExpired();

    SessionState state() const;
    std::uint64_t generation() const;
    std::optional<std::uint16_t> negotiatedVersion() const;
    const std::string& owner() const;

  private:
    SessionState state_ = SessionState::Disconnected;
    std::string owner_;
    std::uint64_t nonce_ = 0;
    std::uint64_t generation_ = 0;
    std::optional<std::uint16_t> version_;
};

class ReconnectBackoff {
  public:
    std::uint32_t nextDelayMilliseconds();
    void reset();
    std::uint32_t attempt() const;

  private:
    std::uint32_t attempt_ = 0;
};

class SequenceValidator {
  public:
    void begin(TransactionId transaction);
    bool accept(const TransactionId& transaction, std::uint32_t sequence);
    void reset();

  private:
    std::optional<TransactionId> transaction_;
    std::uint32_t lastSequence_ = 0;
};

} // namespace emoji_palette::ipc
