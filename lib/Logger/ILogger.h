#pragma once

#include <cstddef>
#include <cstdint>

namespace jalri::logging {

enum class LogLevel {
    info,
    warning,
    error,
};

struct RawRxLogRecord {
    std::size_t length;
    int rssiDbm;
    float snrDb;
    std::uint32_t timestampMs;
    const std::uint8_t* payload;
    bool debugHex;
};

struct RxLogRecord {
    std::uint32_t timestampMs;
    std::uint32_t nodeId;
    const char* packetType;
    std::uint8_t protocolVersion;
    std::uint32_t sequenceNumber;
    std::uint8_t hopCount;
    std::uint8_t ttl;
    int rssiDbm;
    float snrDb;
    std::size_t payloadLength;
};

struct RxErrorLogRecord {
    const char* reason;
    int rssiDbm;
    float snrDb;
    std::size_t length;
};

struct RegistryDeviceLogRecord {
    std::uint32_t nodeId;
    bool isNewDevice;
    std::uint32_t packetsReceived;
    int rssiDbm;
    float snrDb;
    const char* statusStr;
};

struct RegistryTableItem {
    std::uint32_t nodeId;
    const char* statusStr;
    int rssiDbm;
    float snrDb;
    std::uint32_t packetsReceived;
    std::uint32_t lastSeenSec;
};

class ILogger {
public:
    virtual ~ILogger() = default;

    virtual void log(LogLevel level, const char* message) = 0;
    virtual void logRawRx(const RawRxLogRecord& record) = 0;
    virtual void logDeviceRegistryEvent(const RegistryDeviceLogRecord& record) = 0;
    virtual void logRegistryTableSummary(const RegistryTableItem* items, std::size_t count) = 0;
    virtual void logRxHeartbeat(const RxLogRecord& record) = 0;
    virtual void logRxError(const RxErrorLogRecord& record) = 0;
};

}  // namespace jalri::logging
