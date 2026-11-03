#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
NODE_DIR="$ROOT_DIR/../node-firmware"
BUILD_DIR="${TMPDIR:-/tmp}/terrawatch-protocol-contract"
mkdir -p "$BUILD_DIR"

c++ -std=c++17 \
  -I"$ROOT_DIR/test/host" \
  -I"$ROOT_DIR/lib/Protocol" \
  -I"$ROOT_DIR/lib/LoRaDriver" \
  -I"$ROOT_DIR/src/Gateway" \
  -I"$NODE_DIR/include" \
  -I"$NODE_DIR/lib/Protocol/src" \
  -I"$NODE_DIR/lib/Node/src" \
  -I"$NODE_DIR/lib/PacketFactory/src" \
  -I"$NODE_DIR/lib/Utils/src" \
  "$ROOT_DIR/test/protocol_contract_test.cpp" \
  "$NODE_DIR/lib/Protocol/src/Packet.cpp" \
  "$NODE_DIR/lib/Protocol/src/Parser.cpp" \
  "$NODE_DIR/lib/Protocol/src/Serializer.cpp" \
  "$NODE_DIR/lib/Protocol/src/Validator.cpp" \
  "$NODE_DIR/lib/Node/src/NodeManager.cpp" \
  "$NODE_DIR/lib/PacketFactory/src/PacketFactory.cpp" \
  "$NODE_DIR/lib/Utils/src/Crc16.cpp" \
  "$ROOT_DIR/lib/Protocol/ProtocolIntegrationTypes.cpp" \
  "$ROOT_DIR/lib/Protocol/PacketParser.cpp" \
  "$ROOT_DIR/lib/Protocol/PacketValidator.cpp" \
  "$ROOT_DIR/src/Gateway/GatewayAckHandler.cpp" \
  -o "$BUILD_DIR/protocol_contract_test"

"$BUILD_DIR/protocol_contract_test"
echo "software protocol contract: PASS"
