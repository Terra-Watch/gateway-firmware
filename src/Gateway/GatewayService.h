#pragma once

#include "IPacketParser.h"
#include "IPacketValidator.h"
#include "ILogger.h"
#include "LoRaTypes.h"
#include "ILoRaDriver.h"
#include "Registry/DeviceRegistry.h"
#include "Registry/NodeRegistryService.h"
#include "GatewayAckHandler.h"
#include "BackendIngestionClient.h"

namespace jalri::gateway {

class GatewayService final {
public:
    GatewayService(const protocol::IPacketParser& parser,
                   const protocol::IPacketValidator& validator,
                   logging::ILogger& logger,
                   backend::BackendIngestionClient& backendClient,
                   bool debugMode = false);

    void handleReceivedPacket(const lora::RawRadioPacket& packet, lora::ILoRaDriver* radioDriver = nullptr);
    void beginBackend();
    void tick(std::uint32_t currentMs, lora::ILoRaDriver* radioDriver = nullptr);

    void setDebugMode(bool enabled) { debugMode_ = enabled; }
    bool debugMode() const { return debugMode_; }

    registry::DeviceRegistry& deviceRegistry() { return deviceRegistry_; }
    const registry::DeviceRegistry& deviceRegistry() const { return deviceRegistry_; }

    registry::NodeRegistryService& nodeRegistryService() { return nodeRegistryService_; }
    const registry::NodeRegistryService& nodeRegistryService() const { return nodeRegistryService_; }

    const GatewayAckHandler& ackHandler() const { return ackHandler_; }

private:
    const protocol::IPacketParser& parser_;
    const protocol::IPacketValidator& validator_;
    logging::ILogger& logger_;
    bool debugMode_;
    registry::DeviceRegistry deviceRegistry_;
    registry::NodeRegistryService nodeRegistryService_;
    GatewayAckHandler ackHandler_;
    backend::BackendIngestionClient& backendClient_;
    std::uint32_t lastTablePrintMs_{0};
};

}  // namespace jalri::gateway
