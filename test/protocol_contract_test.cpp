#include <array>
#include <cassert>
#include <cstdint>
#include <vector>

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

class FakeRadio final : public jalri::lora::ILoRaDriver {
public:
    bool begin() override { return true; }
    bool receive(jalri::lora::RawRadioPacket&) override { return false; }
    bool send(const std::uint8_t* bytes, std::size_t length) override {
        last.assign(bytes, bytes + length);
        return true;
    }
    std::vector<std::uint8_t> last;
};

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
    assert(bytes[0] == 0xA5U);
    assert(bytes[1] == 0x04U);
    assert(packetSize == 26U);

    jalri::protocol::PacketParser gatewayParser;
    jalri::protocol::PacketValidator gatewayValidator;
    jalri::protocol::ParsedPacketView parsed{};
    assert(gatewayParser.parse(bytes.data(), packetSize, parsed) == jalri::protocol::PacketParseResult::success);
    assert(gatewayValidator.validate(parsed) == jalri::protocol::PacketValidationResult::success);
    assert(parsed.nodeId == 1U && parsed.packetId == packetId);
    assert(parsed.destinationId == 0xFFU && parsed.sequenceNumber == 0U);
    assert(parsed.packetType == 2U && parsed.payloadLength == 13U);

    jalri::gateway::GatewayAckHandler ackHandler;
    std::array<std::uint8_t, 22> ackBytes{};
    assert(ackHandler.buildHeartbeatAckPacket(1U, packetId, 777U, ackBytes));

    jalari::protocol::Packet ack;
    assert(jalari::protocol::Parser::parse(ackBytes.data(), ackBytes.size(), ack));
    assert(jalari::protocol::Validator::isValid(ack));
    assert(ack.type == jalari::protocol::PacketType::HeartbeatAck);
    assert(ack.sourceId == jalari::constants::kGatewayNodeId && ack.destinationId == 1U);
    assert(ack.previousHopId == jalari::constants::kGatewayNodeId);
    const auto ackedId = static_cast<std::uint32_t>(ack.payload[0]) |
                         (static_cast<std::uint32_t>(ack.payload[1]) << 8U) |
                         (static_cast<std::uint32_t>(ack.payload[2]) << 16U) |
                         (static_cast<std::uint32_t>(ack.payload[3]) << 24U);
    assert(ackedId == packetId && ack.payload[4] == 1U);

    jalari::protocol::Packet data;
    const std::uint8_t dataPayload[] = {0x10, 0x20, 0x30};
    assert(factory.createData(0xFEU, dataPayload, sizeof(dataPayload), data));
    data.previousHopId = 2U;
    std::array<std::uint8_t, jalari::constants::kMaxPacketSize> dataBytes{};
    std::size_t dataSize = 0;
    assert(jalari::protocol::Serializer::serialize(data, dataBytes, dataSize));
    assert(gatewayParser.parse(dataBytes.data(), dataSize, parsed) == jalri::protocol::PacketParseResult::success);
    assert(gatewayValidator.validate(parsed) == jalri::protocol::PacketValidationResult::success);
    assert(parsed.packetType == 1U && parsed.nodeId == 1U && parsed.destinationId == 0xFEU);
    assert(parsed.sequenceNumber == data.sequenceNumber);
    assert(parsed.previousHopId == data.previousHopId);

    FakeRadio radio;
    jalri::gateway::GatewayAckHandler dataAckHandler;
    assert(!dataAckHandler.processDataPacket(1U, 0xFEU, data.previousHopId, data.sequenceNumber, -80, 7.0F, 100U, &radio));
    assert(dataAckHandler.stats().ackSent == 1U);
    assert(dataAckHandler.processDataPacket(1U, 0xFEU, data.previousHopId, data.sequenceNumber, -80, 7.0F, 101U, &radio));
    assert(dataAckHandler.stats().duplicatePackets == 1U);
    assert(radio.last.size() == 15U);
    assert(radio.last[4] == data.previousHopId);
    assert(radio.last[5] == jalari::constants::kGatewayNodeId);
    assert(!dataAckHandler.processDataPacket(1U, 0xFFU, data.previousHopId, data.sequenceNumber + 1U, -80, 7.0F, 102U, &radio));
    assert(dataAckHandler.stats().ackSent == 2U);
    assert(!dataAckHandler.processDataPacket(1U, 0xFEU, data.previousHopId, data.sequenceNumber, -80, 7.0F, 10100U, &radio));
    assert(dataAckHandler.stats().packetsReceived == 3U);

    auto malformed = bytes;
    malformed[packetSize - 1U] ^= 0x01U;
    assert(!jalari::protocol::Parser::parse(malformed.data(), packetSize, heartbeat));

    auto legacy = bytes;
    legacy[1] = 3U;
    assert(gatewayParser.parse(legacy.data(), packetSize, parsed) == jalri::protocol::PacketParseResult::parseFailed);

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
