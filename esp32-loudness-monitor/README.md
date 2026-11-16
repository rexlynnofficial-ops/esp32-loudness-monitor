# Loudness Monitor V1

Waveshare ESP32-S3 Touch LCD 3.5 的桌面响度表工程。仓库包含可由 Arduino IDE/CLI 编译和烧录的固件、macOS Core Audio Helper、Windows WASAPI 原型、版本化 Hybrid 协议、主机测试与真实硬件验证记录。

## 当前状态

- 固件已在真实 ESP32-S3（8 MB PSRAM）上编译、烧录并完成 USB 回连。
- macOS Core Audio 全局输出捕获、USB 协议协商和 ESP32 DSP 回传已端到端通过。
- LCD、触摸控制器和 ES8311/I2S 初始化通过；麦克风实时 SPL 数据已观察。
- Windows 源码已提供，但未在 Windows 上构建；实体屏幕视觉与物理触摸点击仍需人工验收。

完整证据见 [docs/VALIDATION.md](docs/VALIDATION.md)，限制见下文。

## 目录

```text
firmware/LoudnessMonitor/  Arduino 固件
helper/                    macOS Core Audio / Windows WASAPI Helper
protocol/PROTOCOL.md       ESP32 ↔ Helper 二进制协议
tests/                     主机 DSP 与协议测试
tools/                     构建、烧录和总体验证脚本
docs/decisions/            架构决策记录
```

## Arduino 固件

### 已验证版本

- Arduino IDE 2
- ESP32 Arduino Core 3.2.0
- Arduino_GFX 1.5.5
- TCA9554 0.1.2
- SensorLib 0.3.1

Waveshare 官方还指定 LVGL 8.4.0；本固件为降低刷新负担直接使用 Arduino_GFX 绘制，不依赖 LVGL 运行时。精确依赖与本机变更记录见 [docs/DEPENDENCIES.md](docs/DEPENDENCIES.md)。

### Arduino IDE 设置

打开 `firmware/LoudnessMonitor/LoudnessMonitor.ino`，选择：

- Board: `ESP32S3 Dev Module`
- Flash Size: `16MB (128Mb)`
- PSRAM: `OPI PSRAM`
- Partition Scheme: `16M Flash (3MB APP/9.9MB FATFS)`
- USB Mode: `Hardware CDC and JTAG`
- USB CDC On Boot: `Enabled`
- Upload Mode: `UART0 / Hardware CDC`
- CPU Frequency: `240MHz`

Arduino IDE 点击 Verify/Upload 即可。命令行等价步骤：

```sh
./tools/build_firmware.sh
./tools/flash_firmware.sh /dev/cu.usbmodem14101
```

烧录脚本会覆盖板上旧固件。端口名称以 Arduino IDE 实际显示为准。

## Helper

### macOS 构建与运行

Core Audio Process Tap 需要 macOS 14.2 或更高版本。首次运行若系统询问“系统音频录制”，应在 System Settings → Privacy & Security 中明确授权；程序不会绕过权限，也不会静默安装后台服务。

```sh
./tools/build_helper.sh
./build-helper/helper/loudness-bridge --device /dev/cu.usbmodem14101
```

也可以直接双击项目根目录的 `LoudnessBridge.command`。它会自动寻找 ESP32 端口；如果 Helper 尚未构建，会先自动构建，然后启动 Computer 模式。首次运行可能需要在 macOS 的“系统音频录制”权限中允许终端/Helper。

设备每次检测到 Helper 都会显示模式选择。测试或无障碍自动化可显式选择：

```sh
./build-helper/helper/loudness-bridge \
  --device /dev/cu.usbmodem14101 --select-computer
```

多声道设备流可请求保留声道数：

```sh
./build-helper/helper/loudness-bridge \
  --device /dev/cu.usbmodem14101 --select-computer \
  --preserve-channels --device-uid 'COREAUDIO_DEVICE_UID' --stream 0
```

默认全局 Tap 混为立体声且 `CATapUnmuted`，不会静音或替换现有监听输出。

### Windows

在 Visual Studio Developer Command Prompt 中：

```powershell
cmake -S . -B build-win
cmake --build build-win --config Release
.\build-win\helper\Release\loudness-bridge.exe --device COM3 --select-computer
```

Windows 后端使用默认 render endpoint、WASAPI shared-mode loopback 和 `IAudioCaptureClient`。该路径没有在本次 macOS 开发机上运行验证。

