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
                                      std::uint16_t sequenceNumber,
                                      lora::ILoRaDriver* radioDriver) {
    if (radioDriver == nullptr) {
        return;
    }

    std::array<std::uint8_t, 14> ackBuf{};
    ackBuf[0] = 0xA5; // preamble
    ackBuf[1] = 0x03; // version
    ackBuf[2] = 0x03; // PacketType::Acknowledgement (3)
    ackBuf[3] = 0xFE; // Gateway ID (Source)
    ackBuf[4] = targetNodeId; // Destination ID
    ackBuf[5] = static_cast<std::uint8_t>(ackSeq_ & 0xFFU);
    ackBuf[6] = static_cast<std::uint8_t>((ackSeq_ >> 8U) & 0xFFU);
    ackSeq_++;
    ackBuf[7] = 8; // ttl
    ackBuf[8] = 0; // hopCount
    ackBuf[9] = 2; // payloadSize = 2 (kAcknowledgementPayloadSize)

    // Payload: 2 bytes acknowledged sequence number (LSB, MSB)
    ackBuf[10] = static_cast<std::uint8_t>(sequenceNumber & 0xFFU);
    ackBuf[11] = static_cast<std::uint8_t>((sequenceNumber >> 8U) & 0xFFU);

    const std::uint16_t crc = computeCrc16_(ackBuf.data(), 12);
    ackBuf[12] = static_cast<std::uint8_t>(crc & 0xFFU);
    ackBuf[13] = static_cast<std::uint8_t>((crc >> 8U) & 0xFFU);

    radioDriver->send(ackBuf.data(), ackBuf.size());
}

bool GatewayAckHandler::processHeartbeat(std::uint32_t nodeId,
                                         std::uint16_t sequenceNumber,
                                         int rssiDbm,
                                         float snrDb,
                                         std::uint32_t timestampMs,
                                         lora::ILoRaDriver* radioDriver) {
    stats_.lastPacketId = sequenceNumber;
    stats_.rssi = rssiDbm;
    stats_.snr = snrDb;
    stats_.lastSeenMs = timestampMs;

    auto it = lastPacketIdMap_.find(nodeId);
    if (it != lastPacketIdMap_.end() && it->second == sequenceNumber && sequenceNumber != 0) {
        stats_.duplicatePackets++;
        Serial.printf("--------------------------------\n");
        Serial.printf("Duplicate heartbeat (#%u)\n", static_cast<unsigned>(sequenceNumber));
        Serial.printf("Node: 0x%02X | RSSI: %d dBm | SNR: %.1f dB\n", static_cast<unsigned>(nodeId), rssiDbm, static_cast<double>(snrDb));
        Serial.printf("ACK resent\n");
        Serial.printf("--------------------------------\n");
        Serial.flush();

        sendAckPacket_(static_cast<std::uint8_t>(nodeId), sequenceNumber, radioDriver);
        return true;
    }

    lastPacketIdMap_[nodeId] = sequenceNumber;
    stats_.packetsReceived++;
    stats_.ackSent++;

    Serial.printf("--------------------------------\n");
    Serial.printf("Heartbeat received\n");
    Serial.printf("Node: 0x%02X | Sequence: %u | RSSI: %d dBm | SNR: %.1f dB\n",
                  static_cast<unsigned>(nodeId), static_cast<unsigned>(sequenceNumber), rssiDbm, static_cast<double>(snrDb));

    sendAckPacket_(static_cast<std::uint8_t>(nodeId), sequenceNumber, radioDriver);
    Serial.printf("ACK sent\n");
    Serial.printf("--------------------------------\n");
    Serial.flush();

    return false;
}

}  // namespace jalri::gateway
