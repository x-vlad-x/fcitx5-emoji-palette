#include "emoji_palette/ipc/protocol.hpp"
#include "emoji_palette/ipc/session.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using emoji_palette::ipc::Envelope;

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

emoji_palette::ipc::TransactionId transaction(std::uint8_t seed) {
    emoji_palette::ipc::TransactionId result;
    for (std::size_t index = 0; index < result.bytes.size(); ++index) {
        result.bytes[index] = static_cast<std::uint8_t>(seed + index);
    }
    return result;
}

void roundTrip(const Envelope& envelope) {
    const auto frame = emoji_palette::ipc::serialize(envelope);
    if (!frame) {
        throw std::runtime_error("valid envelope was not serialized");
    }
    require(frame.value().size() <= emoji_palette::ipc::maximumFrameSize, "frame exceeded limit");
    const auto parsed = emoji_palette::ipc::parse(frame.value());
    require(parsed.error == emoji_palette::ipc::ProtocolError::None && parsed.envelope == envelope,
            "frame did not round trip");
}

void testTransactionId() {
    const auto value = transaction(1);
    const auto text = emoji_palette::ipc::transactionIdToString(value);
    require(text.size() == 32, "transaction text length failed");
    require(emoji_palette::ipc::transactionIdFromString(text) == value,
            "transaction text round trip failed");
    require(!emoji_palette::ipc::transactionIdFromString("ABCDEF0123456789ABCDEF0123456789"),
            "uppercase transaction accepted");
    require(!emoji_palette::ipc::transactionIdFromString("abc"), "short transaction accepted");
}

void testRoundTrips() {
    using namespace emoji_palette::ipc;
    const auto id = transaction(5);
    roundTrip({protocolVersion, Hello{1, 1, 42, 7}});
    roundTrip({protocolVersion, Welcome{1, 42, 3}});
    roundTrip({
        protocolVersion,
        Show{
            id,
            {-1900, 900, 2, 24},
            {-1920, 0, 1920, 1080},
            emoji_palette::Locale::Russian,
            125,
            true,
        },
    });
    roundTrip({protocolVersion, Command{id, 1, CommandKind::SearchText, "пингвин"}});
    roundTrip({protocolVersion, Command{id, 2, CommandKind::Right, ""}});
    roundTrip({protocolVersion, Command{id, 3, CommandKind::SearchText, ""}});
    roundTrip({protocolVersion, Hide{id, CancelReason::FocusLost}});
    roundTrip({protocolVersion, Selected{id, "👨‍👩‍👧"}});
    roundTrip({protocolVersion, Cancelled{id, CancelReason::User}});
    roundTrip({protocolVersion, Ping{99}});
    roundTrip({protocolVersion, Pong{99}});
}

void testSerializationBounds() {
    using namespace emoji_palette::ipc;
    const auto id = transaction(9);
    require(!serialize({2, Ping{1}}), "unsupported envelope version serialized");
    require(!serialize({1, Command{id, 1, CommandKind::Right, "unexpected"}}),
            "text attached to navigation command");
    require(!serialize({1, Command{id, 1, CommandKind::SearchText,
                                   std::string(maximumSearchSize + 1, 'a')}}),
            "oversized search serialized");
    require(!serialize({1, Selected{id, std::string(maximumSelectionSize + 1, 'a')}}),
            "oversized selection serialized");
    require(!serialize({1, Selected{id, std::string("\xF0\x28\x8C\x28", 4)}}),
            "malformed UTF-8 selection serialized");
    require(
        !serialize(
            {1,
             Show{
                 id, {0, 0, -1, 10}, {0, 0, 100, 100}, emoji_palette::Locale::English, 100, true}}),
        "invalid rectangle serialized");
    require(!serialize({1, Selected{{}, "😀"}}), "zero transaction serialized");
}

