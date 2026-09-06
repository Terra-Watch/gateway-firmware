#include "BackendIngestionClient.h"
#include "RootCA.h"
#include "ILoRaDriver.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <cstdio>
#include <cstring>
#include <array>

namespace jalri::backend {

namespace {
constexpr std::uint32_t wifiRetryIntervalMs = 10000U;
constexpr const char* ingestionPath = "/api/ingest/gateway";
constexpr const char* messagePollPath = "/api/ingest/gateway/messages";
constexpr const char* messageReceiptPath = "/api/ingest/gateway/messages/receipt";
constexpr std::uint8_t gatewayProtocolVersion = 4U;
constexpr std::uint8_t gatewayTtl = 8U;

bool jsonStringField(const String& json, const char* key, char* out, std::size_t capacity) {
    if (out == nullptr || capacity == 0U) return false;
    const String marker = String("\"") + key + "\":\"";
    const int start = json.indexOf(marker);
    if (start < 0) return false;
    const int valueStart = start + marker.length();
    const int valueEnd = json.indexOf('"', valueStart);
    if (valueEnd < 0 || static_cast<std::size_t>(valueEnd - valueStart) >= capacity) return false;
    json.substring(valueStart, valueEnd).toCharArray(out, capacity);
    return true;
}

bool jsonNumberField(const String& json, const char* key, std::uint32_t& value) {
    const String marker = String("\"") + key + "\":";
    const int start = json.indexOf(marker);
    if (start < 0) return false;
    value = static_cast<std::uint32_t>(json.substring(start + marker.length()).toInt());
    return true;
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool parseUuid(const char* value, std::uint8_t* output) {
    std::size_t written = 0;
    int high = -1;
    for (const char* p = value; *p != '\0'; ++p) {
        if (*p == '-') continue;
        const int digit = hexValue(*p);
        if (digit < 0) return false;
        if (high < 0) high = digit;
        else { if (written >= 16U) return false; output[written++] = static_cast<std::uint8_t>((high << 4) | digit); high = -1; }
    }
    return written == 16U && high < 0;
}

std::uint16_t crc16(const std::uint8_t* data, std::size_t length) {
    std::uint16_t crc = 0xFFFFU;
    for (std::size_t i = 0; i < length; ++i) { crc ^= static_cast<std::uint16_t>(data[i]) << 8U; for (std::uint8_t bit = 0; bit < 8U; ++bit) crc = (crc & 0x8000U) ? static_cast<std::uint16_t>((crc << 1U) ^ 0x1021U) : static_cast<std::uint16_t>(crc << 1U); }
    return crc;
}
}  // namespace

BackendIngestionClient::BackendIngestionClient(const BackendConfig& config)
    : config_(config) {}

bool BackendIngestionClient::configured_() const {
    return config_.wifiSsid != nullptr && config_.wifiPassword != nullptr &&
           config_.baseUrl != nullptr && config_.wifiSsid[0] != '\0' &&
           config_.baseUrl[0] != '\0';
}

std::size_t BackendIngestionClient::getQueueDepth_() const {
    std::size_t count = 0;
    for (const auto& item : queue_) {
        if (item.occupied) {
            count++;
        }
    }
    return count;
}

void BackendIngestionClient::extractHostname_(char* hostBuf, std::size_t maxLen) const {
    if (hostBuf == nullptr || maxLen == 0) return;
    hostBuf[0] = '\0';
    if (!configured_()) return;

    const char* p = config_.baseUrl;
    if (strncmp(p, "https://", 8) == 0) {
        p += 8;
    } else if (strncmp(p, "http://", 7) == 0) {
        p += 7;
    }

    std::size_t i = 0;
    while (*p != '\0' && *p != '/' && *p != ':' && i < (maxLen - 1)) {
        hostBuf[i++] = *p++;
    }
    hostBuf[i] = '\0';
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
    if (WiFi.status() == WL_CONNECTED) {
        if (!diagnosticsRun_) {
            runNetworkDiagnostics();
            diagnosticsRun_ = true;
        }
        return;
    }

    diagnosticsRun_ = false;
    if ((currentMs - lastWifiAttemptMs_) < wifiRetryIntervalMs) return;

    lastWifiAttemptMs_ = currentMs;
    WiFi.disconnect();
    WiFi.begin(config_.wifiSsid, config_.wifiPassword);
    Serial.printf("[BACKEND ERROR] Wi-Fi unavailable; reconnecting to '%s'\n", config_.wifiSsid);
}

void BackendIngestionClient::runNetworkDiagnostics() {
    char host[128]{};
    extractHostname_(host, sizeof(host));

    // 1. [WIFI]
    Serial.println("[WIFI]");
    Serial.println("status=CONNECTED");
    Serial.printf("ip=%s\n", WiFi.localIP().toString().c_str());
    Serial.printf("gateway=%s\n", WiFi.gatewayIP().toString().c_str());
    Serial.printf("dns=%s\n", WiFi.dnsIP().toString().c_str());
    Serial.printf("rssi=%d\n", WiFi.RSSI());
    Serial.println();

    // 2. [DNS TEST]
    Serial.println("[DNS TEST]");
    Serial.printf("hostname=%s\n", host);
    IPAddress resolvedIp;
    const bool dnsSuccess = WiFi.hostByName(host, resolvedIp);
    if (dnsSuccess) {
        Serial.println("result=SUCCESS");
        Serial.printf("address=%s\n", resolvedIp.toString().c_str());
    } else {
        Serial.println("result=FAIL");
        Serial.println("address=resolution_timeout");
    }
    Serial.println();

    // 3. [TCP TEST]
    Serial.println("[TCP TEST]");
    Serial.printf("host=%s\n", host);
    Serial.println("port=443");
    WiFiClient tcpClient;
    tcpClient.setTimeout(5);
    const bool tcpSuccess = (dnsSuccess && tcpClient.connect(resolvedIp, 443, 5000));
    if (tcpSuccess) {
        Serial.println("result=SUCCESS");
    } else {
        Serial.println("result=FAIL");
        Serial.println("error=connection_failed");
    }
    tcpClient.stop();
    Serial.println();

    // 4. [TLS TEST] - Strict validation with Root CA
    Serial.println("[TLS TEST]");
    Serial.printf("host=%s\n", host);
    WiFiClientSecure tlsClient;
    tlsClient.setCACert(PRODUCTION_ROOT_CA);
    tlsClient.setTimeout(10);
    const bool tlsSuccess = tlsClient.connect(host, 443, 10000);
    if (tlsSuccess) {
        Serial.println("result=SUCCESS");
        Serial.println("stage=CERTIFICATE_VERIFICATION");
        Serial.println("error=none");
    } else {
        char errBuf[128]{};
        tlsClient.lastError(errBuf, sizeof(errBuf));
        Serial.println("result=FAIL");
        Serial.println("stage=HANDSHAKE");
        Serial.printf("error=%s\n", errBuf[0] != '\0' ? errBuf : "handshake_timeout");
    }
    tlsClient.stop();
    Serial.println();

    // 5. [HEALTH TEST]
    WiFiClientSecure healthClient;
    healthClient.setCACert(PRODUCTION_ROOT_CA);
    healthClient.setTimeout(10);
    HTTPClient http;
    const String healthUrl = String(config_.baseUrl) + "/health";
    http.begin(healthClient, healthUrl);
    http.setConnectTimeout(requestTimeoutMs);
    http.setTimeout(requestTimeoutMs);

    const std::uint32_t healthStartMs = millis();
    const int healthStatus = http.GET();
    const std::uint32_t healthLatencyMs = millis() - healthStartMs;
    http.end();

    Serial.println("[HEALTH TEST]");
    Serial.printf("status=%d\n", healthStatus);
    Serial.printf("latencyMs=%lu\n", static_cast<unsigned long>(healthLatencyMs));
    Serial.println();
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
        item.telemetryEvent = false;
        item.nextAttemptMs = millis();
        item.occupied = true;

        Serial.println("[BACKEND QUEUE]");
        Serial.printf("depth=%u\n", static_cast<unsigned>(getQueueDepth_()));
        Serial.printf("capacity=%u\n", static_cast<unsigned>(queueCapacity));
        Serial.println("[BACKEND QUEUE]");
        Serial.printf("enqueued sequence=%lu\n", static_cast<unsigned long>(packet.sequenceNumber));
        return true;
    }

