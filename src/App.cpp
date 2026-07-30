#include "App.h"

namespace jalri {

App::App(lora::ILoRaDriver& loraDriver,
         gateway::GatewayService& gatewayService,
         logging::ILogger& logger)
    : loraDriver_(loraDriver),
      gatewayService_(gatewayService),
      logger_(logger) {}

bool App::begin() {
    logger_.log(logging::LogLevel::info, "Jalri Gateway Firmware Starting...");
    if (!loraDriver_.begin()) {
        logger_.log(logging::LogLevel::error, "LoRa driver initialization failed! Check SX1278 SPI wiring & pins.");
        return false;
    }

    logger_.log(logging::LogLevel::info, "LoRa driver initialized successfully. Listening for physical BoatNode heartbeats...");
    return true;
}

void App::tick() {
    lora::LoRaPacket packet{};

    if (loraDriver_.receive(packet)) {
        gatewayService_.handleReceivedPacket(packet);
    }
}

}  // namespace jalri

