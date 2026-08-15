# Jalri Gateway Firmware

Embedded firmware for the Jalri Maritime Edge Gateway. The Gateway receives LoRa packets from BoatNodes, validates them, transmits acknowledgements, tracks registered devices, and forwards heartbeat events to the Jalri backend over HTTP.

## Overview

The Gateway firmware runs on an ESP32-S3 microcontroller paired with an SX1278 LoRa transceiver (433 MHz). It implements a receive-validate-acknowledge-ingest pipeline with a modular, interface-driven architecture.

The firmware handles:

- Continuous LoRa packet reception
- Protocol v3 packet parsing and validation
- Heartbeat ACK and Data ACK transmission back to BoatNodes
- Duplicate packet detection
- Device registration and liveness tracking
- Wi-Fi connectivity and HTTP POST ingestion to the Jalri backend

## Role in Jalri

```text
[ BoatNode ] --LoRa--> [ Gateway ] --HTTP--> [ Backend ] --> [ Dashboard ]
                            ▲
                         This repo
```

The Gateway is the bridge between the LoRa mesh network and the IP-based backend infrastructure. It receives BoatNode packets over LoRa and pushes events to the backend via Wi-Fi.

## Implemented Features

- **LoRa reception** — continuous listening for BoatNode packets via SX1278 driver
- **Packet parsing** — protocol v3 binary packet parser extracting header fields, payload, and CRC
- **Packet validation** — preamble, version, CRC-16, and structural integrity checks
- **Heartbeat ACK transmission** — 21-byte acknowledgement packets constructed and sent over LoRa (preamble 0xA5, protocol v3, type 4, CRC-16)
- **Data ACK transmission** — 14-byte acknowledgement for DATA packets (type 3)
- **Duplicate detection** — per-node heartbeat dedup by packet ID; per-source/type/sequence dedup for DATA packets; duplicates are re-ACKed but not reprocessed
- **Device registry** — tracks each known node (first seen, last seen, RSSI, SNR, packet count, online/offline status with configurable 30 s timeout)
- **Node registry service** — manages node liveness with heartbeat-driven state tracking
- **Backend ingestion** — queues heartbeat events (8-slot ring buffer) and sends JSON HTTP POST to `/api/ingest/gateway` with retry (2 attempts, 250 ms retry delay, 1500 ms timeout)
- **Wi-Fi management** — automatic connection and reconnection with 10 s retry interval
- **Gateway heartbeat log** — periodic serial output of uptime, free heap, and registered node count (every 5 s)
- **Debug mode** — optional raw packet hex logging

## Architecture

```text
main.cpp
  └── FirmwareComposition (composition root)
        ├── Sx1278LoRaDriver (ILoRaDriver)
        ├── PacketParser (IPacketParser)
        ├── PacketValidator (IPacketValidator)
        ├── SerialLogger (ILogger)
        ├── BackendIngestionClient
        ├── GatewayService
        │     ├── GatewayAckHandler (ACK construction + duplicate detection)
        │     ├── DeviceRegistry (node tracking)
        │     └── NodeRegistryService (liveness management)
        └── App (tick loop: receive → handle → heartbeat log)
```

**Receive pipeline:**

```text
SX1278 → LoRaDriver → Raw Bytes → IPacketParser → ParsedPacketView → IPacketValidator → GatewayService → ACK + Registry + Backend
```

All application-layer code depends on interfaces. Platform-specific work stays behind HAL and driver boundaries. `FirmwareComposition` is the sole composition root.

## Technology Stack

| Component | Technology |
|---|---|
| MCU | ESP32-S3-DevKitC-1-N16R8 |
| LoRa transceiver | SX1278 (via sandeepmistry/LoRa library) |
| Wi-Fi | ESP32 built-in (WPA2) |
| HTTP client | ESP32 HTTPClient library |
| Build system | PlatformIO |
| Framework | Arduino |
| Language | C++17 (`-std=gnu++17`) |
| Flash | 16 MB |
| PSRAM | Enabled |

## Project Structure

