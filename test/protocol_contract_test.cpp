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

class FakeRadio final : public terrawatch::lora::ILoRaDriver {
public:
    bool begin() override { return true; }
    bool receive(terrawatch::lora::RawRadioPacket&) override { return false; }
    bool send(const std::uint8_t* bytes, std::size_t length) override {
        last.assign(bytes, bytes + length);
        return true;
    }
    std::vector<std::uint8_t> last;
};

int main() {
    terrawatch::node::NodeManager node(terrawatch::config::kDefaultFirmwareConfig.node,
                                   terrawatch::node::DeviceRole::FieldNode);
    terrawatch::protocol::PacketFactory factory(node, terrawatch::config::kDefaultFirmwareConfig.network);
    terrawatch::protocol::Packet heartbeat;
    std::uint32_t packetId = 0;
    assert(factory.createHeartbeat(123456U, 95U, 1U, heartbeat, packetId));
    assert(packetId == 1U);

    std::array<std::uint8_t, terrawatch::constants::kMaxPacketSize> bytes{};
    std::size_t packetSize = 0;
    assert(terrawatch::protocol::Serializer::serialize(heartbeat, bytes, packetSize));
    assert(bytes[0] == 0xA5U);
    assert(bytes[1] == 0x05U);
    assert(packetSize == 27U);

    terrawatch::protocol::PacketParser gatewayParser;
    terrawatch::protocol::PacketValidator gatewayValidator;
    terrawatch::protocol::ParsedPacketView parsed{};
    assert(gatewayParser.parse(bytes.data(), packetSize, parsed) == terrawatch::protocol::PacketParseResult::success);
    assert(gatewayValidator.validate(parsed) == terrawatch::protocol::PacketValidationResult::success);
    assert(parsed.nodeId == 1U && parsed.packetId == packetId);
    assert(parsed.destinationId == 0xFFU && parsed.sequenceNumber == 0U);
    assert(parsed.packetType == 2U && parsed.payloadLength == 13U);

    terrawatch::gateway::GatewayAckHandler ackHandler;
    std::array<std::uint8_t, 23> ackBytes{};
    assert(ackHandler.buildHeartbeatAckPacket(1U, packetId, 777U, ackBytes));

    terrawatch::protocol::Packet ack;
    assert(terrawatch::protocol::Parser::parse(ackBytes.data(), ackBytes.size(), ack));
    assert(terrawatch::protocol::Validator::isValid(ack));
    assert(ack.type == terrawatch::protocol::PacketType::HeartbeatAck);
    assert(ack.sourceId == terrawatch::constants::kGatewayNodeId && ack.destinationId == 1U);
    assert(ack.previousHopId == terrawatch::constants::kGatewayNodeId);
    const auto ackedId = static_cast<std::uint32_t>(ack.payload[0]) |
                         (static_cast<std::uint32_t>(ack.payload[1]) << 8U) |
                         (static_cast<std::uint32_t>(ack.payload[2]) << 16U) |
                         (static_cast<std::uint32_t>(ack.payload[3]) << 24U);
    assert(ackedId == packetId && ack.payload[4] == 1U);

    terrawatch::protocol::Packet data;
    const std::uint8_t dataPayload[] = {0x10, 0x20, 0x30};
    assert(factory.createData(0xFEU, dataPayload, sizeof(dataPayload), data));
    data.previousHopId = 2U;
    std::array<std::uint8_t, terrawatch::constants::kMaxPacketSize> dataBytes{};
    std::size_t dataSize = 0;
    assert(terrawatch::protocol::Serializer::serialize(data, dataBytes, dataSize));
    assert(gatewayParser.parse(dataBytes.data(), dataSize, parsed) == terrawatch::protocol::PacketParseResult::success);
    assert(gatewayValidator.validate(parsed) == terrawatch::protocol::PacketValidationResult::success);
    assert(parsed.packetType == 1U && parsed.nodeId == 1U && parsed.destinationId == 0xFEU);
    assert(parsed.sequenceNumber == data.sequenceNumber);
    assert(parsed.previousHopId == data.previousHopId);

    FakeRadio radio;
    terrawatch::gateway::GatewayAckHandler dataAckHandler;
    assert(!dataAckHandler.processDataPacket(1U, 0xFEU, data.previousHopId, data.sequenceNumber, -80, 7.0F, 100U, &radio));
    assert(dataAckHandler.stats().ackSent == 1U);
    assert(dataAckHandler.processDataPacket(1U, 0xFEU, data.previousHopId, data.sequenceNumber, -80, 7.0F, 101U, &radio));
    assert(dataAckHandler.stats().duplicatePackets == 1U);
    // The data came through relay 2, not straight from node 1: the gateway acknowledges the relay, then sends node 1 an end-to-end receipt via that relay.
    assert(radio.last.size() == 16U);
    assert(radio.last[1] == 0x05U && radio.last[2] == 0x06U);                  // protocol v5, PacketType::EndToEndAck
    assert(radio.last[3] == terrawatch::constants::kGatewayNodeId && radio.last[4] == 1U);
    assert(radio.last[5] == terrawatch::constants::kGatewayNodeId && radio.last[11] == data.previousHopId);
    terrawatch::protocol::Packet receipt;
    assert(terrawatch::protocol::Parser::parse(radio.last.data(), radio.last.size(), receipt) && terrawatch::protocol::Validator::isValid(receipt));
    assert(receipt.type == terrawatch::protocol::PacketType::EndToEndAck && receipt.nextHopId == data.previousHopId);
    assert(receipt.payload[0] == static_cast<std::uint8_t>(data.sequenceNumber & 0xFFU));
    assert(!dataAckHandler.processDataPacket(1U, 0xFFU, data.previousHopId, data.sequenceNumber + 1U, -80, 7.0F, 102U, &radio));
    assert(dataAckHandler.stats().ackSent == 2U);
    // A resend 10 s later is still the same packet (the node resends unconfirmed alerts after 12 s or more); after two minutes it is new.
    assert(dataAckHandler.processDataPacket(1U, 0xFEU, data.previousHopId, data.sequenceNumber, -80, 7.0F, 10100U, &radio));
    assert(!dataAckHandler.processDataPacket(1U, 0xFEU, data.previousHopId, data.sequenceNumber, -80, 7.0F, 130000U, &radio));
    assert(dataAckHandler.stats().packetsReceived == 3U);

    auto malformed = bytes;
    malformed[packetSize - 1U] ^= 0x01U;
    assert(!terrawatch::protocol::Parser::parse(malformed.data(), packetSize, heartbeat));

    auto legacy = bytes;
    legacy[1] = 4U; // the version before the next-hop byte
    assert(gatewayParser.parse(legacy.data(), packetSize, parsed) == terrawatch::protocol::PacketParseResult::parseFailed);

    auto wrongAckId = ackBytes;
    wrongAckId[10] ^= 0x01U;
    assert(terrawatch::protocol::Parser::parse(wrongAckId.data(), wrongAckId.size(), ack) == false);
    auto wrongSource = ackBytes;
    wrongSource[3] = 0xFD;
    assert(!terrawatch::protocol::Parser::parse(wrongSource.data(), wrongSource.size(), ack));
    auto wrongType = ackBytes;
    wrongType[2] = 3U;
    assert(!terrawatch::protocol::Parser::parse(wrongType.data(), wrongType.size(), ack));

    return 0;
}
