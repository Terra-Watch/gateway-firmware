#pragma once

#include "ILogger.h"

namespace jalri::logging {

class SerialLogger final : public ILogger {
public:
    void log(LogLevel level, const char* message) override;
    void logRawRx(const RawRxLogRecord& record) override;
    void logDeviceRegistryEvent(const RegistryDeviceLogRecord& record) override;
    void logRegistryTableSummary(const RegistryTableItem* items, std::size_t count) override;
    void logRxHeartbeat(const RxLogRecord& record) override;
    void logRxError(const RxErrorLogRecord& record) override;
};

}  // namespace jalri::logging
