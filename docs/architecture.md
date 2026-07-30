# Jalri Gateway Firmware Architecture

This repository contains firmware for the Jalri Maritime Edge Gateway.

The Gateway receive path depends only on protocol integration interfaces:

```text
SX1278 -> LoRaDriver -> Raw Bytes -> IPacketParser -> ParsedPacketView -> IPacketValidator -> GatewayService -> Logger
```

The application layer depends on interfaces and contains no Arduino APIs.
Platform-specific work remains behind HAL and driver boundaries.

`MockPacketParser` and `MockPacketValidator` are compile-time placeholders only.
They must be replaced by adapters over the future shared `jalri-protocol` library.

Future modules such as `DeviceRegistry`, `PacketRouter`, `MQTTBridge`,
`CommandDispatcher`, and `HealthMonitor` should be attached through constructor
injection in `FirmwareComposition` without changing `App`.
