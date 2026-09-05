#pragma once

#include <cstddef>
#include <cstdint>

namespace jalri::protocol {

enum class PacketParseResult {
    success,
    parseFailed,
};

enum class PacketValidationResult {
    success,
    invalidHeader,
    invalidVersion,
    invalidCrc,
    invalidLength,
    invalidPacketType,
    unsupportedProtocol,
};

struct ParsedPacketView {
    std::uint32_t nodeId;
    std::uint32_t packetId;
    std::uint8_t destinationId;
    std::uint8_t previousHopId;
    std::uint8_t packetType;
    const char* packetTypeName;
    std::uint8_t protocolVersion;
    std::uint32_t sequenceNumber;
    std::uint8_t hopCount;
    std::uint8_t ttl;
    std::size_t payloadLength;
};

const char* toString(PacketParseResult result);
const char* toString(PacketValidationResult result);

}  // namespace jalri::protocol
