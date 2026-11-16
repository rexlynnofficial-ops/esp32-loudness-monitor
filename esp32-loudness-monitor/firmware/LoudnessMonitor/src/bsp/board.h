#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <ESP_I2S.h>
#include <TCA9554.h>
#include <TouchDrvFT6X36.hpp>
#include <Wire.h>

#include <cstddef>
#include <cstdint>

namespace lmv1::bsp {

struct TouchPoint {
  int16_t x = 0;
  int16_t y = 0;
  bool pressed = false;
};

class Board {
 public:
  bool begin();
  bool display_ok() const { return display_ok_; }
  bool touch_ok() const { return touch_ok_; }
  bool microphone_ok() const { return microphone_ok_; }
  Arduino_GFX& display() { return *display_; }
  TouchPoint read_touch();
  std::size_t read_microphone(int16_t* stereo_samples, std::size_t frames);

 private:
  bool begin_display();
  bool begin_touch();
  bool begin_microphone();
  bool codec_write(uint8_t reg, uint8_t value);

  TCA9554 io_expander_{0x20};
  TouchDrvFT6X36 touch_;
  I2SClass i2s_;
  Arduino_DataBus* bus_ = nullptr;
  Arduino_GFX* display_ = nullptr;
  bool display_ok_ = false;
  bool touch_ok_ = false;
  bool microphone_ok_ = false;
};

}  // namespace lmv1::bsp

