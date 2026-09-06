#pragma once

#include <cstddef>
#include <cstdint>

#include "LoRaTypes.h"
#include "ProtocolIntegrationTypes.h"

namespace jalri::lora { class ILoRaDriver; }

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

struct TelemetryIngestionEvent {
    std::uint32_t nodeId;
    std::uint32_t sequence;
    std::uint8_t packetSize;
    std::uint32_t receivedAtMs;
    int rssi;
    float snr;
    bool rtcValid;
    std::uint16_t year;
    std::uint8_t month;
    std::uint8_t day;
    std::uint8_t hour;
    std::uint8_t minute;
    std::uint8_t second;
    std::int16_t temperatureCentiC;
};

class BackendIngestionClient final {
public:
    explicit BackendIngestionClient(const BackendConfig& config);

    void begin();
    void tick(std::uint32_t currentMs);
    bool enqueueHeartbeat(const protocol::ParsedPacketView& packet,
                          const lora::RawRadioPacket& radioPacket);
    bool enqueueTelemetry(const protocol::ParsedPacketView& packet,
                          const lora::RawRadioPacket& radioPacket);
    void dispatchMessages(lora::ILoRaDriver& radioDriver);
    void reportCrewMessageReceipt(const protocol::ParsedPacketView& packet);

    void runNetworkDiagnostics();

private:
    static constexpr std::size_t queueCapacity = 8U;
    static constexpr std::uint8_t maxAttempts = 2U;
    static constexpr std::uint32_t retryDelayMs = 500U;
    static constexpr std::uint32_t requestTimeoutMs = 10000U;

    struct QueueItem {
        HeartbeatIngestionEvent event{};
        TelemetryIngestionEvent telemetry{};
        bool telemetryEvent{false};
        std::uint8_t attempts{0};
        std::uint32_t nextAttemptMs{0};
        bool occupied{false};
    };

    BackendConfig config_;
    QueueItem queue_[queueCapacity]{};
    bool wifiStarted_{false};
    bool diagnosticsRun_{false};
    std::uint32_t lastWifiAttemptMs_{0};
    std::uint16_t outboundSequence_{0};

    bool postHeartbeat_(QueueItem& item);
    bool postTelemetry_(QueueItem& item);
    bool configured_() const;
    void maintainWifi_(std::uint32_t currentMs);
    std::size_t getQueueDepth_() const;
    void extractHostname_(char* hostBuf, std::size_t maxLen) const;
    bool postReceipt_(const char* clientEventId, std::uint8_t nodeId, std::uint16_t sequence, bool boatNodeReceived);
};

}  // namespace jalri::backend
