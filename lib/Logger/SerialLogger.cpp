#include "SerialLogger.h"

#include <Arduino.h>
#include <cstdio>

namespace jalri::logging {

void SerialLogger::log(LogLevel level, const char* message) {
    const char* prefix = "[INFO]";

    switch (level) {
        case LogLevel::info:
            prefix = "[INFO]";
            break;
        case LogLevel::warning:
            prefix = "[WARN]";
            break;
        case LogLevel::error:
            prefix = "[ERROR]";
            break;
    }

    Serial.printf("%s %s\n", prefix, message);
    Serial.flush();
}

void SerialLogger::logRawRx(const RawRxLogRecord& record) {
    Serial.printf("--------------------------------\n");
    Serial.printf("[LoRa RX]\n");
    Serial.printf("Length: %u bytes\n", static_cast<unsigned>(record.length));
    Serial.printf("RSSI: %d dBm\n", record.rssiDbm);
    Serial.printf("SNR: %.2f dB\n", static_cast<double>(record.snrDb));
    Serial.printf("Timestamp: %lu ms\n", static_cast<unsigned long>(record.timestampMs));

    if (record.debugHex && record.payload != nullptr && record.length > 0) {
        Serial.printf("Payload (HEX): ");
        for (std::size_t i = 0; i < record.length; ++i) {
            Serial.printf("%02X%s", record.payload[i], (i + 1 < record.length) ? " " : "");
        }
        Serial.printf("\n");
    }

    Serial.printf("--------------------------------\n");
    Serial.flush();
}

void SerialLogger::logDeviceRegistryEvent(const RegistryDeviceLogRecord& record) {
    Serial.printf("--------------------------------\n\n");
    if (record.isNewDevice) {
        Serial.printf("BoatNode Connected\n\n");
        Serial.printf("Node: 0x%04X\n\n", static_cast<unsigned>(record.nodeId));
        Serial.printf("RSSI: %d\n\n", record.rssiDbm);
        Serial.printf("SNR: %.1f\n\n", static_cast<double>(record.snrDb));
    } else {
        Serial.printf("BoatNode Updated\n\n");
        Serial.printf("Node: 0x%04X\n\n", static_cast<unsigned>(record.nodeId));
        Serial.printf("Packets Received: %lu\n\n", static_cast<unsigned long>(record.packetsReceived));
        Serial.printf("RSSI: %d\n\n", record.rssiDbm);
        Serial.printf("SNR: %.1f\n\n", static_cast<double>(record.snrDb));
        Serial.printf("Status: %s\n\n", record.statusStr);
    }
    Serial.printf("--------------------------------\n");
    Serial.flush();
}

void SerialLogger::logRegistryTableSummary(const RegistryTableItem* items, std::size_t count) {
    Serial.printf("----------------------------------------------------------\n\n");
    Serial.printf("%-10s %-10s %-8s %-7s %-11s %-10s\n\n", "Node", "Status", "RSSI", "SNR", "Packets", "Last Seen");
    for (std::size_t i = 0; i < count; ++i) {
        char lastSeenBuf[32];
        std::snprintf(lastSeenBuf, sizeof(lastSeenBuf), "%lu sec", static_cast<unsigned long>(items[i].lastSeenSec));

        char nodeHexBuf[16];
        std::snprintf(nodeHexBuf, sizeof(nodeHexBuf), "0x%04X", static_cast<unsigned>(items[i].nodeId));

        Serial.printf("%-10s %-10s %-8d %-7.1f %-11lu %-10s\n\n",
                      nodeHexBuf,
                      items[i].statusStr,
                      items[i].rssiDbm,
                      static_cast<double>(items[i].snrDb),
                      static_cast<unsigned long>(items[i].packetsReceived),
                      lastSeenBuf);
    }
    Serial.printf("----------------------------------------------------------\n");
    Serial.flush();
}

void SerialLogger::logRxHeartbeat(const RxLogRecord& record) {
    Serial.printf("[RX]\n");
    Serial.printf("Timestamp: %lu\n", static_cast<unsigned long>(record.timestampMs));
    Serial.printf("Node ID: %lu\n", static_cast<unsigned long>(record.nodeId));
    Serial.printf("Packet Type: %s\n", record.packetType);
    Serial.printf("Protocol Version: %u\n", static_cast<unsigned>(record.protocolVersion));
    Serial.printf("Sequence Number: %lu\n", static_cast<unsigned long>(record.sequenceNumber));
    Serial.printf("Hop Count: %u\n", static_cast<unsigned>(record.hopCount));
    Serial.printf("TTL: %u\n", static_cast<unsigned>(record.ttl));
    Serial.printf("RSSI: %d\n", record.rssiDbm);
    Serial.printf("SNR: %.2f\n", static_cast<double>(record.snrDb));
    Serial.printf("Payload Length: %u\n", static_cast<unsigned>(record.payloadLength));
    Serial.flush();
}

void SerialLogger::logRxError(const RxErrorLogRecord& record) {
    Serial.printf("[RX ERROR]\n");
    Serial.printf("Reason: %s\n", record.reason);
    Serial.printf("RSSI: %d\n", record.rssiDbm);
    Serial.printf("SNR: %.2f\n", static_cast<double>(record.snrDb));
    Serial.printf("Length: %u\n", static_cast<unsigned>(record.length));
    Serial.flush();
}

}  // namespace jalri::logging
