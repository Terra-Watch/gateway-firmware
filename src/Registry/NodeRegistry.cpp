#include "NodeRegistry.h"
#include <Arduino.h>
#include <algorithm>

namespace jalri::registry {

HeartbeatProcessResult NodeRegistry::processHeartbeat(std::uint32_t nodeId,
                                                       std::uint32_t packetId,
                                                       int rssiDbm,
                                                       float snrDb,
                                                       std::uint32_t timestampMs,
                                                       std::uint8_t fwMajor,
                                                       std::uint8_t fwMinor,
                                                       std::uint8_t fwPatch,
                                                       std::uint8_t batteryLevel) {
    auto it = lastPacketIdMap_.find(nodeId);
    if (it != lastPacketIdMap_.end() && it->second == packetId && packetId != 0) {
        totalDuplicatePackets_++;
        return HeartbeatProcessResult::duplicate;
    }

    lastPacketIdMap_[nodeId] = packetId;

    for (auto& node : nodes_) {
        if (node.nodeId == nodeId) {
            node.lastPacketId = packetId;
            node.lastSeenTimestampMs = timestampMs;
            node.rssiDbm = rssiDbm;
            node.snrDb = snrDb;
            node.fwMajor = fwMajor;
            node.fwMinor = fwMinor;
            node.fwPatch = fwPatch;
            node.batteryLevel = batteryLevel;
            node.heartbeatCounter++;
            
            totalPacketsReceived_++;
            totalRssiSum_ += rssiDbm;
            totalSnrSum_ += snrDb;

            return HeartbeatProcessResult::updatedExisting;
        }
    }

    NodeInfo newNode{};
    newNode.nodeId = nodeId;
    newNode.lastPacketId = packetId;
    newNode.firstSeenTimestampMs = timestampMs;
    newNode.lastSeenTimestampMs = timestampMs;
    newNode.rssiDbm = rssiDbm;
    newNode.snrDb = snrDb;
    newNode.fwMajor = fwMajor;
    newNode.fwMinor = fwMinor;
    newNode.fwPatch = fwPatch;
    newNode.batteryLevel = batteryLevel;
    newNode.heartbeatCounter = 1;
    newNode.status = NodeStatus::online;

    nodes_.push_back(newNode);

    totalPacketsReceived_++;
    totalRssiSum_ += rssiDbm;
    totalSnrSum_ += snrDb;

    return HeartbeatProcessResult::newDiscovery;
}

NodeInfo* NodeRegistry::findNode(std::uint32_t nodeId) {
    for (auto& node : nodes_) {
        if (node.nodeId == nodeId) {
            return &node;
        }
    }
    return nullptr;
}

const NodeInfo* NodeRegistry::findNode(std::uint32_t nodeId) const {
    for (const auto& node : nodes_) {
        if (node.nodeId == nodeId) {
            return &node;
        }
    }
    return nullptr;
}

RegistryStats NodeRegistry::calculateStats() const {
    RegistryStats stats{};
    stats.totalNodes = nodes_.size();
    stats.packetsReceived = totalPacketsReceived_;
    stats.duplicatePackets = totalDuplicatePackets_;

    for (const auto& node : nodes_) {
        if (node.status == NodeStatus::online) {
            stats.onlineNodes++;
        } else {
            stats.offlineNodes++;
        }
    }

    if (totalPacketsReceived_ > 0) {
        stats.averageRssiDbm = static_cast<float>(totalRssiSum_ / totalPacketsReceived_);
        stats.averageSnrDb = static_cast<float>(totalSnrSum_ / totalPacketsReceived_);
    }

    return stats;
}

void NodeRegistry::clear() {
    nodes_.clear();
    lastPacketIdMap_.clear();
    totalPacketsReceived_ = 0;
    totalDuplicatePackets_ = 0;
    totalRssiSum_ = 0.0;
    totalSnrSum_ = 0.0;
}

}  // namespace jalri::registry