    Serial.println("[BACKEND QUEUE FULL]");
    Serial.printf("sequence=%lu\n", static_cast<unsigned long>(packet.sequenceNumber));
    Serial.println("[BACKEND ERROR] ingestion queue full; event dropped");
    return false;
}

bool BackendIngestionClient::enqueueTelemetry(const protocol::ParsedPacketView& packet,
                                              const lora::RawRadioPacket& radioPacket) {
    if (!configured_() || packet.payloadLength != 12U || packet.payload[0] != 0x54U || packet.payload[1] != 1U) {
        return false;
    }

    for (auto& item : queue_) {
        if (item.occupied) continue;

        const auto* p = packet.payload;
        item.telemetry = TelemetryIngestionEvent{
            packet.nodeId,
            packet.sequenceNumber,
            static_cast<std::uint8_t>(radioPacket.length),
            radioPacket.receivedAtMs,
            radioPacket.rssiDbm,
            radioPacket.snrDb,
            (p[2] & 0x01U) != 0U,
            static_cast<std::uint16_t>(p[3] | (p[4] << 8U)),
            p[5],
            p[6],
            p[7],
            p[8],
            p[9],
            static_cast<std::int16_t>(static_cast<std::uint16_t>(p[10] | (p[11] << 8U)))
        };
        item.attempts = 0;
        item.nextAttemptMs = millis();
        item.telemetryEvent = true;
        item.occupied = true;

        Serial.println("[BACKEND QUEUE]");
        Serial.printf("depth=%u\n", static_cast<unsigned>(getQueueDepth_()));
        Serial.printf("capacity=%u\n", static_cast<unsigned>(queueCapacity));
        Serial.println("[BACKEND QUEUE]");
        Serial.printf("enqueued sequence=%lu\n", static_cast<unsigned long>(packet.sequenceNumber));
        return true;
    }

    Serial.println("[BACKEND QUEUE FULL]");
    Serial.printf("sequence=%lu\n", static_cast<unsigned long>(packet.sequenceNumber));
    Serial.println("[BACKEND ERROR] ingestion queue full; telemetry dropped");
    return false;
}

bool BackendIngestionClient::postHeartbeat_(QueueItem& item) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[BACKEND ERROR] unavailable");
        return false;
    }

    char host[128]{};
    extractHostname_(host, sizeof(host));

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

    const uint32_t heapBefore = ESP.getFreeHeap();
    Serial.println("[HEAP]");
    Serial.printf("before=%lu\n", static_cast<unsigned long>(heapBefore));

    WiFiClientSecure client;
    client.setCACert(PRODUCTION_ROOT_CA);
    client.setTimeout(requestTimeoutMs / 1000);

    const String url = String(config_.baseUrl) + ingestionPath;
    HTTPClient http;
    http.setConnectTimeout(requestTimeoutMs);
    http.setTimeout(requestTimeoutMs);

    if (!http.begin(client, url)) {
        Serial.println("[BACKEND ERROR] HTTP client initialization failed");
        return false;
    }

    http.addHeader("Content-Type", "application/json");

    Serial.println("[BACKEND REQUEST]");
    Serial.println("method=POST");
    Serial.printf("host=%s\n", host);
    Serial.printf("path=%s\n", ingestionPath);
    Serial.println("event=HEARTBEAT_RECEIVED");
    Serial.printf("node=%lu\n", static_cast<unsigned long>(item.event.nodeId));
    Serial.printf("packet=%lu\n", static_cast<unsigned long>(item.event.packetId));
    Serial.printf("payloadBytes=%d\n", payloadLength);

    const std::uint32_t startMs = millis();
    const int status = http.POST(reinterpret_cast<std::uint8_t*>(payload), static_cast<std::size_t>(payloadLength));
    const std::uint32_t latencyMs = millis() - startMs;
    http.end();

    Serial.println("[BACKEND RESPONSE]");
    Serial.printf("status=%d\n", status);
    Serial.printf("latencyMs=%lu\n", static_cast<unsigned long>(latencyMs));

    const uint32_t heapAfter = ESP.getFreeHeap();
    Serial.printf("after=%lu\n", static_cast<unsigned long>(heapAfter));

    if (status >= 200 && status < 300) return true;
    return false;
}

