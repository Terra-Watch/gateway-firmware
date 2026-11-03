#pragma once

namespace terrawatch {

struct BuildConfig {
    static constexpr const char* firmwareName = "terrawatch-gateway-firmware";
    static constexpr const char* firmwareVersion = "0.1.0";
    static constexpr const char* targetBoard = "esp32-s3-devkitc-1-n16r8";
    static constexpr bool simulationMode = false;
};

}  // namespace terrawatch
