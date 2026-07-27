#include "emoji_palette/ipc/protocol.hpp"

#include "emoji_palette/utf8.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <type_traits>
#include <utility>

namespace emoji_palette::ipc {
namespace {

constexpr std::array<std::uint8_t, 4> magic = {'E', 'P', 'A', 'L'};
constexpr std::size_t headerSize = 12;

enum class MessageType : std::uint8_t {
    Hello = 1,
    Welcome = 2,
    Show = 3,
    Command = 4,
    Hide = 5,
    Selected = 6,
    Cancelled = 7,
    Ping = 8,
    Pong = 9,
};

class Writer {
  public:
    template <typename Value> void integer(Value value) {
        using Unsigned = std::make_unsigned_t<Value>;
        const auto unsignedValue = static_cast<Unsigned>(value);
        for (std::size_t offset = sizeof(Value); offset > 0; --offset) {
            bytes_.push_back(static_cast<std::uint8_t>(unsignedValue >> ((offset - 1) * 8U)));
        }
    }

    void transaction(const TransactionId& value) {
        bytes_.insert(bytes_.end(), value.bytes.begin(), value.bytes.end());
    }

    bool text(std::string_view value, std::size_t maximum) {
        if (value.size() > maximum || value.size() > std::numeric_limits<std::uint16_t>::max() ||
            !isValidUtf8(value)) {
            return false;
        }
        integer(static_cast<std::uint16_t>(value.size()));
        bytes_.insert(bytes_.end(), value.begin(), value.end());
        return true;
    }

    void rectangle(const Rect& value) {
        integer(static_cast<std::int32_t>(value.x));
        integer(static_cast<std::int32_t>(value.y));
        integer(static_cast<std::int32_t>(value.width));
        integer(static_cast<std::int32_t>(value.height));
    }

    std::vector<std::uint8_t>& bytes() { return bytes_; }

  private:
    std::vector<std::uint8_t> bytes_;
};

class Reader {
  public:
    explicit Reader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

    template <typename Value> std::optional<Value> integer() {
        if (remaining() < sizeof(Value)) {
            return std::nullopt;
        }
        using Unsigned = std::make_unsigned_t<Value>;
        Unsigned result = 0;
        for (std::size_t offset = 0; offset < sizeof(Value); ++offset) {
            result = static_cast<Unsigned>((result << 8U) | bytes_[position_ + offset]);
        }
        position_ += sizeof(Value);
        return static_cast<Value>(result);
    }

    std::optional<TransactionId> transaction() {
        if (remaining() < TransactionId{}.bytes.size()) {
            return std::nullopt;
        }
        TransactionId result;
        std::copy_n(bytes_.begin() + static_cast<std::ptrdiff_t>(position_), result.bytes.size(),
                    result.bytes.begin());
        position_ += result.bytes.size();
        return result;
    }

    std::optional<std::string> text(std::size_t maximum, ProtocolError& error) {
        const auto size = integer<std::uint16_t>();
        if (!size || *size > maximum || remaining() < *size) {
            error = ProtocolError::ValueOutOfRange;
            return std::nullopt;
        }
        const auto begin = bytes_.begin() + static_cast<std::ptrdiff_t>(position_);
        std::string result(begin, begin + *size);
        position_ += *size;
        if (!isValidUtf8(result)) {
            error = ProtocolError::InvalidUtf8;
            return std::nullopt;
        }
        return result;
    }

    std::optional<Rect> rectangle() {
        const auto x = integer<std::int32_t>();
        const auto y = integer<std::int32_t>();
        const auto width = integer<std::int32_t>();
        const auto height = integer<std::int32_t>();
        if (!x || !y || !width || !height) {
            return std::nullopt;
        }
        return Rect{*x, *y, *width, *height};
    }

    std::size_t remaining() const { return bytes_.size() - position_; }

