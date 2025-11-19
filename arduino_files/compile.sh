#!/usr/bin/env bash
set -e

BOARD="arduino:mbed_nano:nano33ble"

if [ $# -lt 1 ]; then
  echo "Usage: $0 <main.ino> [files_to_include...]"
  exit 1
fi

MAIN_INO="$1"
shift 1

FILES_TO_COPY=("$MAIN_INO" "$@")

TMP_SKETCH_DIR=$(mktemp -d)
SKETCH_NAME="temp_sketch"
SKETCH_PATH="$TMP_SKETCH_DIR/$SKETCH_NAME"

mkdir -p "$SKETCH_PATH"

echo "📁 Creating temporary sketch: $SKETCH_PATH"

# Copy all provided files
for f in "${FILES_TO_COPY[@]}"; do
  if [ -f "$f" ]; then
    echo "➡️  Including: $f"
    cp "$f" "$SKETCH_PATH/"
  else
    echo "⚠️  Warning: file '$f' does not exist, skipping"
  fi
done

# Rename the main INO to match folder
BASENAME=$(basename "$MAIN_INO")
rm "$SKETCH_PATH/$BASENAME"                        # remove original
cp "$MAIN_INO" "$SKETCH_PATH/$SKETCH_NAME.ino"     # rename only once

# Detect board
PORT=$(arduino-cli board list | grep "Nano 33" | awk '{print $1}' | head -n1)

if [ -z "$PORT" ]; then
  echo "⚠️  No Arduino Nano 33 detected!"
  rm -rf "$TMP_SKETCH_DIR"
  exit 1
fi

echo "🔧 Compiling..."
arduino-cli compile --fqbn "$BOARD" "$SKETCH_PATH"

echo "🚀 Uploading to $PORT ..."
arduino-cli upload -p "$PORT" --fqbn "$BOARD" "$SKETCH_PATH"

echo "✅ Upload complete!"

rm -rf "$TMP_SKETCH_DIR"
