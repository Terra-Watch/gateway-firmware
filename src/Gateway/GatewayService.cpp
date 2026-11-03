#include "GatewayService.h"

#include <Arduino.h>

namespace terrawatch::gateway {

GatewayService::GatewayService(const protocol::IPacketParser& parser,
                               const protocol::IPacketValidator& validator,
                               logging::ILogger& logger,
                               bool debugMode)
    : parser_(parser),
      validator_(validator),
      logger_(logger),
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

    // A relay's route advertisement (type 5) is relay-to-relay traffic: the gateway does not use it, so it is not an error.
    if (parsed.packetType == 5U) {
        Serial.printf("[RELAY] route advertisement from 0x%02X ignored\n", static_cast<unsigned>(parsed.nodeId));
        return;
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
        if (parsed.destinationId != 0xFEU && parsed.destinationId != 0xFFU) {
            Serial.println("[Gateway] DATA is not addressed to Gateway or broadcast; dropped");
            return;
        }
        isDuplicate = ackHandler_.processDataPacket(
            static_cast<std::uint8_t>(parsed.nodeId),
            parsed.destinationId,
            parsed.previousHopId,
            static_cast<std::uint16_t>(parsed.sequenceNumber),
            packet.rssiDbm,
            packet.snrDb,
            packet.receivedAtMs,
            radioDriver
        );
        if (!isDuplicate) {
            Serial.printf("[DATA RX] source=0x%02X sequence=%lu rssi=%d snr=%.1f payload=",
                          static_cast<unsigned>(parsed.nodeId), static_cast<unsigned long>(parsed.sequenceNumber),
                          packet.rssiDbm, static_cast<double>(packet.snrDb));
            for (std::size_t i = 0; i < parsed.payloadLength; ++i) Serial.printf("%02X", parsed.payload[i]);
            Serial.println();
        }
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

void GatewayService::tick(std::uint32_t currentMs, lora::ILoRaDriver* radioDriver) {
    nodeRegistryService_.tick(currentMs);
    ackHandler_.maybeAdvertiseRoute(currentMs, radioDriver);

    if (currentMs - lastStatsMs_ >= 10000U) {
        lastStatsMs_ = currentMs;
        deviceRegistry_.updateStatus(currentMs);
        Serial.println("================================");
        Serial.println("NODE REGISTRY STATISTICS");
        Serial.printf("Total Nodes: %zu\n", deviceRegistry_.deviceCount());
        for (const auto& d : deviceRegistry_.getAllDevices()) {
            Serial.printf("Node 0x%02X: %s | packets=%lu | RSSI=%d dBm | SNR=%.1f dB\n",
                          static_cast<unsigned>(d.nodeId), registry::toString(d.onlineStatus),
                          static_cast<unsigned long>(d.packetsReceived), d.lastRssiDbm, static_cast<double>(d.lastSnrDb));
        }
        Serial.println("================================");
    }
}

}  // namespace terrawatch::gateway