  private:
    std::span<const std::uint8_t> bytes_;
    std::size_t position_ = 0;
};

bool validRectangle(const Rect& value) {
    constexpr int coordinateLimit = 1000000;
    constexpr int dimensionLimit = 32768;
    return value.x >= -coordinateLimit && value.x <= coordinateLimit &&
           value.y >= -coordinateLimit && value.y <= coordinateLimit && value.width >= 0 &&
           value.width <= dimensionLimit && value.height >= 0 && value.height <= dimensionLimit;
}

bool validTransaction(const TransactionId& value) {
    return std::any_of(value.bytes.begin(), value.bytes.end(),
                       [](std::uint8_t byte) { return byte != 0; });
}

bool validCommandKind(std::uint8_t value) {
    return value <= static_cast<std::uint8_t>(CommandKind::ShowVariants);
}

bool validCancelReason(std::uint8_t value) {
    return value <= static_cast<std::uint8_t>(CancelReason::ProtocolError);
}

bool validLocale(std::uint8_t value) { return value <= static_cast<std::uint8_t>(Locale::Russian); }

MessageType messageType(const Payload& payload) {
    return std::visit(
        [](const auto& value) {
            using Value = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Value, Hello>) {
                return MessageType::Hello;
            } else if constexpr (std::is_same_v<Value, Welcome>) {
                return MessageType::Welcome;
            } else if constexpr (std::is_same_v<Value, Show>) {
                return MessageType::Show;
            } else if constexpr (std::is_same_v<Value, Command>) {
                return MessageType::Command;
            } else if constexpr (std::is_same_v<Value, Hide>) {
                return MessageType::Hide;
            } else if constexpr (std::is_same_v<Value, Selected>) {
                return MessageType::Selected;
            } else if constexpr (std::is_same_v<Value, Cancelled>) {
                return MessageType::Cancelled;
            } else if constexpr (std::is_same_v<Value, Ping>) {
                return MessageType::Ping;
            } else {
                return MessageType::Pong;
            }
        },
        payload);
}

bool writePayload(Writer& writer, const Payload& payload) {
    return std::visit(
        [&](const auto& value) {
            using Value = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Value, Hello>) {
                writer.integer(value.minimumVersion);
                writer.integer(value.maximumVersion);
                writer.integer(value.nonce);
                writer.integer(value.capabilities);
            } else if constexpr (std::is_same_v<Value, Welcome>) {
                writer.integer(value.selectedVersion);
                writer.integer(value.nonce);
                writer.integer(value.capabilities);
            } else if constexpr (std::is_same_v<Value, Show>) {
                if (!validTransaction(value.transaction) || !validRectangle(value.caret) ||
                    !validRectangle(value.screen) || value.scalePercent < 50 ||
                    value.scalePercent > 400) {
                    return false;
                }
                writer.transaction(value.transaction);
                writer.rectangle(value.caret);
                writer.rectangle(value.screen);
                writer.integer(static_cast<std::uint8_t>(value.locale));
                writer.integer(value.scalePercent);
                writer.integer(static_cast<std::uint8_t>(value.closeAfterSelection));
            } else if constexpr (std::is_same_v<Value, Command>) {
                if (!validTransaction(value.transaction) ||
                    (value.kind != CommandKind::SearchText && !value.text.empty())) {
                    return false;
                }
                writer.transaction(value.transaction);
                writer.integer(value.sequence);
                writer.integer(static_cast<std::uint8_t>(value.kind));
                return writer.text(value.text, maximumSearchSize);
            } else if constexpr (std::is_same_v<Value, Selected>) {
                if (!validTransaction(value.transaction) || value.sequence.empty()) {
                    return false;
                }
                writer.transaction(value.transaction);
                return writer.text(value.sequence, maximumSelectionSize);
            } else if constexpr (std::is_same_v<Value, Hide> || std::is_same_v<Value, Cancelled>) {
                if (!validTransaction(value.transaction)) {
                    return false;
                }
                writer.transaction(value.transaction);
                writer.integer(static_cast<std::uint8_t>(value.reason));
            } else {
                writer.integer(value.nonce);
            }
            return true;
        },
        payload);
}