```text
jalri-gateway-firmware/
├── include/
│   └── BuildConfig.h         # Wi-Fi credentials, backend URL, gateway ID, firmware version
├── src/
│   ├── main.cpp              # Arduino setup()/loop() entry point
│   ├── App.h / App.cpp       # Application tick loop
│   ├── FirmwareComposition.h/.cpp  # Composition root (DI wiring)
│   ├── Gateway/
│   │   ├── GatewayService.h/.cpp   # Core receive-validate-ACK-ingest pipeline
│   │   └── GatewayAckHandler.h/.cpp # ACK packet construction + duplicate detection
│   └── Registry/
│       ├── DeviceRegistry.h/.cpp    # Node tracking (RSSI, SNR, packet count, status)
│       ├── NodeRegistry.h/.cpp      # Node record storage
│       ├── NodeRegistryService.h/.cpp # Liveness management
│       ├── NodeLivenessManager.h/.cpp # Liveness timeout logic
│       ├── NodeInfo.h               # Node metadata struct
│       └── NodeStatus.h             # Online/offline status
├── lib/
│   ├── Backend/
│   │   └── BackendIngestionClient.h/.cpp  # Wi-Fi + HTTP POST to backend
│   ├── BoatProtocol/
│   │   ├── PacketParser.h/.cpp      # Protocol v3 packet parser
│   │   ├── PacketValidator.h/.cpp   # Preamble, version, CRC validation
│   │   ├── IPacketParser.h          # Parser interface
│   │   ├── IPacketValidator.h       # Validator interface
│   │   ├── MockPacketParser.h/.cpp  # Compile-time placeholder
│   │   ├── MockPacketValidator.h/.cpp
│   │   └── ProtocolIntegrationTypes.h/.cpp # Shared types (ParsedPacketView, enums)
│   ├── EventBus/
│   │   └── IEventSink.h            # Event interface
│   ├── HAL/
│   │   ├── IGpioPin.h, ISpiBus.h, ISystemClock.h
│   ├── LoRaDriver/
│   │   ├── Sx1278LoRaDriver.h/.cpp  # SX1278 radio adapter
│   │   ├── ILoRaDriver.h           # Radio interface
│   │   └── LoRaTypes.h             # RawRadioPacket struct
│   └── Logger/
│       ├── SerialLogger.h/.cpp      # Serial logging implementation
│       └── ILogger.h               # Logger interface
├── test/
│   ├── protocol_contract_test.cpp   # Host-compilable BoatNode↔Gateway protocol test
│   ├── run_protocol_contract.sh     # Test runner script
│   ├── host/Arduino.h              # Minimal Arduino stub for host compilation
│   └── README.md
├── docs/
│   └── architecture.md             # Architecture overview
├── platformio.ini                   # Build configuration (macOS + Windows environments)
└── CMakeLists.txt
```

## Configuration

Configuration is defined in [`BuildConfig.h`](include/BuildConfig.h):

| Setting | Description | Default |
|---|---|---|
| `wifiSsid` | Wi-Fi network name | `""` (empty — must be set per deployment) |
| `wifiPassword` | Wi-Fi password | `""` (empty) |
| `baseUrl` | Backend API base URL | `""` (empty) |
| `gatewayId` | Gateway node ID | `0xFE` |
| `firmwareVersion` | Firmware version string | `"0.1.0"` |
| `simulationMode` | Simulation flag | `false` |

**Important:** Wi-Fi credentials and backend URL must be configured before deployment. No credentials are shipped in the repository.

## Build

```bash
# Build (default environment: esp32-s3-devkitm-1, macOS)
pio run

# Build for Windows environment
pio run -e windows

# Upload to connected board
pio run --target upload

# Serial monitor (115200 baud)
pio device monitor
```

Platform environments are pre-configured for macOS (`/dev/cu.usbmodem5B5E0268161`) and Windows (`COM10`). Update the port in `platformio.ini` for your setup.

## Testing

### Protocol Contract Test

A host-compilable test validates the BoatNode → Gateway packet round-trip:

```bash
cd test
./run_protocol_contract.sh
```

This test:
1. Creates a BoatNode heartbeat packet using the node firmware's PacketFactory/Serializer
2. Parses it with the Gateway's PacketParser and validates with PacketValidator
3. Verifies all header fields (source, destination, packet ID, type, payload length)
4. Builds a Gateway heartbeat ACK and verifies it parses correctly on the BoatNode side
5. Tests rejection of malformed packets (corrupted CRC, wrong source, wrong type)

### Hardware Validation

Not performed. The firmware builds successfully for the target platform, but physical RF communication between BoatNode and Gateway has not been tested with actual hardware.

## Current Status

| Category | Status |
|---|---|
| LoRa reception pipeline | Implemented in code |
| Packet parsing and validation | Implemented in code |
| Heartbeat ACK transmission | Implemented in code |
| Data ACK transmission | Implemented in code |
| Duplicate detection | Implemented in code |
| Device registry and liveness | Implemented in code |
| Backend HTTP ingestion | Implemented in code |
| Wi-Fi management | Implemented in code |
| Protocol contract test | Implemented and host-compilable |
| Hardware validation | Not performed |
| End-to-end RF test | Not performed |
| Backend integration test | Not performed |
| Firmware version | 0.1.0 |

## Integration

- **BoatNode**: Receives protocol v3 packets from the [BoatNode firmware](../jalari-node-firmware/). Both share compatible packet formats verified by the protocol contract test.
- **Backend**: Sends `HEARTBEAT_RECEIVED` events via HTTP POST to `/api/ingest/gateway` on the [Jalri backend](../website/backend/). The JSON payload includes nodeId, packetId, destination, sequence, packetSize, receivedAtMs, rssi, and snr.
- **Protocol version**: v3 (preamble `0xA5`, 10-byte header with TTL and hop count)

## Future Work

- Replace MockPacketParser/MockPacketValidator with shared `jalri-protocol` library adapters
- MQTT bridge for persistent connectivity
- Command dispatch from backend to BoatNodes
- Health monitoring and diagnostics
- Physical hardware validation and RF testing
