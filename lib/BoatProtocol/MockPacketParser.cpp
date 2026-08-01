#include "MockPacketParser.h"

namespace jalri::protocol {

PacketParseResult MockPacketParser::parse(const std::uint8_t* bytes,
                                          std::size_t length,
                                          ParsedPacketView& packet) const {
    if (bytes == nullptr || length < 10) {
        return PacketParseResult::parseFailed;
    }

    if (bytes[0] != 0xA5) {
        return PacketParseResult::parseFailed;
    }

    packet.protocolVersion = bytes[1];
    packet.nodeId = static_cast<std::uint32_t>(bytes[3]);
    packet.sequenceNumber = static_cast<std::uint32_t>(bytes[5]) |
                            (static_cast<std::uint32_t>(bytes[6]) << 8);
    packet.ttl = bytes[7];
    packet.hopCount = bytes[8];
    packet.payloadLength = bytes[9];

    if (bytes[2] == 2) {
        packet.packetTypeName = "HEARTBEAT";
    } else if (bytes[2] == 4) {
        packet.packetTypeName = "HEARTBEAT_ACK";
    } else {
        packet.packetTypeName = "DATA";
    }

    if (length >= 14) {
        packet.packetId = static_cast<std::uint32_t>(bytes[10]) |
                          (static_cast<std::uint32_t>(bytes[11]) << 8) |
                          (static_cast<std::uint32_t>(bytes[12]) << 16) |
                          (static_cast<std::uint32_t>(bytes[13]) << 24);
    } else {
        packet.packetId = 0;
    }

    return PacketParseResult::success;
}

}  // namespace jalri::protocol

