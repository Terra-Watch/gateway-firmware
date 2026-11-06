#include "Uplink.h"

#include <Arduino.h>

#if __has_include("Secrets.h")
#include "Secrets.h"
#define TW_UPLINK_ENABLED 1
#else
#define TW_UPLINK_ENABLED 0
#endif

#if TW_UPLINK_ENABLED
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <mbedtls/md.h>
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
#endif

namespace terrawatch::uplink {

#if TW_UPLINK_ENABLED
namespace {
constexpr std::size_t kQueueLength = 96;         // readings kept while offline
constexpr std::size_t kBatch = 20;               // readings per request
constexpr std::uint32_t kFlushMs = 5000;         // wait this long for more readings before sending
constexpr std::uint32_t kWifiRetryMs = 10000;
QueueHandle_t queue = nullptr;
bool started = false;

String hmacHex(const String& body) {
    unsigned char out[32];
    mbedtls_md_context_t ctx; mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&ctx, reinterpret_cast<const unsigned char*>(TW_GATEWAY_SECRET), strlen(TW_GATEWAY_SECRET));
    mbedtls_md_hmac_update(&ctx, reinterpret_cast<const unsigned char*>(body.c_str()), body.length());
    mbedtls_md_hmac_finish(&ctx, out); mbedtls_md_free(&ctx);
    char hex[65]; for (int i = 0; i < 32; ++i) snprintf(&hex[i * 2], 3, "%02x", out[i]);
    return String(hex);
}

String toJson(const DataEvent* events, std::size_t n) {
    String body = "{\"readings\":[";
    for (std::size_t i = 0; i < n; ++i) {
        const DataEvent& e = events[i];
        if (i) body += ',';
        char head[96]; snprintf(head, sizeof(head), "{\"node_id\":%u,\"seq\":%u,\"rssi\":%d,\"snr\":%.1f,\"payload_hex\":\"", e.node, e.sequence, e.rssi, static_cast<double>(e.snr));
        body += head;
        for (std::uint8_t k = 0; k < e.length; ++k) { char b[3]; snprintf(b, sizeof(b), "%02X", e.payload[k]); body += b; }
        body += "\"}";
    }
    body += "]}";
    return body;
}

bool post(const String& body) {
    WiFiClientSecure client;
#ifdef TW_UPLINK_INSECURE_TLS
    client.setInsecure();                          // debugging only: does not check the server certificate
#else
    client.setCACertBundle(rootca_crt_bundle_start);
#endif
    HTTPClient http;
    if (!http.begin(client, String(TW_BACKEND_URL) + "/api/stations/ingest")) return false;
    http.setTimeout(15000);
    http.addHeader("content-type", "application/json"); http.addHeader("x-gateway-id", TW_GATEWAY_ID); http.addHeader("x-signature", hmacHex(body));
    const int code = http.POST(body);
    Serial.printf("[UPLINK] POST %u bytes -> HTTP %d %s\n", static_cast<unsigned>(body.length()), code, code > 0 ? http.getString().substring(0, 90).c_str() : http.errorToString(code).c_str());
    http.end();
    return code == 200;
}

void task(void*) {
    static DataEvent batch[kBatch]; std::size_t held = 0; std::uint32_t lastWifi = 0;
    WiFi.mode(WIFI_STA); WiFi.setAutoReconnect(true); WiFi.begin(TW_WIFI_SSID, TW_WIFI_PASSWORD);
    Serial.printf("[UPLINK] connecting to Wi-Fi \"%s\"\n", TW_WIFI_SSID);
    for (;;) {
        if (WiFi.status() != WL_CONNECTED) {
            if (millis() - lastWifi > kWifiRetryMs) { lastWifi = millis(); Serial.printf("[UPLINK] Wi-Fi not connected (status %d), retrying\n", static_cast<int>(WiFi.status())); WiFi.disconnect(); WiFi.begin(TW_WIFI_SSID, TW_WIFI_PASSWORD); }
            vTaskDelay(pdMS_TO_TICKS(1000)); continue;
        }
        if (held == 0) { if (xQueueReceive(queue, &batch[0], pdMS_TO_TICKS(1000)) == pdTRUE) held = 1; else continue; }
        const std::uint32_t until = millis() + kFlushMs;
        while (held < kBatch && static_cast<std::int32_t>(until - millis()) > 0) { if (xQueueReceive(queue, &batch[held], pdMS_TO_TICKS(200)) == pdTRUE) ++held; }
        if (post(toJson(batch, held))) held = 0; else vTaskDelay(pdMS_TO_TICKS(5000));      // keep the batch and try again
    }
}
}  // namespace
#endif

Uplink& Uplink::instance() { static Uplink u; return u; }

bool Uplink::enabled() const { return TW_UPLINK_ENABLED == 1; }

void Uplink::begin() {
#if TW_UPLINK_ENABLED
    if (started) return;
    started = true; queue = xQueueCreate(kQueueLength, sizeof(DataEvent));
    xTaskCreatePinnedToCore(task, "uplink", 12288, nullptr, 1, nullptr, 0);
#endif
}

void Uplink::enqueue(const DataEvent& event) {
#if TW_UPLINK_ENABLED
    if (!queue) return;
    if (xQueueSend(queue, &event, 0) != pdTRUE) { DataEvent dropped; xQueueReceive(queue, &dropped, 0); xQueueSend(queue, &event, 0); }   // full: drop the oldest
#else
    (void)event;
#endif
}

}  // namespace terrawatch::uplink
