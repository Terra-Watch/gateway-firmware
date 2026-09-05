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

bool GatewayAckHandler::sendAckPacket_(std::uint8_t targetNodeId,
                                      std::uint32_t packetId,
                                      std::uint32_t timestampMs,
                                      lora::ILoRaDriver* radioDriver) {
    if (radioDriver == nullptr) {
        return false;
    }

    std::array<std::uint8_t, 22> ackBuf{};
    if (!buildHeartbeatAckPacket(targetNodeId, packetId, timestampMs, ackBuf)) return false;

    const bool sent = radioDriver->send(ackBuf.data(), ackBuf.size());
    if (!sent) stats_.ackTxFailures++;
    return sent;
}

bool GatewayAckHandler::buildHeartbeatAckPacket(std::uint8_t targetNodeId,
                                                std::uint32_t packetId,
                                                std::uint32_t timestampMs,
                                                std::array<std::uint8_t, 22>& ackBuf) {
    ackBuf.fill(0);
    ackBuf[0] = 0xA5; // preamble
    ackBuf[1] = 0x04; // version
    ackBuf[2] = 0x04; // PacketType::HeartbeatAck (4)
    ackBuf[3] = 0xFE; // Gateway ID (Source)
    ackBuf[4] = targetNodeId; // Destination ID
    ackBuf[5] = 0xFE; // Previous hop: gateway
    ackBuf[6] = static_cast<std::uint8_t>(ackSeq_ & 0xFFU);
    ackBuf[7] = static_cast<std::uint8_t>((ackSeq_ >> 8U) & 0xFFU);
    ackSeq_++;
    ackBuf[8] = 8; // ttl
    ackBuf[9] = 0; // hopCount
    ackBuf[10] = 9; // packet ID (4), target node ID (1), gateway timestamp (4)

    ackBuf[11] = static_cast<std::uint8_t>(packetId & 0xFFU);
    ackBuf[12] = static_cast<std::uint8_t>((packetId >> 8U) & 0xFFU);
    ackBuf[13] = static_cast<std::uint8_t>((packetId >> 16U) & 0xFFU);
    ackBuf[14] = static_cast<std::uint8_t>((packetId >> 24U) & 0xFFU);
    ackBuf[15] = targetNodeId;
    ackBuf[16] = static_cast<std::uint8_t>(timestampMs & 0xFFU);
    ackBuf[17] = static_cast<std::uint8_t>((timestampMs >> 8U) & 0xFFU);
    ackBuf[18] = static_cast<std::uint8_t>((timestampMs >> 16U) & 0xFFU);
    ackBuf[19] = static_cast<std::uint8_t>((timestampMs >> 24U) & 0xFFU);

    const std::uint16_t crc = computeCrc16_(ackBuf.data(), 20);
    ackBuf[20] = static_cast<std::uint8_t>(crc & 0xFFU);
    ackBuf[21] = static_cast<std::uint8_t>((crc >> 8U) & 0xFFU);

    return true;
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

    bool duplicate = false;
    for (auto& entry : heartbeatDuplicates_) {
        if (!entry.active) continue;
        if (timestampMs - entry.timestampMs >= duplicateExpiryMs_) { entry.active = false; continue; }
        if (entry.nodeId == nodeId && entry.packetId == packetId && packetId != 0) duplicate = true;
    }
    if (duplicate) {
        stats_.duplicatePackets++;
        Serial.printf("--------------------------------\n");
        Serial.printf("Duplicate heartbeat (#%lu)\n", static_cast<unsigned long>(packetId));
        Serial.printf("Node: 0x%02X | RSSI: %d dBm | SNR: %.1f dB\n", static_cast<unsigned>(nodeId), rssiDbm, static_cast<double>(snrDb));
        Serial.printf("ACK resent\n");
        Serial.printf("--------------------------------\n");
        Serial.flush();

        if (sendAckPacket_(static_cast<std::uint8_t>(nodeId), packetId, timestampMs, radioDriver)) stats_.ackSent++;
        return true;
    }

    auto& heartbeatEntry = heartbeatDuplicates_[nextHeartbeatDuplicate_];
    heartbeatEntry.nodeId = nodeId;
    heartbeatEntry.packetId = packetId;
    heartbeatEntry.timestampMs = timestampMs;
    heartbeatEntry.active = true;
    nextHeartbeatDuplicate_ = (nextHeartbeatDuplicate_ + 1U) % heartbeatDuplicates_.size();
    stats_.packetsReceived++;

    Serial.printf("--------------------------------\n");
    Serial.printf("Heartbeat received\n");
    Serial.printf("Node: 0x%02X | Packet ID: %lu | RSSI: %d dBm | SNR: %.1f dB\n",
                  static_cast<unsigned>(nodeId), static_cast<unsigned long>(packetId), rssiDbm, static_cast<double>(snrDb));

    if (sendAckPacket_(static_cast<std::uint8_t>(nodeId), packetId, timestampMs, radioDriver)) stats_.ackSent++;
    Serial.printf("[HEARTBEAT ACK TX] type: 4 | source: 0xFE | target: 0x%02X | ack packet ID: %lu | size: 22\n",
                  static_cast<unsigned>(nodeId), static_cast<unsigned long>(packetId));
    Serial.printf("--------------------------------\n");
    Serial.flush();

    return false;
}

