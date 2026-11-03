# TerraWatch gateway firmware

ESP32-S3 + SX1278 (433 MHz) LoRa receiver for the fixed forest stations. It listens for packets from stations and relays, validates them, sends the acknowledgement, keeps a node table, and prints each packet on the serial port.

It has no Wi-Fi, no HTTP and no backend client. Getting readings from the serial port into the TerraWatch backend (`../docs/13-lookout-stations.md`, section 4) is a later step.

```
station --LoRa--> [relay ...] --LoRa--> gateway --serial--> (bridge, later) --> backend
```

## What it does
- Continuous LoRa receive; parse and validate protocol v4 packets (`lib/Protocol`, same wire format as `../node-firmware`).
- Heartbeat and data acknowledgements; duplicate detection.
- Node registry with liveness (`src/Registry`); online/offline status per node.
- Serial output: `[HEARTBEAT RX] ...` and `[DATA RX] source=0x.. sequence=.. rssi=.. snr=.. payload=<hex>`.

## Build and test
```
pio run                         # default env (set upload_port in platformio.ini for your machine)
bash test/run_protocol_contract.sh   # host test against ../node-firmware
```
Not tested on hardware.

## Layout
`src/` app, gateway service, ack handler, registry; `lib/` LoRa driver, protocol parser/validator, logger, HAL interfaces; `include/BuildConfig.h` name and version.
