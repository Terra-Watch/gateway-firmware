#pragma once

#include <cstdint>
#include <map>
#include <cstddef>
#include "ILoRaDriver.h"

namespace jalri::gateway {

struct GatewayStats {
    std::uint32_t packetsReceived = 0;
    std::uint32_t ackSent = 0;
    std::uint32_t duplicatePackets = 0;
    std::uint32_t lastPacketId = 0;
    int rssi = 0;
    float snr = 0.0f;
    std::uint32_t lastSeenMs = 0;
};

struct DuplicateKey {
    std::uint8_t sourceId;
    std::uint8_t type;
    std::uint16_t sequenceNumber;

    bool operator<(const DuplicateKey& other) const {
        if (sourceId != other.sourceId) return sourceId < other.sourceId;
        if (type != other.type) return type < other.type;
        return sequenceNumber < other.sequenceNumber;
    }
};

class GatewayAckHandler {
public:
    GatewayAckHandler() = default;

    // Returns true if duplicate, false if new
    bool processHeartbeat(std::uint32_t nodeId,
                         std::uint32_t packetId,
                         int rssiDbm,
                         float snrDb,
                         std::uint32_t timestampMs,
                         lora::ILoRaDriver* radioDriver);

    bool processDataPacket(std::uint8_t targetNodeId,
                          std::uint16_t sequenceNumber,
                          int rssiDbm,
                          float snrDb,
                          std::uint32_t timestampMs,
                          lora::ILoRaDriver* radioDriver);

    const GatewayStats& stats() const { return stats_; }

private:
    void sendAckPacket_(std::uint8_t targetNodeId,
                       std::uint32_t packetId,
                       std::uint32_t timestampMs,
                       lora::ILoRaDriver* radioDriver);

    void sendDataAckPacket_(std::uint8_t targetNodeId,
                           std::uint16_t sequenceNumber,
                           lora::ILoRaDriver* radioDriver);

    static std::uint16_t computeCrc16_(const std::uint8_t* data, std::size_t length);

    std::map<std::uint32_t, std::uint32_t> lastPacketIdMap_{};
    std::map<DuplicateKey, std::uint32_t> dataDuplicateMap_{};
    GatewayStats stats_{};
    std::uint16_t ackSeq_ = 1;
};

}  // namespace jalri::gateway
