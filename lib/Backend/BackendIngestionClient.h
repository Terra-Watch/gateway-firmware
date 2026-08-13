#pragma once

#include <cstddef>
#include <cstdint>

#include "LoRaTypes.h"
#include "ProtocolIntegrationTypes.h"

namespace jalri::backend {

struct BackendConfig {
    const char* wifiSsid;
    const char* wifiPassword;
    const char* baseUrl;
    std::uint8_t gatewayId;
};

struct HeartbeatIngestionEvent {
    std::uint32_t nodeId;
    std::uint32_t packetId;
    std::uint8_t destination;
    std::uint32_t sequence;
    std::uint8_t packetSize;
    std::uint32_t receivedAtMs;
    int rssi;
    float snr;
};

class BackendIngestionClient final {
public:
    explicit BackendIngestionClient(const BackendConfig& config);

    void begin();
    void tick(std::uint32_t currentMs);
    bool enqueueHeartbeat(const protocol::ParsedPacketView& packet,
                          const lora::RawRadioPacket& radioPacket);

private:
    static constexpr std::size_t queueCapacity = 8U;
    static constexpr std::uint8_t maxAttempts = 2U;
    static constexpr std::uint32_t retryDelayMs = 250U;
    static constexpr std::uint32_t requestTimeoutMs = 1500U;

    struct QueueItem {
        HeartbeatIngestionEvent event{};
        std::uint8_t attempts{0};
        std::uint32_t nextAttemptMs{0};
        bool occupied{false};
    };

    BackendConfig config_;
    QueueItem queue_[queueCapacity]{};
    bool wifiStarted_{false};
    std::uint32_t lastWifiAttemptMs_{0};
    bool postHeartbeat_(QueueItem& item);
    bool configured_() const;
    void maintainWifi_(std::uint32_t currentMs);
};

}  // namespace jalri::backend
