#include "GatewayService.h"

#include <Arduino.h>

namespace jalri::gateway {

GatewayService::GatewayService(const protocol::IPacketParser& parser,
                               const protocol::IPacketValidator& validator,
                               logging::ILogger& logger,
                               backend::BackendIngestionClient& backendClient,
                               bool debugMode)
    : parser_(parser),
      validator_(validator),
      logger_(logger),
      backendClient_(backendClient),
      debugMode_(debugMode) {}

void GatewayService::handleReceivedPacket(const lora::RawRadioPacket& packet, lora::ILoRaDriver* radioDriver) {
    Serial.println("--------------------------------");
    Serial.println("Packet Received");
    Serial.println();
    Serial.print("Length: ");
    Serial.println(static_cast<unsigned>(packet.length));
    Serial.println();
    Serial.print("RSSI: ");
    Serial.println(packet.rssiDbm);
    Serial.println();
    Serial.print("SNR: ");
    Serial.println(static_cast<double>(packet.snrDb), 1);
    Serial.println();
    Serial.print("Timestamp: ");
    Serial.println(static_cast<unsigned long>(packet.receivedAtMs));
    Serial.println();
    Serial.println("--------------------------------");
    if (Serial) {
        Serial.flush();
    }

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
        Serial.println("[Parser] FAILED");
        Serial.println();
        if (Serial) {
            Serial.flush();
        }

        const logging::RxErrorLogRecord errorLog{
            protocol::toString(parseResult),
            packet.rssiDbm,
            packet.snrDb,
            packet.length
        };
        logger_.logRxError(errorLog);
        return;
    }
    Serial.println("[Parser] SUCCESS");
    Serial.println();
    if (Serial) {
        Serial.flush();
    }

    // 3. Validator
    const protocol::PacketValidationResult validationResult =
        validator_.validate(parsed);

    if (validationResult != protocol::PacketValidationResult::success) {
        Serial.println("[Validator] FAILED");
        Serial.println();
        if (Serial) {
            Serial.flush();
        }

        const logging::RxErrorLogRecord errorLog{
            protocol::toString(validationResult),
            packet.rssiDbm,
            packet.snrDb,
            packet.length
        };
        logger_.logRxError(errorLog);
        return;
    }
    Serial.println("[Validator] SUCCESS");
    Serial.println();
    if (Serial) {
        Serial.flush();
    }

    // 4. Immediate ACK Transmission & Duplicate Detection
    bool isDuplicate = false;
    if (parsed.packetTypeName != nullptr && strcmp(parsed.packetTypeName, "DATA") == 0) {
        isDuplicate = ackHandler_.processDataPacket(
            static_cast<std::uint8_t>(parsed.nodeId),
            static_cast<std::uint16_t>(parsed.sequenceNumber),
            packet.rssiDbm,
            packet.snrDb,
            packet.receivedAtMs,
            radioDriver
        );
    } else {
        if (parsed.packetType != 2U) {
            Serial.println("[Gateway] Unsupported non-heartbeat packet type");
            return;
        }
        Serial.printf("[HEARTBEAT RX] source: 0x%02X | destination: 0x%02X | packet ID: %lu | sequence: %lu | RSSI: %d | SNR: %.1f\n",
                      static_cast<unsigned>(parsed.nodeId), static_cast<unsigned>(parsed.destinationId),
                      static_cast<unsigned long>(parsed.packetId), static_cast<unsigned long>(parsed.sequenceNumber),
                      packet.rssiDbm, static_cast<double>(packet.snrDb));
        isDuplicate = ackHandler_.processHeartbeat(
            parsed.nodeId,
            parsed.packetId,
            packet.rssiDbm,
            packet.snrDb,
            packet.receivedAtMs,
            radioDriver
        );

        nodeRegistryService_.handleHeartbeat(
            parsed.nodeId,
            parsed.packetId,
            packet.rssiDbm,
            packet.snrDb,
            packet.receivedAtMs
        );

        // ACK is sent by processHeartbeat above before this backend queue operation.
        backendClient_.enqueueHeartbeat(parsed, packet);
    }

    if (isDuplicate) {
        return; // ACK resent, skip duplicate processing
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
}

void GatewayService::beginBackend() {
    backendClient_.begin();
}

void GatewayService::tick(std::uint32_t currentMs) {
    nodeRegistryService_.tick(currentMs);
    backendClient_.tick(currentMs);
}

}  // namespace jalri::gateway