std::optional<Payload> readPayload(MessageType type, Reader& reader, ProtocolError& error) {
    switch (type) {
    case MessageType::Hello: {
        const auto minimum = reader.integer<std::uint16_t>();
        const auto maximum = reader.integer<std::uint16_t>();
        const auto nonce = reader.integer<std::uint64_t>();
        const auto capabilities = reader.integer<std::uint32_t>();
        if (!minimum || !maximum || !nonce || !capabilities || *minimum == 0 ||
            *minimum > *maximum) {
            return std::nullopt;
        }
        return Hello{*minimum, *maximum, *nonce, *capabilities};
    }
    case MessageType::Welcome: {
        const auto selected = reader.integer<std::uint16_t>();
        const auto nonce = reader.integer<std::uint64_t>();
        const auto capabilities = reader.integer<std::uint32_t>();
        if (!selected || !nonce || !capabilities || *selected == 0) {
            return std::nullopt;
        }
        return Welcome{*selected, *nonce, *capabilities};
    }
    case MessageType::Show: {
        const auto transaction = reader.transaction();
        const auto caret = reader.rectangle();
        const auto screen = reader.rectangle();
        const auto locale = reader.integer<std::uint8_t>();
        const auto scale = reader.integer<std::uint16_t>();
        const auto close = reader.integer<std::uint8_t>();
        if (!transaction || !validTransaction(*transaction) || !caret || !screen || !locale ||
            !scale || !close || !validRectangle(*caret) || !validRectangle(*screen) ||
            !validLocale(*locale) || *scale < 50 || *scale > 400 || *close > 1) {
            error = ProtocolError::ValueOutOfRange;
            return std::nullopt;
        }
        return Show{*transaction, *caret,     *screen, static_cast<Locale>(*locale),
                    *scale,       *close == 1};
    }
    case MessageType::Command: {
        const auto transaction = reader.transaction();
        const auto sequence = reader.integer<std::uint32_t>();
        const auto kind = reader.integer<std::uint8_t>();
        const auto text = reader.text(maximumSearchSize, error);
        if (!transaction || !validTransaction(*transaction) || !sequence || !kind || !text ||
            !validCommandKind(*kind) ||
            (static_cast<CommandKind>(*kind) != CommandKind::SearchText && !text->empty())) {
            return std::nullopt;
        }
        return Command{*transaction, *sequence, static_cast<CommandKind>(*kind), *text};
    }
    case MessageType::Hide:
    case MessageType::Cancelled: {
        const auto transaction = reader.transaction();
        const auto reason = reader.integer<std::uint8_t>();
        if (!transaction || !validTransaction(*transaction) || !reason ||
            !validCancelReason(*reason)) {
            return std::nullopt;
        }
        if (type == MessageType::Hide) {
            return Hide{*transaction, static_cast<CancelReason>(*reason)};
        }
        return Cancelled{*transaction, static_cast<CancelReason>(*reason)};
    }
    case MessageType::Selected: {
        const auto transaction = reader.transaction();
        const auto sequence = reader.text(maximumSelectionSize, error);
        if (!transaction || !validTransaction(*transaction) || !sequence || sequence->empty()) {
            return std::nullopt;
        }
        return Selected{*transaction, *sequence};
    }
    case MessageType::Ping:
    case MessageType::Pong: {
        const auto nonce = reader.integer<std::uint64_t>();
        if (!nonce) {
            return std::nullopt;
        }
        if (type == MessageType::Ping) {
            return Ping{*nonce};
        }
        return Pong{*nonce};
    }
    }
    return std::nullopt;
}

}

