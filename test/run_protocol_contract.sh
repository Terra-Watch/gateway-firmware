#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
NODE_DIR="/Users/kireeti/Desktop/Projects/RESQ/jalari-node-firmware"
BUILD_DIR="${TMPDIR:-/tmp}/jalri-protocol-contract"
mkdir -p "$BUILD_DIR"

c++ -std=c++17 \
  -I"$ROOT_DIR/test/host" \
  -I"$ROOT_DIR/lib/BoatProtocol" \
  -I"$ROOT_DIR/lib/LoRaDriver" \
  -I"$ROOT_DIR/src/Gateway" \
  -I"$NODE_DIR/include" \
  -I"$NODE_DIR/lib/BoatProtocol/src" \
  -I"$NODE_DIR/lib/Node/src" \
  -I"$NODE_DIR/lib/PacketFactory/src" \
  -I"$NODE_DIR/lib/Utils/src" \
  "$ROOT_DIR/test/protocol_contract_test.cpp" \
  "$NODE_DIR/lib/BoatProtocol/src/Packet.cpp" \
  "$NODE_DIR/lib/BoatProtocol/src/Parser.cpp" \
  "$NODE_DIR/lib/BoatProtocol/src/Serializer.cpp" \
  "$NODE_DIR/lib/BoatProtocol/src/Validator.cpp" \
  "$NODE_DIR/lib/Node/src/NodeManager.cpp" \
  "$NODE_DIR/lib/PacketFactory/src/PacketFactory.cpp" \
  "$NODE_DIR/lib/Utils/src/Crc16.cpp" \
  "$ROOT_DIR/lib/BoatProtocol/ProtocolIntegrationTypes.cpp" \
  "$ROOT_DIR/lib/BoatProtocol/PacketParser.cpp" \
  "$ROOT_DIR/lib/BoatProtocol/PacketValidator.cpp" \
  "$ROOT_DIR/src/Gateway/GatewayAckHandler.cpp" \
  -o "$BUILD_DIR/protocol_contract_test"

"$BUILD_DIR/protocol_contract_test"
echo "software protocol contract: PASS"
