#pragma once

#include <cstddef>
#include <cstdint>

namespace terrawatch::uplink {

/** One data packet heard by the gateway, as the backend's /api/stations/ingest wants it. */
struct DataEvent {
    std::uint8_t node;
    std::uint16_t sequence;
    std::int16_t rssi;
    float snr;
    std::uint8_t length;
    std::uint8_t payload[32];
};

/** Sends data packets to the backend over Wi-Fi. Runs in its own task on the other core so a slow HTTPS call never delays the LoRa receive and acknowledgement loop.
 *  Needs include/Secrets.h; without it every call does nothing. Readings wait in a queue while the Wi-Fi or the backend is down (oldest dropped when full). */
class Uplink final {
public:
    static Uplink& instance();
    void begin();                              // starts the task; safe to call once
    bool enabled() const;
    void enqueue(const DataEvent& event);      // from the LoRa loop; never blocks
};

}  // namespace terrawatch::uplink
