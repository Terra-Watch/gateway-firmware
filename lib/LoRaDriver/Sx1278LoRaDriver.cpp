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

}  // namespace

bool Sx1278LoRaDriver::begin() {
    SPI.begin(loraSck, loraMiso, loraMosi, loraCs);
    LoRa.setPins(loraCs, loraReset, loraDio0);

    if (!LoRa.begin(frequencyHz)) {
        return false;
    }

    LoRa.setSpreadingFactor(spreadingFactor);
    LoRa.setSignalBandwidth(signalBandwidthHz);
    LoRa.setCodingRate4(codingRateDenominator);
    LoRa.setSyncWord(syncWord);
    LoRa.enableCrc();
    LoRa.receive();

    return true;
}

bool Sx1278LoRaDriver::receive(RawRadioPacket& packet) {
    const int packetSize = LoRa.parsePacket();

    if (packetSize <= 0) {
        return false;
    }

    std::size_t bytesRead = 0U;

    while (LoRa.available() && bytesRead < rxBuffer_.size()) {
        const int value = LoRa.read();

        if (value < 0) {
            break;
        }

        rxBuffer_[bytesRead] = static_cast<std::uint8_t>(value);
        ++bytesRead;
    }

    packet.bytes = rxBuffer_.data();
    packet.length = bytesRead;
    packet.rssiDbm = LoRa.packetRssi();
    packet.snrDb = LoRa.packetSnr();
    packet.receivedAtMs = millis();

    LoRa.receive();

    return bytesRead > 0U;
}

}  // namespace jalri::lora
