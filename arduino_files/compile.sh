#!/usr/bin/env bash
# ============================================================
# Usage: ./compile.sh <file.ino>
# Compile and upload a single .ino file by creating temp sketch folder
# ============================================================

set -e

BOARD="arduino:mbed_nano:nano33ble"
INO_FILE="$1"

if [ -z "$INO_FILE" ]; then
  echo "Usage: $0 <file.ino>"
  exit 1
fi

if [ ! -f "$INO_FILE" ]; then
  echo "Error: file '$INO_FILE' not found"
  exit 1
fi

# Create a temp directory for this sketch
TMP_SKETCH_DIR=$(mktemp -d)

# Copy .ino file into temp dir, rename .ino file to match folder name
SKETCH_NAME="temp_sketch"
mkdir -p "$TMP_SKETCH_DIR/$SKETCH_NAME"
cp "$INO_FILE" "$TMP_SKETCH_DIR/$SKETCH_NAME/$SKETCH_NAME.ino"

# Find Arduino Nano 33 port
PORT=$(arduino-cli board list | grep "Nano 33" | awk '{print $1}' | head -n1)

if [ -z "$PORT" ]; then
  echo "⚠️  No Arduino Nano 33 found! Connect your board and retry."
  rm -rf "$TMP_SKETCH_DIR"
  exit 1
fi

echo "🔧 Compiling $INO_FILE ..."
arduino-cli compile --fqbn "$BOARD" "$TMP_SKETCH_DIR/$SKETCH_NAME"

echo "🚀 Uploading to $PORT ..."
if ! arduino-cli upload -p "$PORT" --fqbn "$BOARD" "$TMP_SKETCH_DIR/$SKETCH_NAME"; then
  echo "⚠️ Upload failed — board might have switched ports, retrying..."
  sleep 2
  NEWPORT=$(arduino-cli board list | grep "Nano 33" | awk '{print $1}' | head -n1)
  if [ -n "$NEWPORT" ]; then
    echo "🔁 Retrying upload on $NEWPORT ..."
    arduino-cli upload -p "$NEWPORT" --fqbn "$BOARD" "$TMP_SKETCH_DIR/$SKETCH_NAME"
  else
    echo "❌ Still no port found. Try double-tapping reset on the board."
    rm -rf "$TMP_SKETCH_DIR"
    exit 1
  fi
fi

echo "✅ Upload complete!"

# Clean up temp folder
rm -rf "$TMP_SKETCH_DIR"
