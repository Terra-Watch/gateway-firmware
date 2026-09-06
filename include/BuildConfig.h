#pragma once

#include <cstdint>

#ifndef JALRI_PRODUCTION_BACKEND
#define JALRI_PRODUCTION_BACKEND 1
#endif

namespace jalri {

struct BackendBuildConfig {
    // Development remains the default; production is selected explicitly at build time.
    static constexpr const char* wifiSsid = "Kireeti";
    static constexpr const char* wifiPassword = "kireetiv";
    static constexpr const char* developmentBaseUrl = "http://172.20.10.3:8000";
    static constexpr const char* productionBaseUrl = "https://jalari-frontend-dashboard.onrender.com";
    static constexpr const char* baseUrl =
        JALRI_PRODUCTION_BACKEND ? productionBaseUrl : developmentBaseUrl;
    static constexpr std::uint8_t gatewayId = 0xFE;
};

struct BuildConfig {
    static constexpr const char* firmwareName = "jalri-gateway-firmware";
    static constexpr const char* firmwareVersion = "0.1.0";
    static constexpr const char* targetBoard = "esp32-s3-devkitc-1-n16r8";
    static constexpr bool simulationMode = false;
};

}  // namespace jalri
