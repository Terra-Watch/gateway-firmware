#pragma once

#include "NodeInfo.h"
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace jalri::registry {

class NodeRegistry final {
public:
    NodeRegistry() = default;

    HeartbeatProcessResult processHeartbeat(std::uint32_t nodeId,
                                           std::uint32_t packetId,
                                           int rssiDbm,
                                           float snrDb,
                                           std::uint32_t timestampMs,
                                           std::uint8_t fwMajor = 0,
                                           std::uint8_t fwMinor = 1,
                                           std::uint8_t fwPatch = 0,
                                           std::uint8_t batteryLevel = 95);

    NodeInfo* findNode(std::uint32_t nodeId);
    const NodeInfo* findNode(std::uint32_t nodeId) const;

    RegistryStats calculateStats() const;

    std::vector<NodeInfo>& getAllNodes() { return nodes_; }
    const std::vector<NodeInfo>& getAllNodes() const { return nodes_; }

    void clear();

private:
    std::vector<NodeInfo> nodes_;
    std::unordered_map<std::uint32_t, std::uint32_t> lastPacketIdMap_;
    std::uint32_t totalPacketsReceived_{0};
    std::uint32_t totalDuplicatePackets_{0};
    double totalRssiSum_{0.0};
    double totalSnrSum_{0.0};
};

}  // namespace jalri::registry
