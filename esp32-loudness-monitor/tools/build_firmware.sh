#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
arduino_cli="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"

if [ ! -x "$arduino_cli" ]; then
  arduino_cli=$(command -v arduino-cli || true)
fi
if [ -z "$arduino_cli" ] || [ ! -x "$arduino_cli" ]; then
  echo "Arduino CLI was not found. Install Arduino IDE 2 or arduino-cli." >&2
  exit 1
fi

fqbn="esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,USBMode=hwcdc,CDCOnBoot=cdc,UploadMode=default"
exec "$arduino_cli" compile --fqbn "$fqbn" \
  --warnings all \
  --build-path "$project_dir/build-firmware" \
  "$project_dir/firmware/LoudnessMonitor"

