#include "Sx1278LoRaDriver.h"
#include "BuildConfig.h"

#include <Arduino.h>
#include <LoRa.h>
#include <SPI.h>

namespace jalri::lora {

namespace {

constexpr int loraSck = 12;
constexpr int loraMiso = 13;
constexpr int loraMosi = 11;
constexpr int loraCs = 10;
constexpr int loraReset = 9;
constexpr int loraDio0 = 14;

Sx1278LoRaDriver* g_driverInstance = nullptr;

void IRAM_ATTR onLoRaReceiveISR(int packetSize) {
    if (g_driverInstance != nullptr) {
        g_driverInstance->handleRxInterrupt(packetSize);
    }
}

}  // namespace

bool Sx1278LoRaDriver::begin() {
    g_driverInstance = this;
    pinMode(loraCs, OUTPUT);
    digitalWrite(loraCs, HIGH);
    SPI.begin(loraSck, loraMiso, loraMosi);
    LoRa.setPins(loraCs, loraReset, loraDio0);

    if (!LoRa.begin(frequencyHz)) {
        Serial.println("[GATEWAY ERROR] LoRa.begin() FAILED");
        Serial.flush();
        return false;
    }

    LoRa.setSpreadingFactor(spreadingFactor);
    LoRa.setSignalBandwidth(signalBandwidthHz);
    LoRa.setCodingRate4(codingRateDenominator);
    LoRa.setSyncWord(syncWord);
    LoRa.enableCrc();

    LoRa.onReceive(onLoRaReceiveISR);
    LoRa.receive();

    return true;
}

void IRAM_ATTR Sx1278LoRaDriver::handleRxInterrupt(int packetSize) {
    pendingPacketSize_ = packetSize;
    dio0Flag_ = true;
    isrCount_++;
}

bool Sx1278LoRaDriver::receive(RawRadioPacket& packet) {
    if (!dio0Flag_ || pendingPacketSize_ <= 0) {
        return false;
    }

    const int packetSize = pendingPacketSize_;
    dio0Flag_ = false;
    pendingPacketSize_ = 0;

    std::size_t bytesRead = 0U;
    while (LoRa.available() && bytesRead < rxBuffer_.size()) {
        const int value = LoRa.read();
        if (value < 0) break;
        rxBuffer_[bytesRead] = static_cast<std::uint8_t>(value);
        ++bytesRead;
    }

    if (bytesRead == 0U) {
        LoRa.receive();
        return false;
    }

    packet.bytes = rxBuffer_.data();
    packet.length = bytesRead;
    packet.rssiDbm = LoRa.packetRssi();
    packet.snrDb = LoRa.packetSnr();
    packet.receivedAtMs = millis();

    packetsReceivedCounter_++;

    LoRa.receive();
    return true;
}

bool Sx1278LoRaDriver::send(const std::uint8_t* data, std::size_t length) {
    if (data == nullptr || length == 0) return false;

    LoRa.idle();
    LoRa.beginPacket();
    const std::size_t written = LoRa.write(data, length);
    if (written != length) {
        LoRa.endPacket();
        LoRa.receive();
        return false;
    }
    bool res = (LoRa.endPacket() == 1);
    if (res) acksSentCounter_++;

    LoRa.receive();
    return res;
}

}  // namespace jalri::lora