## UI 与操作

- Main：LUFS-I 大数字、M/S、LRA、True Peak、Max TP、Target 偏差和 60 秒曲线。
- Mic SPL：当前/Peak SPL、校准状态与 94 dB 校准按钮。
- Waveform：显示 240 点连续时域波形和中心基线；页面底部 `H− / H+` 按 25% 步长调节高度（25%–400%），并保存到 NVS。1/3-octave 分析仍保留在后台，页面不再使用柱状图。
- USB / Helper：独立显示 USB CDC、Helper 连接状态、Local/Hybrid 引擎、Computer/Mic 模式，并可直接触摸切换模式。
- Device Info：输入采样率、声道、Local/Hybrid、Bridge 与 Mic 状态。
- 底部按钮：Reset、Target ±0.1 LU、Next。

校准时将 94 dB / 1 kHz 声学校准器装到麦克风上，等读数稳定后按 `CAL 94dB`。偏移与 Target 保存在 ESP32 NVS。

## 架构

```text
macOS Core Audio Tap / Windows WASAPI Loopback
                    │
        stereo PCM  │  >2ch K-weighted observation
                    ▼
          CRC-protected LMV1 protocol
                    │ USB CDC
                    ▼
          ESP32 loudness/session engine
                    │
          display + touch + NVS history
```

Stereo 模式中 Helper 只负责系统捕获、重采样、量化和传输，LUFS/门限/LRA/True Peak 在 ESP32。Hybrid 模式中 Helper 做多声道 K-weighting/LFE 排除与块观测，ESP32 仍拥有 M/S/I、LRA、Target、Reset 和 UI。详见 [ADR-0001](docs/decisions/0001-arduino-usb-transport.md) 与 [ADR-0002](docs/decisions/0002-hybrid-processing-boundary.md)。

## 测试

```sh
./tools/verify_all.sh
```

它会构建 Helper/主机核心，运行协议、DSP、UI、Helper 自测，然后编译 Arduino 固件。烧录和物理测试需要显式运行烧录脚本。

## 已知限制

1. Arduino Core 3.2.0 预编译 TinyUSB 库关闭了 UAC，所以这个 Arduino V1 使用 CDC 音频数据传输；系统总线 Computer 模式需要 Helper。直接 UAC 变体需自定义 Arduino Core 或 ESP-IDF `esp_device_uac`。
2. DSP 和协议是工程级实现，尚未用 ITU/EBU 官方一致性向量或独立实验室认证；4x True Peak FIR 需要在专业交付前做标准符合性验证。
3. Hybrid Helper 当前把 sample peak 填入协议的 true-peak 观察字段；多声道 Helper 端的合规 true-peak oversampler 是下一项明确工作，不会在 UI 中伪称已认证。
4. 1/3 octave 页面使用 Goertzel 中心频点框架，不是 IEC 61260 Class 1/2 滤波器组。
5. 板载麦克风默认未校准；未使用 94 dB 校准器前，SPL 只作相对参考。
6. macOS preserved-channel 需要实际多声道设备 UID/stream；本次只实测全局立体声 mixdown。
7. Windows 和实体触摸/视觉布局仍需对应平台/人工验收，详见验证矩阵。
8. 短时间 macOS Helper 捕获可保持 0 丢块，但长时间连续捕获仍可能出现 CDC 背压丢块；在此问题关闭前不应宣称持续无损传输。
9. Computer Input Trim 默认 0 dB；如果 DAW 内部表与系统输出 Tap 存在固定电平差，可在 USB / Helper 页面按 0.5 dB 调整并保存。

## 官方依据

- [Waveshare Arduino 依赖与板级示例](https://docs.waveshare.com/ESP32-S3-Touch-LCD-3.5/Arduino)
- [Espressif Arduino I2S API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/i2s.html)
- [Espressif USB CDC/DFU](https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/cdc_dfu_flash.html)
- [Espressif USB Audio Device](https://docs.espressif.com/projects/esp-iot-solution/en/latest/usb/usb_device/usb_device_uac.html)
- [Apple Core Audio Process Tap](https://developer.apple.com/documentation/coreaudio/capturing-system-audio-with-core-audio-taps)
- [Microsoft WASAPI Loopback](https://learn.microsoft.com/en-us/windows/win32/coreaudio/loopback-recording)
