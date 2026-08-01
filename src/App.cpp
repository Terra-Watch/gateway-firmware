#include "App.h"

#include <Arduino.h>

namespace jalri {

namespace {
constexpr std::uint32_t heartbeatIntervalMs = 5000U;
}

App::App(lora::ILoRaDriver& loraDriver,
         gateway::GatewayService& gatewayService,
         logging::ILogger& logger)
    : loraDriver_(loraDriver),
      gatewayService_(gatewayService),
      logger_(logger) {}

bool App::begin() {
    logger_.log(logging::LogLevel::info, "Jalri Gateway Firmware Starting...");
    if (!loraDriver_.begin()) {
        Serial.println("[LoRa] SX1278 initialization FAILED");
        Serial.println();
        if (Serial) {
            Serial.flush();
        }
        logger_.log(logging::LogLevel::error, "LoRa driver initialization failed! Check SX1278 SPI wiring & pins.");
        return false;
    }

    Serial.println("[LoRa] SX1278 initialization SUCCESS");
    Serial.println();
    if (Serial) {
        Serial.flush();
    }
    logger_.log(logging::LogLevel::info, "LoRa driver initialized successfully. Listening for physical BoatNode heartbeats...");
    return true;
}

void App::tick() {
    const std::uint32_t currentMs = millis();

    lora::LoRaPacket packet{};

    if (loraDriver_.receive(packet)) {
        gatewayService_.handleReceivedPacket(packet, &loraDriver_);
    }

    if ((currentMs - lastHeartbeatMs_) >= heartbeatIntervalMs) {
        lastHeartbeatMs_ = currentMs;
        printHeartbeat(currentMs);
    }
}

void App::printHeartbeat(std::uint32_t currentMs) {
    Serial.println("--------------------------------");
    Serial.println("Gateway Alive");
    Serial.println();
    Serial.print("Uptime: ");
    Serial.print(static_cast<unsigned long>(currentMs));
    Serial.println(" ms");
    Serial.println();
    Serial.print("Free Heap: ");
    Serial.print(static_cast<unsigned long>(ESP.getFreeHeap()));
    Serial.println(" bytes");
    Serial.println();
    Serial.print("Registered Nodes: ");
    Serial.println(static_cast<unsigned>(gatewayService_.deviceRegistry().deviceCount()));
    Serial.println();
    Serial.println("--------------------------------");
    if (Serial) {
        Serial.flush();
    }
}

}  // namespace jalri
