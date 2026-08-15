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

std::uint8_t readRegisterDiagnostic(std::uint8_t address) {
    SPI.beginTransaction(SPISettings(200000U, MSBFIRST, SPI_MODE0));
    digitalWrite(loraCs, LOW);
    SPI.transfer(address & 0x7FU);
    const std::uint8_t value = SPI.transfer(0x00U);
    digitalWrite(loraCs, HIGH);
    SPI.endTransaction();
    return value;
}

void printInitializationDiagnostics() {
    Serial.println("[GATEWAY DIAG] LoRa.begin() failure diagnostics");
    Serial.printf("[GATEWAY DIAG] Pins: SCK=%d MISO=%d MOSI=%d CS=%d RST=%d DIO0=%d\n",
                  loraSck, loraMiso, loraMosi, loraCs, loraReset, loraDio0);
    Serial.printf("[GATEWAY DIAG] Pin levels: CS=%d RST=%d DIO0=%d\n",
                  digitalRead(loraCs), digitalRead(loraReset), digitalRead(loraDio0));
    Serial.printf("[GATEWAY DIAG] SX127x REG_VERSION(0x42)=0x%02X (expected 0x12)\n",
                  static_cast<unsigned>(readRegisterDiagnostic(0x42U)));
    Serial.printf("[GATEWAY DIAG] SX127x REG_OP_MODE(0x01)=0x%02X\n",
                  static_cast<unsigned>(readRegisterDiagnostic(0x01U)));
    Serial.printf("[GATEWAY DIAG] SX127x REG_SYNC_WORD(0x39)=0x%02X\n",
                  static_cast<unsigned>(readRegisterDiagnostic(0x39U)));
}

SPIClass loraSpi(FSPI);

}  // namespace

bool Sx1278LoRaDriver::begin() {
    pinMode(loraCs, OUTPUT);
    digitalWrite(loraCs, HIGH);

    pinMode(loraReset, OUTPUT);
    digitalWrite(loraReset, HIGH);

    loraSpi.begin(loraSck, loraMiso, loraMosi, loraCs);
    LoRa.setSPI(loraSpi);
    LoRa.setPins(loraCs, loraReset, loraDio0);

    if (!LoRa.begin(frequencyHz)) {
        Serial.println("[GATEWAY ERROR] LoRa.begin() FAILED");
        printInitializationDiagnostics();
        Serial.flush();
        return false;
    }

    LoRa.setSpreadingFactor(spreadingFactor);
    LoRa.setSignalBandwidth(signalBandwidthHz);
    LoRa.setCodingRate4(codingRateDenominator);
    LoRa.setSyncWord(syncWord);
    LoRa.enableCrc();

    // Polling mode: no onReceive callback, no SPI-in-ISR hazard.
    // parsePacket() is called every tick from the main loop.
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
        if (value < 0) break;
        rxBuffer_[bytesRead] = static_cast<std::uint8_t>(value);
        ++bytesRead;
    }

    if (bytesRead == 0U) {
        return false;
    }

    packet.bytes = rxBuffer_.data();
    packet.length = bytesRead;
    packet.rssiDbm = LoRa.packetRssi();
    packet.snrDb = LoRa.packetSnr();
    packet.receivedAtMs = millis();

    packetsReceivedCounter_++;

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
