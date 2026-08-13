#include "PacketParser.h"

namespace jalri::protocol {
namespace {
constexpr std::size_t headerSize = 10U;
constexpr std::size_t crcSize = 2U;
constexpr std::size_t maxPayloadSize = 48U;

std::uint16_t crc16Ccitt(const std::uint8_t* data, std::size_t length) {
    std::uint16_t crc = 0xFFFFU;
    for (std::size_t i = 0; i < length; ++i) {
        crc ^= static_cast<std::uint16_t>(data[i]) << 8U;
        for (std::uint8_t bit = 0; bit < 8U; ++bit) {
            crc = (crc & 0x8000U) != 0U
                ? static_cast<std::uint16_t>((crc << 1U) ^ 0x1021U)
                : static_cast<std::uint16_t>(crc << 1U);
        }
    }
    return crc;
}
}  // namespace

PacketParseResult PacketParser::parse(const std::uint8_t* bytes,
                                      std::size_t length,
                                      ParsedPacketView& packet) const {
    if (bytes == nullptr || length < headerSize + crcSize || bytes[0] != 0xA5U || bytes[1] != 0x03U) {
        return PacketParseResult::parseFailed;
    }

    const std::size_t payloadLength = bytes[9];
    const std::size_t expectedLength = headerSize + payloadLength + crcSize;
    if (payloadLength > maxPayloadSize || length != expectedLength) {
        return PacketParseResult::parseFailed;
    }

    const std::uint16_t expectedCrc = static_cast<std::uint16_t>(bytes[length - 2U]) |
                                      (static_cast<std::uint16_t>(bytes[length - 1U]) << 8U);
    if (crc16Ccitt(bytes, length - crcSize) != expectedCrc) {
        return PacketParseResult::parseFailed;
    }

    packet = {};
    packet.protocolVersion = bytes[1];
    packet.packetType = bytes[2];
    packet.nodeId = bytes[3];
    packet.destinationId = bytes[4];
    packet.sequenceNumber = static_cast<std::uint32_t>(bytes[5]) |
                            (static_cast<std::uint32_t>(bytes[6]) << 8U);
    packet.ttl = bytes[7];
    packet.hopCount = bytes[8];
    packet.payloadLength = payloadLength;

    switch (packet.packetType) {
    case 1U: packet.packetTypeName = "DATA"; break;
    case 2U: packet.packetTypeName = "HEARTBEAT"; break;
    case 3U: packet.packetTypeName = "ACKNOWLEDGEMENT"; break;
    case 4U: packet.packetTypeName = "HEARTBEAT_ACK"; break;
    default: packet.packetTypeName = "UNKNOWN"; break;
    }

    if (packet.packetType == 2U && payloadLength >= 4U) {
        packet.packetId = static_cast<std::uint32_t>(bytes[10]) |
                          (static_cast<std::uint32_t>(bytes[11]) << 8U) |
                          (static_cast<std::uint32_t>(bytes[12]) << 16U) |
                          (static_cast<std::uint32_t>(bytes[13]) << 24U);
    }
    return PacketParseResult::success;
}

}  // namespace jalri::protocol
