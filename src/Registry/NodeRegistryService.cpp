#include "NodeRegistryService.h"
#include <Arduino.h>

namespace jalri::registry {

NodeRegistryService::NodeRegistryService(std::uint32_t offlineTimeoutMs)
    : livenessManager_(offlineTimeoutMs) {}

HeartbeatProcessResult NodeRegistryService::handleHeartbeat(std::uint32_t nodeId,
                                                             std::uint32_t packetId,
                                                             int rssiDbm,
                                                             float snrDb,
                                                             std::uint32_t timestampMs,
                                                             std::uint8_t fwMajor,
                                                             std::uint8_t fwMinor,
                                                             std::uint8_t fwPatch,
                                                             std::uint8_t batteryLevel) {
    NodeInfo* existingNode = registry_.findNode(nodeId);
    const bool wasOffline = (existingNode != nullptr && existingNode->status == NodeStatus::offline);

    const HeartbeatProcessResult res = registry_.processHeartbeat(
        nodeId, packetId, rssiDbm, snrDb, timestampMs, fwMajor, fwMinor, fwPatch, batteryLevel
    );

    if (res == HeartbeatProcessResult::newDiscovery) {
        Serial.println("--------------------------------");
        Serial.println("NEW NODE DISCOVERED");
        Serial.printf("Node: 0x%04X\n", static_cast<unsigned>(nodeId));
        Serial.printf("RSSI: %d dBm\n", rssiDbm);
        Serial.printf("SNR: %.1f dB\n", static_cast<double>(snrDb));
        Serial.println("--------------------------------");
        Serial.flush();
    } else if (res == HeartbeatProcessResult::updatedExisting && wasOffline) {
        NodeInfo* node = registry_.findNode(nodeId);
        if (node != nullptr) {
            livenessManager_.handleNodeReconnected(*node);
        }
    }

    return res;
}

void NodeRegistryService::tick(std::uint32_t currentMs) {
    livenessManager_.evaluateLiveness(registry_, currentMs);

    if (currentMs - lastStatsPrintMs_ >= 10000U) {
        lastStatsPrintMs_ = currentMs;
        printStats();
    }
}

void NodeRegistryService::printStats() {
    const RegistryStats stats = registry_.calculateStats();

    Serial.println("================================");
    Serial.println("NODE REGISTRY STATISTICS");
    Serial.printf("Total Nodes: %zu\n", stats.totalNodes);
    Serial.printf("Online Nodes: %zu\n", stats.onlineNodes);
    Serial.printf("Offline Nodes: %zu\n", stats.offlineNodes);
    Serial.printf("Packets Received: %lu\n", static_cast<unsigned long>(stats.packetsReceived));
    Serial.printf("Duplicate Packets: %lu\n", static_cast<unsigned long>(stats.duplicatePackets));
    Serial.printf("Average RSSI: %.1f dBm\n", static_cast<double>(stats.averageRssiDbm));
    Serial.printf("Average SNR: %.1f dB\n", static_cast<double>(stats.averageSnrDb));
    Serial.println("================================");
    Serial.flush();
}

}  // namespace jalri::registry