void testMalformedFrames() {
    using namespace emoji_palette::ipc;
    const auto serialized = serialize({1, Selected{transaction(2), "😀"}});
    if (!serialized) {
        throw std::runtime_error("valid selection was not serialized");
    }
    auto frame = serialized.value();
    auto changed = frame;
    changed[0] = 'X';
    require(parse(changed).error == ProtocolError::InvalidMagic, "invalid magic accepted");
    changed = frame;
    changed[5] = 2;
    require(parse(changed).error == ProtocolError::UnsupportedVersion,
            "unsupported version accepted");
    changed = frame;
    changed[6] = 127;
    require(parse(changed).error == ProtocolError::UnknownMessageType,
            "unknown message type accepted");
    changed = frame;
    changed[7] = 1;
    require(parse(changed).error == ProtocolError::InvalidReservedBits, "reserved bits accepted");
    changed = frame;
    changed[11] = static_cast<std::uint8_t>(changed[11] + 1);
    require(parse(changed).error == ProtocolError::SizeMismatch, "wrong payload size accepted");
    changed = frame;
    changed.back() = 0xFF;
    require(parse(changed).error == ProtocolError::InvalidUtf8, "invalid UTF-8 frame accepted");
    changed = frame;
    changed.push_back(0);
    require(parse(changed).error == ProtocolError::SizeMismatch, "trailing data accepted");
    require(parse(std::span<const std::uint8_t>(frame.data(), 5)).error ==
                ProtocolError::FrameTooSmall,
            "short frame accepted");
    std::vector<std::uint8_t> oversized(maximumFrameSize + 1);
    require(parse(oversized).error == ProtocolError::FrameTooLarge, "oversized frame accepted");
}

void testNegotiation() {
    using namespace emoji_palette::ipc;
    const Hello compatible{1, 2, 77, 7};
    const auto welcome = negotiate(compatible, 3);
    require(welcome == Welcome{1, 77, 3}, "capability negotiation failed");
    require(!negotiate({2, 3, 77, 3}, 3), "incompatible version negotiated");
}

void testSession() {
    using namespace emoji_palette::ipc;
    PeerSession session;
    const auto hello = session.begin(":1.42", 1234, 7);
    require(std::get<Hello>(hello.payload).nonce == 1234 &&
                session.state() == SessionState::Negotiating,
            "session did not begin negotiation");
    require(!session.acceptWelcome(":1.99", {1, 1234, 3}), "wrong owner accepted");
    require(!session.acceptWelcome(":1.42", {1, 9999, 3}), "wrong nonce accepted");
    require(session.acceptWelcome(":1.42", {1, 1234, 3}), "valid welcome rejected");
    require(session.accepts(":1.42") && !session.accepts(":1.99"), "ready owner binding failed");
    const auto readyGeneration = session.generation();
    session.ownerLost(":1.99");
    require(session.state() == SessionState::Ready, "unrelated owner loss disconnected session");
    session.ownerLost(":1.42");
    require(session.state() == SessionState::Disconnected &&
                session.generation() == readyGeneration + 1,
            "owner loss did not reset session");
    session.begin(":1.43", 88, 1);
    require(!session.acceptWelcome(":1.43", {2, 88, 1}) &&
                session.state() == SessionState::Incompatible,
            "version mismatch did not terminate negotiation");
    session.begin(":1.44", 89, 1);
    session.negotiationExpired();
    require(session.state() == SessionState::Disconnected, "negotiation timeout failed");
}

void testReconnectAndReplay() {
    using namespace emoji_palette::ipc;
    ReconnectBackoff backoff;
    const std::vector<std::uint32_t> expected = {100, 200, 400, 800, 1600, 3200, 5000, 5000};
    for (const auto delay : expected) {
        require(backoff.nextDelayMilliseconds() == delay, "reconnect delay failed");
    }
    backoff.reset();
    require(backoff.attempt() == 0 && backoff.nextDelayMilliseconds() == 100,
            "reconnect reset failed");

    SequenceValidator validator;
    const auto first = transaction(1);
    const auto second = transaction(2);
    validator.begin(first);
    require(validator.accept(first, 1), "first sequence rejected");
    require(!validator.accept(first, 1), "replayed sequence accepted");
    require(!validator.accept(first, 0), "zero sequence accepted");
    require(!validator.accept(second, 2), "wrong transaction accepted");
    require(validator.accept(first, 3), "monotonic sequence rejected");
    validator.reset();
    require(!validator.accept(first, 4), "sequence accepted after reset");
}

}

int main() {
    try {
        testTransactionId();
        testRoundTrips();
        testSerializationBounds();
        testMalformedFrames();
        testNegotiation();
        testSession();
        testReconnectAndReplay();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