bool BackendIngestionClient::postTelemetry_(QueueItem& item) {
    if (WiFi.status() != WL_CONNECTED) return false;

    char host[128]{};
    extractHostname_(host, sizeof(host));

    char payload[384]{};
    const auto& e = item.telemetry;
    const int payloadLength = std::snprintf(
        payload, sizeof(payload),
        "{\"eventType\":\"TELEMETRY_RECEIVED\",\"nodeId\":%lu,\"sequence\":%lu,\"packetSize\":%u,\"receivedAtMs\":%lu,\"rssi\":%d,\"snr\":%.2f,\"rtcValid\":%s,\"year\":%u,\"month\":%u,\"day\":%u,\"hour\":%u,\"minute\":%u,\"second\":%u,\"temperatureC\":%.2f}",
        static_cast<unsigned long>(e.nodeId), static_cast<unsigned long>(e.sequence), static_cast<unsigned>(e.packetSize),
        static_cast<unsigned long>(e.receivedAtMs), e.rssi, static_cast<double>(e.snr), e.rtcValid ? "true" : "false",
        e.year, e.month, e.day, e.hour, e.minute, e.second, static_cast<double>(e.temperatureCentiC) / 100.0);

    if (payloadLength < 0 || static_cast<std::size_t>(payloadLength) >= sizeof(payload)) return true;

    const uint32_t heapBefore = ESP.getFreeHeap();
    Serial.println("[HEAP]");
    Serial.printf("before=%lu\n", static_cast<unsigned long>(heapBefore));

    WiFiClientSecure client;
    client.setCACert(PRODUCTION_ROOT_CA);
    client.setTimeout(requestTimeoutMs / 1000);

    const String url = String(config_.baseUrl) + ingestionPath;
    HTTPClient http;
    http.setConnectTimeout(requestTimeoutMs);
    http.setTimeout(requestTimeoutMs);

    if (!http.begin(client, url)) return false;
    http.addHeader("Content-Type", "application/json");

    Serial.println("[BACKEND REQUEST]");
    Serial.println("method=POST");
    Serial.printf("host=%s\n", host);
    Serial.printf("path=%s\n", ingestionPath);
    Serial.println("event=TELEMETRY_RECEIVED");
    Serial.printf("node=%lu\n", static_cast<unsigned long>(e.nodeId));
    Serial.printf("sequence=%lu\n", static_cast<unsigned long>(e.sequence));
    Serial.printf("payloadBytes=%d\n", payloadLength);

    const std::uint32_t startMs = millis();
    const int status = http.POST(reinterpret_cast<std::uint8_t*>(payload), static_cast<std::size_t>(payloadLength));
    const std::uint32_t latencyMs = millis() - startMs;
    http.end();

    Serial.println("[BACKEND RESPONSE]");
    Serial.printf("status=%d\n", status);
    Serial.printf("latencyMs=%lu\n", static_cast<unsigned long>(latencyMs));

    const uint32_t heapAfter = ESP.getFreeHeap();
    Serial.printf("after=%lu\n", static_cast<unsigned long>(heapAfter));

    return status >= 200 && status < 300;
}

