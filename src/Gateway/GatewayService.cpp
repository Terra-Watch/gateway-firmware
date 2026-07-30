#include "GatewayService.h"

namespace jalri::gateway {

GatewayService::GatewayService(const protocol::IPacketParser& parser,
                               const protocol::IPacketValidator& validator,
                               logging::ILogger& logger,
                               bool debugMode)
    : parser_(parser),
      validator_(validator),
      logger_(logger),
      debugMode_(debugMode) {}

void GatewayService::handleReceivedPacket(const lora::RawRadioPacket& packet) {
    // 1. Raw packet logged (if debugMode enabled)
    if (debugMode_) {
        const logging::RawRxLogRecord rxLog{
            packet.length,
            packet.rssiDbm,
            packet.snrDb,
            packet.receivedAtMs,
            packet.bytes,
            debugMode_
        };
        logger_.logRawRx(rxLog);
    }

    // 2. Parser
    protocol::ParsedPacketView parsed{};
    const protocol::PacketParseResult parseResult =
        parser_.parse(packet.bytes, packet.length, parsed);

    if (parseResult != protocol::PacketParseResult::success) {
        const logging::RxErrorLogRecord errorLog{
            protocol::toString(parseResult),
            packet.rssiDbm,
            packet.snrDb,
            packet.length
        };
        logger_.logRxError(errorLog);
        return;
    }

    // 3. Validator
    const protocol::PacketValidationResult validationResult =
        validator_.validate(parsed);

    if (validationResult != protocol::PacketValidationResult::success) {
        const logging::RxErrorLogRecord errorLog{
            protocol::toString(validationResult),
            packet.rssiDbm,
            packet.snrDb,
            packet.length
        };
        logger_.logRxError(errorLog);
        return;
    }

    // 4. DeviceRegistry
    const bool isNewDevice = deviceRegistry_.registerPacket(
        parsed.nodeId,
        parsed.packetTypeName,
        parsed.sequenceNumber,
        packet.rssiDbm,
        packet.snrDb,
        packet.receivedAtMs
    );

    const auto* device = deviceRegistry_.findNode(parsed.nodeId);

    // 5. Logger
    if (device != nullptr) {
        const logging::RegistryDeviceLogRecord registryLog{
            parsed.nodeId,
            isNewDevice,
            device->packetsReceived,
            packet.rssiDbm,
            packet.snrDb,
            registry::toString(device->onlineStatus)
        };
        logger_.logDeviceRegistryEvent(registryLog);
    }

    // 6. Periodic 10-second Registry Output
    if (lastTablePrintMs_ == 0 || (packet.receivedAtMs - lastTablePrintMs_ >= 10000U)) {
        lastTablePrintMs_ = packet.receivedAtMs;
        deviceRegistry_.updateStatus(packet.receivedAtMs);

        const auto& devices = deviceRegistry_.getAllDevices();
        std::vector<logging::RegistryTableItem> tableItems;
        tableItems.reserve(devices.size());

        for (const auto& dev : devices) {
            std::uint32_t lastSeenSec = (packet.receivedAtMs >= dev.lastSeenTimestampMs)
                ? ((packet.receivedAtMs - dev.lastSeenTimestampMs) / 1000U)
                : 0U;
            tableItems.push_back({
                dev.nodeId,
                registry::toString(dev.onlineStatus),
                dev.lastRssiDbm,
                dev.lastSnrDb,
                dev.packetsReceived,
                lastSeenSec
            });
        }

        logger_.logRegistryTableSummary(tableItems.data(), tableItems.size());
    }
}

}  // namespace jalri::gateway
