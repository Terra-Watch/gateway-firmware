#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace jalri::registry {

enum class OnlineStatus {
    unknown,
    online,
    offline
};

const char* toString(OnlineStatus status);

struct DeviceRecord {
    std::uint32_t nodeId{0};
    std::uint32_t firstSeenTimestampMs{0};
    std::uint32_t lastSeenTimestampMs{0};
    int lastRssiDbm{0};
    float lastSnrDb{0.0f};
    const char* lastPacketType{"UNKNOWN"};
    std::uint32_t packetsReceived{0};
    std::uint32_t lastSequenceNumber{0};
    OnlineStatus onlineStatus{OnlineStatus::unknown};

    OnlineStatus calculateStatus(std::uint32_t currentTimestampMs, std::uint32_t timeoutMs = 30000U) const;
};

class DeviceRegistry final {
public:
    explicit DeviceRegistry(std::uint32_t offlineTimeoutMs = 30000U);

    bool registerPacket(std::uint32_t nodeId,
                        const char* packetType,
                        std::uint32_t sequenceNumber,
                        int rssiDbm,
                        float snrDb,
                        std::uint32_t timestampMs);

    const DeviceRecord* findNode(std::uint32_t nodeId) const;
    bool contains(std::uint32_t nodeId) const;
    std::size_t deviceCount() const;
    const std::vector<DeviceRecord>& getAllDevices() const;

    void updateStatus(std::uint32_t currentTimestampMs);

    std::uint32_t offlineTimeoutMs() const { return offlineTimeoutMs_; }
    void setOfflineTimeoutMs(std::uint32_t timeoutMs) { offlineTimeoutMs_ = timeoutMs; }
    void clear();

private:
    std::vector<DeviceRecord> devices_;
    std::uint32_t offlineTimeoutMs_;
};

}  // namespace jalri::registry
