#include "BackendIngestionClient.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include <cstdio>

namespace jalri::backend {

namespace {
constexpr std::uint32_t wifiRetryIntervalMs = 10000U;
constexpr const char* ingestionPath = "/api/ingest/gateway";
}

BackendIngestionClient::BackendIngestionClient(const BackendConfig& config)
    : config_(config) {}

bool BackendIngestionClient::configured_() const {
    return config_.wifiSsid != nullptr && config_.wifiPassword != nullptr &&
           config_.baseUrl != nullptr && config_.wifiSsid[0] != '\0' &&
           config_.baseUrl[0] != '\0';
}

void BackendIngestionClient::begin() {
    if (!configured_()) {
        Serial.println("[BACKEND ERROR] transport not configured");
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(config_.wifiSsid, config_.wifiPassword);
    wifiStarted_ = true;
    lastWifiAttemptMs_ = millis();
    Serial.printf("[BACKEND] Wi-Fi connecting to SSID: '%s'...\n", config_.wifiSsid);
}

void BackendIngestionClient::maintainWifi_(std::uint32_t currentMs) {
    if (!configured_() || !wifiStarted_) return;
    static bool loggedConnected = false;
    if (WiFi.status() == WL_CONNECTED) {
        if (!loggedConnected) {
            Serial.printf("[BACKEND] Wi-Fi CONNECTED! IP: %s\n", WiFi.localIP().toString().c_str());
            loggedConnected = true;
        }
        return;
    }
    loggedConnected = false;
    if ((currentMs - lastWifiAttemptMs_) < wifiRetryIntervalMs) return;

    lastWifiAttemptMs_ = currentMs;
    WiFi.disconnect();
    WiFi.begin(config_.wifiSsid, config_.wifiPassword);
    Serial.printf("[BACKEND ERROR] Wi-Fi unavailable; reconnecting to '%s'\n", config_.wifiSsid);
}

bool BackendIngestionClient::enqueueHeartbeat(const protocol::ParsedPacketView& packet,
                                              const lora::RawRadioPacket& radioPacket) {
    if (!configured_()) return false;

    for (auto& item : queue_) {
        if (item.occupied) continue;

        item.event = HeartbeatIngestionEvent{
            packet.nodeId,
            packet.packetId,
            packet.destinationId,
            packet.sequenceNumber,
            static_cast<std::uint8_t>(radioPacket.length),
            radioPacket.receivedAtMs,
            radioPacket.rssiDbm,
            radioPacket.snrDb,
        };
        item.attempts = 0;
        item.nextAttemptMs = millis();
        item.occupied = true;
        return true;
    }

    Serial.println("[BACKEND ERROR] ingestion queue full; event dropped");
    return false;
}

bool BackendIngestionClient::postHeartbeat_(QueueItem& item) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[BACKEND ERROR] unavailable");
        return false;
    }

    char payload[256]{};
    const int payloadLength = std::snprintf(
        payload,
        sizeof(payload),
        "{\"eventType\":\"HEARTBEAT_RECEIVED\",\"nodeId\":%lu,\"packetId\":%lu,\"destination\":%u,\"sequence\":%lu,\"packetSize\":%u,\"receivedAtMs\":%lu,\"rssi\":%d,\"snr\":%.2f}",
        static_cast<unsigned long>(item.event.nodeId),
        static_cast<unsigned long>(item.event.packetId),
        static_cast<unsigned>(item.event.destination),
        static_cast<unsigned long>(item.event.sequence),
        static_cast<unsigned>(item.event.packetSize),
        static_cast<unsigned long>(item.event.receivedAtMs),
        item.event.rssi,
        static_cast<double>(item.event.snr));

    if (payloadLength < 0 || static_cast<std::size_t>(payloadLength) >= sizeof(payload)) {
        Serial.println("[BACKEND ERROR] payload too large");
        return true;
    }

    String url = String(config_.baseUrl) + ingestionPath;
    HTTPClient http;
    http.setConnectTimeout(requestTimeoutMs);
    http.setTimeout(requestTimeoutMs);
    if (!http.begin(url)) {
        Serial.println("[BACKEND ERROR] HTTP client initialization failed");
        return false;
    }

    http.addHeader("Content-Type", "application/json");
    Serial.printf("[BACKEND TX] event: HEARTBEAT_RECEIVED gateway: 0x%02X node: %lu packet: %lu\n",
                  static_cast<unsigned>(config_.gatewayId),
                  static_cast<unsigned long>(item.event.nodeId),
                  static_cast<unsigned long>(item.event.packetId));
    const int status = http.POST(reinterpret_cast<std::uint8_t*>(payload), static_cast<std::size_t>(payloadLength));
    Serial.printf("[BACKEND RX] HTTP status: %d\n", status);
    http.end();

    if (status >= 200 && status < 300) return true;
    Serial.printf("[BACKEND ERROR] HTTP status: %d\n", status);
    return false;
}

void BackendIngestionClient::tick(std::uint32_t currentMs) {
    maintainWifi_(currentMs);
    if (!configured_() || WiFi.status() != WL_CONNECTED) return;

    for (auto& item : queue_) {
        if (!item.occupied || currentMs < item.nextAttemptMs) continue;

        item.attempts++;
        if (postHeartbeat_(item)) {
            item.occupied = false;
            continue;
        }

        if (item.attempts >= maxAttempts) {
            Serial.println("[BACKEND ERROR] bounded retries exhausted");
            item.occupied = false;
        } else {
            item.nextAttemptMs = currentMs + retryDelayMs;
        }
        break;
    }
}

}  // namespace jalri::backend