void BackendIngestionClient::tick(std::uint32_t currentMs) {
    maintainWifi_(currentMs);
    if (!configured_() || WiFi.status() != WL_CONNECTED) return;

    for (auto& item : queue_) {
        if (!item.occupied || currentMs < item.nextAttemptMs) continue;

        item.attempts++;
        const bool success = item.telemetryEvent ? postTelemetry_(item) : postHeartbeat_(item);
        const std::uint32_t seq = item.telemetryEvent ? item.telemetry.sequence : item.event.sequence;

        if (success) {
            item.occupied = false;
            Serial.println("[BACKEND QUEUE]");
            Serial.printf("depth=%u\n", static_cast<unsigned>(getQueueDepth_()));
            Serial.printf("capacity=%u\n", static_cast<unsigned>(queueCapacity));
            Serial.println("[BACKEND QUEUE]");
            Serial.printf("dequeued sequence=%lu\n", static_cast<unsigned long>(seq));
            continue;
        }

        Serial.println("[RETRY]");
        Serial.printf("event=%s\n", item.telemetryEvent ? "TELEMETRY_RECEIVED" : "HEARTBEAT_RECEIVED");
        Serial.printf("attempt=%u\n", static_cast<unsigned>(item.attempts));
        Serial.printf("maxAttempts=%u\n", static_cast<unsigned>(maxAttempts));
        Serial.println("reason=http_non_2xx");

        if (item.attempts >= maxAttempts) {
            Serial.println("[BACKEND ERROR] bounded retries exhausted");
            item.occupied = false;
            Serial.println("[BACKEND QUEUE]");
            Serial.printf("depth=%u\n", static_cast<unsigned>(getQueueDepth_()));
            Serial.printf("capacity=%u\n", static_cast<unsigned>(queueCapacity));
            Serial.println("[BACKEND QUEUE]");
            Serial.printf("dequeued sequence=%lu\n", static_cast<unsigned long>(seq));
        } else {
            item.nextAttemptMs = currentMs + retryDelayMs;
        }
        break;
    }
}

