#include <array>
#include <cassert>
#include <cstdint>

#include "Arduino.h"
#include "Config.h"
#include "NodeManager.h"
#include "PacketFactory.h"
#include "Parser.h"
#include "Serializer.h"
#include "Validator.h"
#include "GatewayAckHandler.h"
#include "PacketParser.h"
#include "PacketValidator.h"

TestSerial Serial;

int main() {
    jalari::node::NodeManager node(jalari::config::kDefaultFirmwareConfig.node,
                                   jalari::node::DeviceRole::BoatNode);
    jalari::protocol::PacketFactory factory(node, jalari::config::kDefaultFirmwareConfig.network);
    jalari::protocol::Packet heartbeat;
    std::uint32_t packetId = 0;
    assert(factory.createHeartbeat(123456U, 95U, 1U, heartbeat, packetId));
    assert(packetId == 1U);

    std::array<std::uint8_t, jalari::constants::kMaxPacketSize> bytes{};
    std::size_t packetSize = 0;
    assert(jalari::protocol::Serializer::serialize(heartbeat, bytes, packetSize));
    assert(packetSize == 25U);

    jalri::protocol::PacketParser gatewayParser;
    jalri::protocol::PacketValidator gatewayValidator;
    jalri::protocol::ParsedPacketView parsed{};
    assert(gatewayParser.parse(bytes.data(), packetSize, parsed) == jalri::protocol::PacketParseResult::success);
    assert(gatewayValidator.validate(parsed) == jalri::protocol::PacketValidationResult::success);
    assert(parsed.nodeId == 1U && parsed.packetId == packetId);
    assert(parsed.destinationId == 0xFFU && parsed.sequenceNumber == 0U);
    assert(parsed.packetType == 2U && parsed.payloadLength == 13U);

    jalri::gateway::GatewayAckHandler ackHandler;
    std::array<std::uint8_t, 21> ackBytes{};
    assert(ackHandler.buildHeartbeatAckPacket(1U, packetId, 777U, ackBytes));

    jalari::protocol::Packet ack;
    assert(jalari::protocol::Parser::parse(ackBytes.data(), ackBytes.size(), ack));
    assert(jalari::protocol::Validator::isValid(ack));
    assert(ack.type == jalari::protocol::PacketType::HeartbeatAck);
    assert(ack.sourceId == jalari::constants::kGatewayNodeId && ack.destinationId == 1U);
    const auto ackedId = static_cast<std::uint32_t>(ack.payload[0]) |
                         (static_cast<std::uint32_t>(ack.payload[1]) << 8U) |
                         (static_cast<std::uint32_t>(ack.payload[2]) << 16U) |
                         (static_cast<std::uint32_t>(ack.payload[3]) << 24U);
    assert(ackedId == packetId && ack.payload[4] == 1U);

    auto malformed = bytes;
    malformed[packetSize - 1U] ^= 0x01U;
    assert(!jalari::protocol::Parser::parse(malformed.data(), packetSize, heartbeat));

    auto wrongAckId = ackBytes;
    wrongAckId[10] ^= 0x01U;
    assert(jalari::protocol::Parser::parse(wrongAckId.data(), wrongAckId.size(), ack) == false);
    auto wrongSource = ackBytes;
    wrongSource[3] = 0xFD;
    assert(!jalari::protocol::Parser::parse(wrongSource.data(), wrongSource.size(), ack));
    auto wrongType = ackBytes;
    wrongType[2] = 3U;
    assert(!jalari::protocol::Parser::parse(wrongType.data(), wrongType.size(), ack));

    return 0;
}
