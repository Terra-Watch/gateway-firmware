
#include "FirmwareComposition.h"
#include "Uplink/Uplink.h"

#include <Arduino.h>

namespace {
terrawatch::FirmwareComposition* composition = nullptr;
bool loopCheckpointPrinted = false;
}

void setup() {
    Serial.begin(115200);
    delay(1500);

    Serial.println("================================");
    Serial.println("TERRAWATCH GATEWAY");
    Serial.println("================================");
    Serial.flush();

    Serial.println("[BOOT] 1: Serial initialized");
    Serial.flush();

    Serial.println("[BOOT] 2: Constructing FirmwareComposition");
    Serial.flush();
    composition = new terrawatch::FirmwareComposition();

    Serial.println("[BOOT] 3: App constructed & starting initialization");
    Serial.flush();

    Serial.println("[BOOT] 4: Initializing LoRa Driver");
    Serial.flush();
    const bool initResult = composition->app().begin();

    if (initResult) {
        Serial.println("[BOOT] 5: Setup completed successfully (LoRa Driver OK)");
    } else {
        Serial.println("[BOOT] 5: Setup completed (LoRa Driver init returned false - check physical SX1278 wiring)");
    }
    Serial.flush();
    terrawatch::uplink::Uplink::instance().begin();
    Serial.println(terrawatch::uplink::Uplink::instance().enabled() ? "[UPLINK] enabled" : "[UPLINK] off (no include/Secrets.h)");
}

void loop() {
    if (!loopCheckpointPrinted) {
        Serial.println("[BOOT] 6: Entering loop()");
        Serial.flush();
        loopCheckpointPrinted = true;
    }

    if (composition != nullptr) {
        composition->app().tick();
    }
    delay(10);
}
