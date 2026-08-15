#pragma once

#include <cstdint>

namespace jalri {

struct BackendBuildConfig {
    // Set these per deployment; no deployment URL is assumed here.
    static constexpr const char* wifiSsid = "JalriHotspot";
    static constexpr const char* wifiPassword = "kireetiv";
    static constexpr const char* baseUrl = "http://172.20.10.3:8000";
    static constexpr std::uint8_t gatewayId = 0xFE;
};

struct BuildConfig {
    static constexpr const char* firmwareName = "jalri-gateway-firmware";
    static constexpr const char* firmwareVersion = "0.1.0";
    static constexpr const char* targetBoard = "esp32-s3-devkitc-1-n16r8";
    static constexpr bool simulationMode = false;
};

}  // namespace jalri
