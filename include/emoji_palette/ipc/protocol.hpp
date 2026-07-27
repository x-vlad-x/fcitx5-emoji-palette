#pragma once

#include "emoji_palette/catalog.hpp"
#include "emoji_palette/geometry.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace emoji_palette::ipc {

constexpr std::uint16_t protocolVersion = 1;
constexpr std::size_t maximumFrameSize = 64 * 1024;
constexpr std::size_t maximumSearchSize = 256;
constexpr std::size_t maximumSelectionSize = 128;

struct TransactionId {
    std::array<std::uint8_t, 16> bytes{};

    bool operator==(const TransactionId&) const = default;
};

std::optional<TransactionId> transactionIdFromString(std::string_view value);
std::string transactionIdToString(const TransactionId& value);

enum class Capability : std::uint32_t {
    PointerSelection = 1U << 0U,
    LayerShell = 1U << 1U,
    LocalizedSearch = 1U << 2U,
};

enum class CommandKind : std::uint8_t {
    Left,
    Right,
    Up,
    Down,
    Home,
    End,
    PageUp,
    PageDown,
    PreviousCategory,
    NextCategory,
    Select,
    Cancel,
    SearchText,
    ToggleFavorite,
    ShowVariants,
};

enum class CancelReason : std::uint8_t {
    User,
    FocusLost,
    ContextReset,
    ContextDestroyed,
    InputMethodChanged,
    HelperDisconnected,
    Timeout,
    ProtocolError,
};

struct Hello {
    std::uint16_t minimumVersion;
    std::uint16_t maximumVersion;
    std::uint64_t nonce;
    std::uint32_t capabilities;

    bool operator==(const Hello&) const = default;
};

struct Welcome {
    std::uint16_t selectedVersion;
    std::uint64_t nonce;
    std::uint32_t capabilities;

    bool operator==(const Welcome&) const = default;
};

struct Show {
    TransactionId transaction;
    Rect caret;
    Rect screen;
    Locale locale;
    std::uint16_t scalePercent;
    bool closeAfterSelection;

    bool operator==(const Show&) const = default;
};

struct Command {
    TransactionId transaction;
    std::uint32_t sequence;
    CommandKind kind;
    std::string text;

    bool operator==(const Command&) const = default;
};

struct Hide {
    TransactionId transaction;
    CancelReason reason;

    bool operator==(const Hide&) const = default;
};

struct Selected {
    TransactionId transaction;
    std::string sequence;

    bool operator==(const Selected&) const = default;
};

struct Cancelled {
    TransactionId transaction;
    CancelReason reason;

    bool operator==(const Cancelled&) const = default;
};

struct Ping {
    std::uint64_t nonce;

    bool operator==(const Ping&) const = default;
};

struct Pong {
    std::uint64_t nonce;

    bool operator==(const Pong&) const = default;
};

using Payload = std::variant<Hello, Welcome, Show, Command, Hide, Selected, Cancelled, Ping, Pong>;

struct Envelope {
    std::uint16_t version;
    Payload payload;

    bool operator==(const Envelope&) const = default;
};

enum class ProtocolError {
    None,
    FrameTooSmall,
    FrameTooLarge,
    InvalidMagic,
    UnsupportedVersion,
    UnknownMessageType,
    InvalidReservedBits,
    SizeMismatch,
    MalformedPayload,
    InvalidUtf8,
    ValueOutOfRange,
};

struct ParseResult {
    std::optional<Envelope> envelope;
    ProtocolError error;
};

std::optional<std::vector<std::uint8_t>> serialize(const Envelope& envelope);
ParseResult parse(std::span<const std::uint8_t> frame);
std::optional<Welcome> negotiate(const Hello& hello, std::uint32_t supportedCapabilities);

} // namespace emoji_palette::ipc
