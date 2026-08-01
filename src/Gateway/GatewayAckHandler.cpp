#include "GatewayAckHandler.h"
#include <Arduino.h>
#include <cstdio>
#include <array>

namespace jalri::gateway {

std::uint16_t GatewayAckHandler::computeCrc16_(const std::uint8_t* data, std::size_t length) {
    std::uint16_t crc = 0xFFFFU;
    for (std::size_t i = 0; i < length; ++i) {
        crc ^= static_cast<std::uint16_t>(data[i]) << 8U;
        for (std::uint8_t bit = 0; bit < 8; ++bit) {
            if ((crc & 0x8000U) != 0) {
                crc = static_cast<std::uint16_t>((crc << 1U) ^ 0x1021U);
            } else {
                crc = static_cast<std::uint16_t>(crc << 1U);
            }
        }
    }
    return crc;
}

void GatewayAckHandler::sendAckPacket_(std::uint8_t targetNodeId,
                                      std::uint32_t packetId,
                                      std::uint32_t timestampMs,
                                      lora::ILoRaDriver* radioDriver) {
    if (radioDriver == nullptr) {
        return;
    }

    std::array<std::uint8_t, 21> ackBuf{};
    ackBuf[0] = 0xA5; // preamble
    ackBuf[1] = 0x03; // version
    ackBuf[2] = 0x04; // HeartbeatAck
    ackBuf[3] = 0xFE; // Gateway ID
    ackBuf[4] = targetNodeId;
    ackBuf[5] = static_cast<std::uint8_t>(ackSeq_ & 0xFFU);
    ackBuf[6] = static_cast<std::uint8_t>((ackSeq_ >> 8U) & 0xFFU);
    ackSeq_++;
    ackBuf[7] = 8; // ttl
    ackBuf[8] = 0; // hopCount
    ackBuf[9] = 9; // payloadSize

    // Payload: packetId (4 bytes), nodeId (1 byte), timestamp (4 bytes)
    ackBuf[10] = static_cast<std::uint8_t>(packetId & 0xFFU);
    ackBuf[11] = static_cast<std::uint8_t>((packetId >> 8U) & 0xFFU);
    ackBuf[12] = static_cast<std::uint8_t>((packetId >> 16U) & 0xFFU);
    ackBuf[13] = static_cast<std::uint8_t>((packetId >> 24U) & 0xFFU);

    ackBuf[14] = targetNodeId;

    ackBuf[15] = static_cast<std::uint8_t>(timestampMs & 0xFFU);
    ackBuf[16] = static_cast<std::uint8_t>((timestampMs >> 8U) & 0xFFU);
    ackBuf[17] = static_cast<std::uint8_t>((timestampMs >> 16U) & 0xFFU);
    ackBuf[18] = static_cast<std::uint8_t>((timestampMs >> 24U) & 0xFFU);

    const std::uint16_t crc = computeCrc16_(ackBuf.data(), 19);
    ackBuf[19] = static_cast<std::uint8_t>(crc & 0xFFU);
    ackBuf[20] = static_cast<std::uint8_t>((crc >> 8U) & 0xFFU);

    radioDriver->send(ackBuf.data(), ackBuf.size());
}

bool GatewayAckHandler::processHeartbeat(std::uint32_t nodeId,
                                         std::uint32_t packetId,
                                         int rssiDbm,
                                         float snrDb,
                                         std::uint32_t timestampMs,
                                         lora::ILoRaDriver* radioDriver) {
    stats_.lastPacketId = packetId;
    stats_.rssi = rssiDbm;
    stats_.snr = snrDb;
    stats_.lastSeenMs = timestampMs;

    auto it = lastPacketIdMap_.find(nodeId);
    if (it != lastPacketIdMap_.end() && it->second == packetId && packetId != 0) {
        stats_.duplicatePackets++;
        Serial.printf("--------------------------------\n");
        Serial.printf("Duplicate heartbeat (#%lu)\n", static_cast<unsigned long>(packetId));
        Serial.printf("Node: 0x%02X | RSSI: %d dBm | SNR: %.1f dB\n", static_cast<unsigned>(nodeId), rssiDbm, static_cast<double>(snrDb));
        Serial.printf("ACK resent\n");
        Serial.printf("--------------------------------\n");
        Serial.flush();

        sendAckPacket_(static_cast<std::uint8_t>(nodeId), packetId, timestampMs, radioDriver);
        return true;
    }

    lastPacketIdMap_[nodeId] = packetId;
    stats_.packetsReceived++;
    stats_.ackSent++;

    Serial.printf("--------------------------------\n");
    Serial.printf("Heartbeat received\n");
    Serial.printf("Node: 0x%02X | Packet ID: %lu | RSSI: %d dBm | SNR: %.1f dB\n",
                  static_cast<unsigned>(nodeId), static_cast<unsigned long>(packetId), rssiDbm, static_cast<double>(snrDb));

    sendAckPacket_(static_cast<std::uint8_t>(nodeId), packetId, timestampMs, radioDriver);
    Serial.printf("ACK sent\n");
    Serial.printf("--------------------------------\n");
    Serial.flush();

    return false;
}

void GatewayAckHandler::sendDataAckPacket_(std::uint8_t targetNodeId,
                                           std::uint16_t sequenceNumber,
                                           lora::ILoRaDriver* radioDriver) {
    if (radioDriver == nullptr) {
        return;
    }

    std::array<std::uint8_t, 14> ackBuf{};
    ackBuf[0] = 0xA5; // preamble
    ackBuf[1] = 0x03; // version
    ackBuf[2] = 0x03; // PacketType::Acknowledgement
    ackBuf[3] = 0xFE; // Gateway ID
    ackBuf[4] = targetNodeId;
    ackBuf[5] = static_cast<std::uint8_t>(ackSeq_ & 0xFFU);
    ackBuf[6] = static_cast<std::uint8_t>((ackSeq_ >> 8U) & 0xFFU);
    ackSeq_++;
    ackBuf[7] = 8; // ttl
    ackBuf[8] = 0; // hopCount
    ackBuf[9] = 2; // payloadSize (16-bit acknowledged sequence number)

    ackBuf[10] = static_cast<std::uint8_t>(sequenceNumber & 0xFFU);
    ackBuf[11] = static_cast<std::uint8_t>((sequenceNumber >> 8U) & 0xFFU);

    const std::uint16_t crc = computeCrc16_(ackBuf.data(), 12);
    ackBuf[12] = static_cast<std::uint8_t>(crc & 0xFFU);
    ackBuf[13] = static_cast<std::uint8_t>((crc >> 8U) & 0xFFU);

    radioDriver->send(ackBuf.data(), ackBuf.size());
}

bool GatewayAckHandler::processDataPacket(std::uint8_t targetNodeId,
                                          std::uint16_t sequenceNumber,
                                          int rssiDbm,
                                          float snrDb,
                                          std::uint32_t timestampMs,
                                          lora::ILoRaDriver* radioDriver) {
    const DuplicateKey key{targetNodeId, 1U /* PacketType::Data */, sequenceNumber};

    auto it = dataDuplicateMap_.find(key);
    if (it != dataDuplicateMap_.end()) {
        stats_.duplicatePackets++;
        Serial.printf("--------------------------------\n");
        Serial.printf("Duplicate Data Packet (Seq #%u)\n", sequenceNumber);
        Serial.printf("Node: 0x%02X | RSSI: %d dBm | SNR: %.1f dB\n", targetNodeId, rssiDbm, static_cast<double>(snrDb));
        Serial.printf("Data ACK resent\n");
        Serial.printf("--------------------------------\n");
        Serial.flush();

        sendDataAckPacket_(targetNodeId, sequenceNumber, radioDriver);
        return true;
    }

    dataDuplicateMap_[key] = timestampMs;
    stats_.packetsReceived++;
    stats_.ackSent++;

    Serial.printf("--------------------------------\n");
    Serial.printf("Data Packet received\n");
    Serial.printf("Node: 0x%02X | Seq: %u | RSSI: %d dBm | SNR: %.1f dB\n",
                  targetNodeId, sequenceNumber, rssiDbm, static_cast<double>(snrDb));

    sendDataAckPacket_(targetNodeId, sequenceNumber, radioDriver);
    Serial.printf("Data ACK sent\n");
    Serial.printf("--------------------------------\n");
    Serial.flush();

    return false;
}

}  // namespace jalri::gateway
