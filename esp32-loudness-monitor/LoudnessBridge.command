#!/bin/bash

set -u

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
HELPER_BIN="$PROJECT_DIR/build-host/helper/loudness-bridge"

printf '\nLoudness Monitor Helper\n'
printf '%s\n\n' '========================'

if ! command -v cmake >/dev/null 2>&1; then
  echo "错误：未找到 cmake。请先安装 CMake，然后重新双击此文件。"
  read -r -p "按回车关闭..." _
  exit 1
fi

if [ ! -x "$HELPER_BIN" ]; then
  echo "首次运行：正在构建 Helper..."
  if ! cmake -S "$PROJECT_DIR" -B "$PROJECT_DIR/build-host" \
      -G "Unix Makefiles" -DLMV1_BUILD_HELPER=ON -DLMV1_BUILD_TESTS=OFF || \
     ! cmake --build "$PROJECT_DIR/build-host" --parallel; then
    echo "错误：Helper 构建失败。"
    read -r -p "按回车关闭..." _
    exit 1
  fi
fi

DEVICE_PORT=""
for candidate in /dev/cu.usbmodem*; do
  if [ -e "$candidate" ]; then
    DEVICE_PORT="$candidate"
    break
  fi
done

if [ -z "$DEVICE_PORT" ]; then
  echo "错误：没有找到 ESP32 USB 端口。请连接开发板后重新双击。"
  read -r -p "按回车关闭..." _
  exit 1
fi

echo "设备：$DEVICE_PORT"
echo "正在启动 Computer 模式。按 Ctrl-C 可停止。"
echo

"$HELPER_BIN" --device "$DEVICE_PORT" --select-computer
EXIT_CODE=$?

echo
if [ "$EXIT_CODE" -eq 0 ]; then
  echo "Helper 已停止。"
else
  echo "Helper 已退出，错误码：$EXIT_CODE"
fi
read -r -p "按回车关闭此窗口..." _
exit "$EXIT_CODE"