void BackendIngestionClient::dispatchMessages(lora::ILoRaDriver& radioDriver) {
    if (WiFi.status() != WL_CONNECTED || !configured_()) return;
    WiFiClientSecure client;
    client.setCACert(PRODUCTION_ROOT_CA);
    client.setTimeout(requestTimeoutMs / 1000U);
    HTTPClient http;
    if (!http.begin(client, String(config_.baseUrl) + messagePollPath)) return;
    http.setConnectTimeout(requestTimeoutMs);
    http.setTimeout(requestTimeoutMs);
    const int status = http.GET();
    String response = status >= 200 && status < 300 ? http.getString() : String();
    http.end();
    if (response.length() == 0) return;

    char clientEventId[40]{};
    char text[32]{};
    std::uint32_t nodeId = 0;
    if (!jsonStringField(response, "clientEventId", clientEventId, sizeof(clientEventId)) ||
        !jsonStringField(response, "text", text, sizeof(text)) ||
        !jsonNumberField(response, "nodeId", nodeId) || nodeId == 0U || nodeId > 255U) return;

    const std::size_t textLength = std::strlen(text);
    if (textLength == 0U || textLength > 27U) return;
    std::array<std::uint8_t, 61> packet{};
    packet[0] = 0xA5U; packet[1] = gatewayProtocolVersion; packet[2] = 1U;
    packet[3] = config_.gatewayId; packet[4] = static_cast<std::uint8_t>(nodeId); packet[5] = config_.gatewayId;
    packet[6] = static_cast<std::uint8_t>(outboundSequence_ & 0xFFU); packet[7] = static_cast<std::uint8_t>((outboundSequence_ >> 8U) & 0xFFU); ++outboundSequence_;
    packet[8] = gatewayTtl; packet[9] = 0U; packet[10] = static_cast<std::uint8_t>(21U + textLength);
    const std::size_t payloadOffset = 11U;
    packet[payloadOffset] = 0x4AU; packet[payloadOffset + 1U] = 0x43U; packet[payloadOffset + 2U] = 0x4DU; packet[payloadOffset + 3U] = 0x31U;
    if (!parseUuid(clientEventId, packet.data() + payloadOffset + 4U)) return;
    packet[payloadOffset + 20U] = static_cast<std::uint8_t>(textLength);
    std::memcpy(packet.data() + payloadOffset + 21U, text, textLength);
    const std::size_t bodyLength = 11U + 21U + textLength;
    const std::uint16_t crc = crc16(packet.data(), bodyLength);
    packet[bodyLength] = static_cast<std::uint8_t>(crc & 0xFFU); packet[bodyLength + 1U] = static_cast<std::uint8_t>((crc >> 8U) & 0xFFU);
    if (!radioDriver.send(packet.data(), bodyLength + 2U)) return;
    postReceipt_(clientEventId, static_cast<std::uint8_t>(nodeId), static_cast<std::uint16_t>(outboundSequence_ - 1U), false);
}

void BackendIngestionClient::reportCrewMessageReceipt(const protocol::ParsedPacketView& packet) {
    if (packet.payloadLength < 21U || packet.payload[0] != 0x4AU || packet.payload[1] != 0x43U || packet.payload[2] != 0x4DU || packet.payload[3] != 0x31U) return;
    char clientEventId[37]{}; const std::uint8_t* id = packet.payload + 4U; const char* hex = "0123456789abcdef"; std::size_t offset = 0;
    for (std::size_t i = 0; i < 16U; ++i) { if (i == 4U || i == 6U || i == 8U || i == 10U) clientEventId[offset++] = '-'; clientEventId[offset++] = hex[id[i] >> 4U]; clientEventId[offset++] = hex[id[i] & 0x0FU]; }
    clientEventId[offset] = '\0';
    postReceipt_(clientEventId, static_cast<std::uint8_t>(packet.nodeId), static_cast<std::uint16_t>(packet.sequenceNumber), true);
}

bool BackendIngestionClient::postReceipt_(const char* clientEventId, std::uint8_t nodeId, std::uint16_t sequence, bool boatNodeReceived) {
    WiFiClientSecure client; client.setCACert(PRODUCTION_ROOT_CA); client.setTimeout(requestTimeoutMs / 1000U);
    HTTPClient http; if (!http.begin(client, String(config_.baseUrl) + messageReceiptPath)) return false;
    http.setConnectTimeout(requestTimeoutMs); http.setTimeout(requestTimeoutMs); http.addHeader("Content-Type", "application/json");
    char body[180]{}; const int length = std::snprintf(body, sizeof(body), "{\"clientEventId\":\"%s\",\"nodeId\":%u,\"sequence\":%u,\"boatNodeReceived\":%s}", clientEventId, static_cast<unsigned>(nodeId), static_cast<unsigned>(sequence), boatNodeReceived ? "true" : "false");
    const int status = length > 0 ? http.POST(reinterpret_cast<std::uint8_t*>(body), static_cast<std::size_t>(length)) : -1; http.end();
    return status >= 200 && status < 300;
}

}  // namespace jalri::backend
