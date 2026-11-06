# TerraWatch gateway firmware

ESP32-S3 + SX1278 (433 MHz) LoRa receiver for the fixed forest stations. It listens for packets from stations and relays, validates them, sends the acknowledgement, keeps a node table, and prints each packet on the serial port.

It has no Wi-Fi, no HTTP and no backend client. Readings go from the serial port to the TerraWatch backend through the bridge script `../website/backend/scripts/station_bridge.py`; setup is in `../website/docs/STATIONS_BRIDGE_SETUP.md`.

```
station --LoRa--> [relay ...] --LoRa--> gateway --serial--> bridge --> backend
```

## What it does
- Continuous LoRa receive; parse and validate protocol v5 packets (`lib/Protocol`, same wire format as `../node-firmware`).
- Heartbeat and data acknowledgements; duplicate detection.
- Acts as the root of the collection tree: it sends an end-to-end receipt for data that came through relays, back through the relay the data arrived through, and remembers recent data longer so a resent alert is not counted twice.
- Relay route advertisements (type 5) are relay-to-relay traffic: they are ignored without an error.
- Node registry with liveness (`src/Registry`); online/offline status per node.
- Serial output: `[HEARTBEAT RX] ...` and `[DATA RX] source=0x.. sequence=.. rssi=.. snr=.. payload=<hex>`. The payload is whatever the station sent: air quality (tag `S`), position (`P`) or a sound alert (`A`); decoding is left to the bridge.
- Optional RF diagnostic build (`diagnostic-gateway`, `-D TW_RF_DIAGNOSTIC=1`).

## Build and test
```
pio run                              # default env esp32-s3-devkitm-1 (set upload_port in platformio.ini for your machine)
pio run -e diagnostic-gateway        # with RF diagnostics
bash test/run_protocol_contract.sh   # host test against ../node-firmware
```
Not tested on hardware.

## Layout
`src/` app, gateway service, ack handler, registry; `lib/` LoRa driver, protocol parser/validator, logger, HAL interfaces; `include/BuildConfig.h` name and version; `docs/architecture.md` the object graph and the per-loop flow.
