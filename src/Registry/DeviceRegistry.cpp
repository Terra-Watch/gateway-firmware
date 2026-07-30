#include "DeviceRegistry.h"

namespace jalri::registry {

const char* toString(OnlineStatus status) {
    switch (status) {
        case OnlineStatus::online:
            return "ONLINE";
        case OnlineStatus::offline:
            return "OFFLINE";
        case OnlineStatus::unknown:
        default:
            return "UNKNOWN";
    }
}

OnlineStatus DeviceRecord::calculateStatus(std::uint32_t currentTimestampMs, std::uint32_t timeoutMs) const {
    if (firstSeenTimestampMs == 0 && lastSeenTimestampMs == 0) {
        return OnlineStatus::unknown;
    }
    if (currentTimestampMs >= lastSeenTimestampMs && (currentTimestampMs - lastSeenTimestampMs) <= timeoutMs) {
        return OnlineStatus::online;
    }
    return OnlineStatus::offline;
}

DeviceRegistry::DeviceRegistry(std::uint32_t offlineTimeoutMs)
    : offlineTimeoutMs_(offlineTimeoutMs) {}

bool DeviceRegistry::registerPacket(std::uint32_t nodeId,
                                     const char* packetType,
                                     std::uint32_t sequenceNumber,
                                     int rssiDbm,
                                     float snrDb,
                                     std::uint32_t timestampMs) {
    for (auto& device : devices_) {
        if (device.nodeId == nodeId) {
            device.lastSeenTimestampMs = timestampMs;
            device.lastRssiDbm = rssiDbm;
            device.lastSnrDb = snrDb;
            device.lastPacketType = (packetType != nullptr) ? packetType : "UNKNOWN";
            device.lastSequenceNumber = sequenceNumber;
            device.packetsReceived++;
            device.onlineStatus = device.calculateStatus(timestampMs, offlineTimeoutMs_);
            return false;  // Existing device updated
        }
    }

    DeviceRecord newRecord{};
    newRecord.nodeId = nodeId;
    newRecord.firstSeenTimestampMs = timestampMs;
    newRecord.lastSeenTimestampMs = timestampMs;
    newRecord.lastRssiDbm = rssiDbm;
    newRecord.lastSnrDb = snrDb;
    newRecord.lastPacketType = (packetType != nullptr) ? packetType : "UNKNOWN";
    newRecord.packetsReceived = 1;
    newRecord.lastSequenceNumber = sequenceNumber;
    newRecord.onlineStatus = OnlineStatus::online;

    devices_.push_back(newRecord);
    return true;  // New device discovered
}

const DeviceRecord* DeviceRegistry::findNode(std::uint32_t nodeId) const {
    for (const auto& device : devices_) {
        if (device.nodeId == nodeId) {
            return &device;
        }
    }
    return nullptr;
}

bool DeviceRegistry::contains(std::uint32_t nodeId) const {
    return findNode(nodeId) != nullptr;
}

std::size_t DeviceRegistry::deviceCount() const {
    return devices_.size();
}

const std::vector<DeviceRecord>& DeviceRegistry::getAllDevices() const {
    return devices_;
}

void DeviceRegistry::updateStatus(std::uint32_t currentTimestampMs) {
    for (auto& device : devices_) {
        device.onlineStatus = device.calculateStatus(currentTimestampMs, offlineTimeoutMs_);
    }
}

void DeviceRegistry::clear() {
    devices_.clear();
}

}  // namespace jalri::registry