std::optional<TransactionId> transactionIdFromString(std::string_view value) {
    if (value.size() != 32) {
        return std::nullopt;
    }
    TransactionId result;
    for (std::size_t index = 0; index < result.bytes.size(); ++index) {
        const auto high = value[index * 2];
        const auto low = value[index * 2 + 1];
        const auto decode = [](char character) -> std::optional<std::uint8_t> {
            if (character >= '0' && character <= '9') {
                return static_cast<std::uint8_t>(character - '0');
            }
            if (character >= 'a' && character <= 'f') {
                return static_cast<std::uint8_t>(character - 'a' + 10);
            }
            return std::nullopt;
        };
        const auto highValue = decode(high);
        const auto lowValue = decode(low);
        if (!highValue || !lowValue) {
            return std::nullopt;
        }
        result.bytes[index] = static_cast<std::uint8_t>((*highValue << 4U) | *lowValue);
    }
    return result;
}

std::string transactionIdToString(const TransactionId& value) {
    static constexpr std::array<char, 16> digits = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f',
    };
    std::string result;
    result.reserve(32);
    for (const auto byte : value.bytes) {
        result.push_back(digits[byte >> 4U]);
        result.push_back(digits[byte & 0x0FU]);
    }
    return result;
}

std::optional<std::vector<std::uint8_t>> serialize(const Envelope& envelope) {
    if (envelope.version != protocolVersion) {
        return std::nullopt;
    }
    Writer payloadWriter;
    if (!writePayload(payloadWriter, envelope.payload) ||
        payloadWriter.bytes().size() > maximumFrameSize - headerSize) {
        return std::nullopt;
    }
    Writer frameWriter;
    frameWriter.bytes().insert(frameWriter.bytes().end(), magic.begin(), magic.end());
    frameWriter.integer(envelope.version);
    frameWriter.integer(static_cast<std::uint8_t>(messageType(envelope.payload)));
    frameWriter.integer(std::uint8_t{0});
    frameWriter.integer(static_cast<std::uint32_t>(payloadWriter.bytes().size()));
    frameWriter.bytes().insert(frameWriter.bytes().end(), payloadWriter.bytes().begin(),
                               payloadWriter.bytes().end());
    return std::move(frameWriter.bytes());
}

ParseResult parse(std::span<const std::uint8_t> frame) {
    if (frame.size() < headerSize) {
        return {std::nullopt, ProtocolError::FrameTooSmall};
    }
    if (frame.size() > maximumFrameSize) {
        return {std::nullopt, ProtocolError::FrameTooLarge};
    }
    if (!std::equal(magic.begin(), magic.end(), frame.begin())) {
        return {std::nullopt, ProtocolError::InvalidMagic};
    }
    Reader header(frame.subspan(magic.size()));
    const auto version = header.integer<std::uint16_t>();
    const auto rawType = header.integer<std::uint8_t>();
    const auto reserved = header.integer<std::uint8_t>();
    const auto payloadSize = header.integer<std::uint32_t>();
    if (!version || *version != protocolVersion) {
        return {std::nullopt, ProtocolError::UnsupportedVersion};
    }
    if (!rawType || *rawType < static_cast<std::uint8_t>(MessageType::Hello) ||
        *rawType > static_cast<std::uint8_t>(MessageType::Pong)) {
        return {std::nullopt, ProtocolError::UnknownMessageType};
    }
    if (!reserved || *reserved != 0) {
        return {std::nullopt, ProtocolError::InvalidReservedBits};
    }
    if (!payloadSize || *payloadSize != frame.size() - headerSize) {
        return {std::nullopt, ProtocolError::SizeMismatch};
    }
    Reader payload(frame.subspan(headerSize));
    ProtocolError error = ProtocolError::MalformedPayload;
    auto value = readPayload(static_cast<MessageType>(*rawType), payload, error);
    if (!value || payload.remaining() != 0) {
        return {std::nullopt, error};
    }
    return {Envelope{*version, std::move(*value)}, ProtocolError::None};
}

std::optional<Welcome> negotiate(const Hello& hello, std::uint32_t supportedCapabilities) {
    if (hello.minimumVersion > protocolVersion || hello.maximumVersion < protocolVersion) {
        return std::nullopt;
    }
    return Welcome{
        protocolVersion,
        hello.nonce,
        hello.capabilities & supportedCapabilities,
    };
}

}
