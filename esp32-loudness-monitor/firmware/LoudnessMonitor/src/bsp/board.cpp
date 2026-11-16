#include "board.h"

#include <algorithm>

namespace lmv1::bsp {
namespace {
constexpr int kI2cSda = 8;
constexpr int kI2cScl = 7;
constexpr int kBacklight = 6;
constexpr int kSpiMiso = 2;
constexpr int kSpiMosi = 1;
constexpr int kSpiClock = 5;
constexpr int kLcdDc = 3;
constexpr int kI2sMclk = 12;
constexpr int kI2sBclk = 13;
constexpr int kI2sLrclk = 15;
constexpr int kI2sDataOut = 16;
constexpr int kI2sDataIn = 14;
constexpr uint8_t kCodecAddress = 0x18;
}

bool Board::begin() {
  Wire.begin(kI2cSda, kI2cScl);
  Wire.setClock(400000);
  display_ok_ = begin_display();
  touch_ok_ = begin_touch();
  microphone_ok_ = begin_microphone();
  return display_ok_;
}

bool Board::begin_display() {
  if (!io_expander_.begin()) return false;
  io_expander_.pinMode1(1, OUTPUT);
  io_expander_.write1(1, 1);
  delay(10);
  io_expander_.write1(1, 0);
  delay(10);
  io_expander_.write1(1, 1);
  delay(200);

  bus_ = new Arduino_ESP32SPI(kLcdDc, -1, kSpiClock, kSpiMosi, kSpiMiso);
  display_ = new Arduino_ST7796(bus_, -1, 1, true, 320, 480);
  if (display_ == nullptr || !display_->begin()) return false;
  pinMode(kBacklight, OUTPUT);
  digitalWrite(kBacklight, HIGH);
  display_->fillScreen(BLACK);
  return true;
}

bool Board::begin_touch() {
  return touch_.begin(Wire, FT6X36_SLAVE_ADDRESS);
}

bool Board::codec_write(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(kCodecAddress);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission(true) == 0;
}

bool Board::begin_microphone() {
  // 48 kHz / 16-bit I2S slave sequence derived from Waveshare's Apache-2.0
  // ES8311 example and the Everest Semiconductor register map.
  const uint8_t sequence[][2] = {
      {0x00, 0x1f}, {0x00, 0x00}, {0x00, 0x80}, {0x01, 0x3f},
      {0x02, 0x00}, {0x03, 0x10}, {0x04, 0x10}, {0x05, 0x00},
      {0x06, 0x03}, {0x07, 0x00}, {0x08, 0xff}, {0x00, 0x80},
      {0x09, 0x0c}, {0x0a, 0x0c}, {0x0d, 0x01}, {0x0e, 0x02},
      {0x12, 0x00}, {0x13, 0x10}, {0x17, 0xc8}, {0x14, 0x1a},
      {0x1c, 0x6a}, {0x37, 0x08}};
  for (const auto& write : sequence) {
    if (!codec_write(write[0], write[1])) return false;
    if (write[0] == 0x00 && write[1] == 0x1f) delay(20);
  }
  i2s_.setPins(kI2sBclk, kI2sLrclk, kI2sDataOut, kI2sDataIn, kI2sMclk);
  return i2s_.begin(I2S_MODE_STD, 48000, I2S_DATA_BIT_WIDTH_16BIT,
                    I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH);
}

TouchPoint Board::read_touch() {
  TouchPoint result;
  if (!touch_ok_) return result;
  int16_t raw_x[1] = {0};
  int16_t raw_y[1] = {0};
  result.pressed = touch_.getPoint(raw_x, raw_y, 1) > 0;
  if (result.pressed) {
    // Display rotation 1: native 320x480 becomes landscape 480x320.
    result.x = std::max<int16_t>(0, std::min<int16_t>(479, raw_y[0]));
    result.y = std::max<int16_t>(0, std::min<int16_t>(319, 319 - raw_x[0]));
  }
  return result;
}

std::size_t Board::read_microphone(int16_t* stereo_samples, std::size_t frames) {
  if (!microphone_ok_ || stereo_samples == nullptr || frames == 0) return 0;
  const std::size_t requested = frames * 2 * sizeof(int16_t);
  const std::size_t bytes = i2s_.readBytes(reinterpret_cast<char*>(stereo_samples), requested);
  return bytes / (2 * sizeof(int16_t));
}

}  // namespace lmv1::bsp
