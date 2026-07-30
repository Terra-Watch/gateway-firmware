#include "MockPacketParser.h"

namespace jalri::protocol {

PacketParseResult MockPacketParser::parse(const std::uint8_t* bytes,
                                          std::size_t length,
                                          ParsedPacketView& packet) const {
    static std::uint32_t seqCounter = 1;

    std::uint32_t extractedNodeId = 0x1001U;
    std::uint32_t extractedSeq = seqCounter++;

    if (bytes != nullptr && length >= 4) {
        extractedNodeId = (static_cast<std::uint32_t>(bytes[0]) << 24) |
                          (static_cast<std::uint32_t>(bytes[1]) << 16) |
                          (static_cast<std::uint32_t>(bytes[2]) << 8) |
                          static_cast<std::uint32_t>(bytes[3]);
    }
    if (bytes != nullptr && length >= 8) {
        extractedSeq = (static_cast<std::uint32_t>(bytes[4]) << 24) |
                       (static_cast<std::uint32_t>(bytes[5]) << 16) |
                       (static_cast<std::uint32_t>(bytes[6]) << 8) |
                       static_cast<std::uint32_t>(bytes[7]);
    }

    packet.nodeId = extractedNodeId;
    packet.packetTypeName = "HEARTBEAT";
    packet.protocolVersion = 1;
    packet.sequenceNumber = extractedSeq;
    packet.hopCount = 0;
    packet.ttl = 10;
    packet.payloadLength = length;

    return PacketParseResult::success;
}

}  // namespace jalri::protocol