bool GatewayAckHandler::sendDataAckPacket_(std::uint8_t targetNodeId,
                                           std::uint16_t sequenceNumber,
                                           lora::ILoRaDriver* radioDriver) {
    if (radioDriver == nullptr) {
        return false;
    }

    std::array<std::uint8_t, 15> ackBuf{};
    ackBuf[0] = 0xA5; // preamble
    ackBuf[1] = 0x04; // version
    ackBuf[2] = 0x03; // PacketType::Acknowledgement
    ackBuf[3] = 0xFE; // Gateway ID
    ackBuf[4] = targetNodeId;
    ackBuf[5] = 0xFE; // Previous hop: gateway
    ackBuf[6] = static_cast<std::uint8_t>(ackSeq_ & 0xFFU);
    ackBuf[7] = static_cast<std::uint8_t>((ackSeq_ >> 8U) & 0xFFU);
    ackSeq_++;
    ackBuf[8] = 8; // ttl
    ackBuf[9] = 0; // hopCount
    ackBuf[10] = 2; // payloadSize (16-bit acknowledged sequence number)

    ackBuf[11] = static_cast<std::uint8_t>(sequenceNumber & 0xFFU);
    ackBuf[12] = static_cast<std::uint8_t>((sequenceNumber >> 8U) & 0xFFU);

    const std::uint16_t crc = computeCrc16_(ackBuf.data(), 13);
    ackBuf[13] = static_cast<std::uint8_t>(crc & 0xFFU);
    ackBuf[14] = static_cast<std::uint8_t>((crc >> 8U) & 0xFFU);

    const bool sent = radioDriver->send(ackBuf.data(), ackBuf.size());
    if (!sent) stats_.ackTxFailures++;
    return sent;
}

bool GatewayAckHandler::processDataPacket(std::uint8_t targetNodeId,
                                          std::uint8_t destinationId,
                                          std::uint8_t previousHopId,
                                          std::uint16_t sequenceNumber,
                                          int rssiDbm,
                                          float snrDb,
                                          std::uint32_t timestampMs,
                                          lora::ILoRaDriver* radioDriver) {
    const DuplicateKey key{targetNodeId, 1U /* PacketType::Data */, sequenceNumber};
    bool duplicate = false;
    for (auto& entry : dataDuplicates_) {
        if (!entry.active) continue;
        if (timestampMs - entry.timestampMs >= duplicateExpiryMs_) { entry.active = false; continue; }
        if (entry.key.sourceId == key.sourceId && entry.key.type == key.type && entry.key.sequenceNumber == key.sequenceNumber) duplicate = true;
    }
    if (duplicate) {
        stats_.duplicatePackets++;
        Serial.printf("--------------------------------\n");
        Serial.printf("Duplicate Data Packet (Seq #%u)\n", sequenceNumber);
        Serial.printf("Node: 0x%02X | RSSI: %d dBm | SNR: %.1f dB\n", targetNodeId, rssiDbm, static_cast<double>(snrDb));
        Serial.printf("Data ACK resent\n");
        Serial.printf("--------------------------------\n");
        Serial.flush();

        if (destinationId == 0xFEU && sendDataAckPacket_(previousHopId, sequenceNumber, radioDriver)) stats_.ackSent++;
        return true;
    }

    auto& dataEntry = dataDuplicates_[nextDataDuplicate_];
    dataEntry.key = key;
    dataEntry.timestampMs = timestampMs;
    dataEntry.active = true;
    nextDataDuplicate_ = (nextDataDuplicate_ + 1U) % dataDuplicates_.size();
    stats_.packetsReceived++;

    Serial.printf("--------------------------------\n");
    Serial.printf("Data Packet received\n");
    Serial.printf("Node: 0x%02X | Seq: %u | RSSI: %d dBm | SNR: %.1f dB\n",
                  targetNodeId, sequenceNumber, rssiDbm, static_cast<double>(snrDb));

    if (destinationId == 0xFEU && sendDataAckPacket_(previousHopId, sequenceNumber, radioDriver)) stats_.ackSent++;
    Serial.printf("Data ACK sent\n");
    Serial.printf("--------------------------------\n");
    Serial.flush();

    return false;
}

}  // namespace jalri::gateway
