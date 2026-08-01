#pragma once

#include <cstddef>
#include <cstdint>
#include "NodeStatus.h"

namespace jalri::registry {

struct NodeInfo {
    std::uint32_t nodeId{0};
    std::uint32_t lastPacketId{0};
    std::uint32_t lastSeenTimestampMs{0};
    std::uint32_t firstSeenTimestampMs{0};
    int rssiDbm{0};
    float snrDb{0.0f};
    std::uint8_t fwMajor{0};
    std::uint8_t fwMinor{1};
    std::uint8_t fwPatch{0};
    std::uint8_t batteryLevel{95};
    std::uint32_t heartbeatCounter{0};
    NodeStatus status{NodeStatus::online};
    std::uint32_t lastRttMs{0};
};

struct RegistryStats {
    std::size_t totalNodes{0};
    std::size_t onlineNodes{0};
    std::size_t offlineNodes{0};
    std::uint32_t packetsReceived{0};
    std::uint32_t duplicatePackets{0};
    float averageRssiDbm{0.0f};
    float averageSnrDb{0.0f};
};

enum class HeartbeatProcessResult {
    newDiscovery,
    updatedExisting,
    duplicate
};

}  // namespace jalri::registry
