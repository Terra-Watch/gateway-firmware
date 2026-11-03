# TerraWatch gateway architecture

`FirmwareComposition` builds the object graph once: SX1278 driver, packet parser and validator, serial logger, `GatewayService`, `App`.

Per loop, `App::tick` asks the driver for a packet and hands it to `GatewayService`, which parses, validates, acknowledges (`GatewayAckHandler`), updates the node registry and prints the packet to the serial port. Only `Sx1278LoRaDriver` and the logger touch Arduino APIs.

The wire format is shared with `../node-firmware` (`lib/Protocol`); `test/run_protocol_contract.sh` checks both sides agree.
