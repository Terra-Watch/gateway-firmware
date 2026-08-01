#pragma once

#include "NodeRegistry.h"
#include "NodeLivenessManager.h"
#include <cstdint>

namespace jalri::registry {

class NodeRegistryService final {
public:
    explicit NodeRegistryService(std::uint32_t offlineTimeoutMs = 15000U);

    HeartbeatProcessResult handleHeartbeat(std::uint32_t nodeId,
                                           std::uint32_t packetId,
                                           int rssiDbm,
                                           float snrDb,
                                           std::uint32_t timestampMs,
                                           std::uint8_t fwMajor = 0,
                                           std::uint8_t fwMinor = 1,
                                           std::uint8_t fwPatch = 0,
                                           std::uint8_t batteryLevel = 95);

    void tick(std::uint32_t currentMs);

    NodeRegistry& registry() { return registry_; }
    const NodeRegistry& registry() const { return registry_; }

    NodeLivenessManager& livenessManager() { return livenessManager_; }
    const NodeLivenessManager& livenessManager() const { return livenessManager_; }

    void printStats();

private:
    NodeRegistry registry_;
    NodeLivenessManager livenessManager_;
    std::uint32_t lastStatsPrintMs_{0};
};

}  // namespace jalri::registry
