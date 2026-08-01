#pragma once

#include "NodeRegistry.h"
#include <cstdint>

namespace jalri::registry {

class NodeLivenessManager final {
public:
    explicit NodeLivenessManager(std::uint32_t offlineTimeoutMs = 15000U);

    void evaluateLiveness(NodeRegistry& registry, std::uint32_t currentMs);

    void handleNodeReconnected(NodeInfo& node);

    std::uint32_t offlineTimeoutMs() const { return offlineTimeoutMs_; }
    void setOfflineTimeoutMs(std::uint32_t timeoutMs) { offlineTimeoutMs_ = timeoutMs; }

private:
    std::uint32_t offlineTimeoutMs_;
    std::uint32_t lastEvaluationMs_{0};
};

}  // namespace jalri::registry
