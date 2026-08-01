#include "NodeLivenessManager.h"
#include <Arduino.h>

namespace jalri::registry {

NodeLivenessManager::NodeLivenessManager(std::uint32_t offlineTimeoutMs)
    : offlineTimeoutMs_(offlineTimeoutMs) {}

void NodeLivenessManager::evaluateLiveness(NodeRegistry& registry, std::uint32_t currentMs) {
    if (currentMs - lastEvaluationMs_ < 1000U) {
        return;
    }
    lastEvaluationMs_ = currentMs;

    for (auto& node : registry.getAllNodes()) {
        if (node.status == NodeStatus::online) {
            const std::uint32_t elapsedMs = currentMs - node.lastSeenTimestampMs;
            if (elapsedMs > offlineTimeoutMs_) {
                node.status = NodeStatus::offline;

                Serial.println("--------------------------------");
                Serial.println("Node Offline");
                Serial.printf("Node ID: 0x%04X\n", static_cast<unsigned>(node.nodeId));
                Serial.printf("Last Seen: %lu ms ago\n", static_cast<unsigned long>(elapsedMs));
                Serial.printf("Elapsed: %lu ms\n", static_cast<unsigned long>(elapsedMs));
                Serial.println("--------------------------------");
                Serial.flush();
            }
        }
    }
}

void NodeLivenessManager::handleNodeReconnected(NodeInfo& node) {
    if (node.status == NodeStatus::offline) {
        node.status = NodeStatus::online;

        Serial.println("--------------------------------");
        Serial.println("Node Back Online");
        Serial.printf("Node ID: 0x%04X\n", static_cast<unsigned>(node.nodeId));
        Serial.println("--------------------------------");
        Serial.flush();
    }
}

}  // namespace jalri::registry
