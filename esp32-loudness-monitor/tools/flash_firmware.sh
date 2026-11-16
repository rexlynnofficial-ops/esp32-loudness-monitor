#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
  echo "Usage: $0 /dev/cu.usbmodemXXXX" >&2
  exit 2
fi
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
arduino_cli="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
fqbn="esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,USBMode=hwcdc,CDCOnBoot=cdc,UploadMode=default"

"$project_dir/tools/build_firmware.sh"
exec "$arduino_cli" upload --fqbn "$fqbn" --port "$1" \
  --input-dir "$project_dir/build-firmware" \
  "$project_dir/firmware/LoudnessMonitor"

