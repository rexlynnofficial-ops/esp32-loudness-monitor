#pragma once

#include <Arduino_GFX_Library.h>

#include <array>
#include <deque>

#include "../bsp/board.h"
#include "../dsp/loudness_meter.h"
#include "../dsp/third_octave.h"
#include "../storage/settings.h"
#include "render_invalidation.h"
#include "waveform_path.h"

namespace lmv1::ui {

enum class Page : uint8_t { kMain, kMic, kWaveform, kUsb, kSettings };
enum class UiAction : uint8_t {
  kNone,
  kReset,
  kTargetDown,
  kTargetUp,
  kNextPage,
  kSelectComputer,
  kSelectMic,
  kCalibrate,
  kWaveformDown,
  kWaveformUp,
  kComputerTrimDown,
  kComputerTrimUp,
};

struct UiState {
  lmv1::dsp::MeterSnapshot meter{};
  const std::deque<float>* history = nullptr;
  const std::array<float, lmv1::dsp::ThirdOctaveAnalyzer::kBandCount>* octave = nullptr;
  const std::array<float, 240>* waveform = nullptr;
  std::size_t waveform_cursor = 0;
  lmv1::storage::SettingsData settings{};
  Page page = Page::kMain;
  float spl_db = -INFINITY;
  float spl_peak_db = -INFINITY;
  bool helper_connected = false;
  bool hybrid = false;
  bool prompt_mode = false;
  bool mic_ok = false;
  uint8_t channels = 2;
  uint32_t sample_rate = 48000;
  const char* status = "MIC";
};

class MeterUi {
 public:
  explicit MeterUi(Arduino_GFX& display) : display_(display) {}
  void begin();
  void render(const UiState& state);
  UiAction handle_touch(const lmv1::bsp::TouchPoint& point);

 private:
  void draw_header(const UiState& state, const char* title);
  void draw_main(const UiState& state);
  void draw_mic(const UiState& state);
  void draw_waveform(const UiState& state);
  void draw_usb(const UiState& state);
  void draw_settings(const UiState& state);
  void draw_button(int x, int y, int w, int h, const char* label, uint16_t color);
  void draw_value(float value, const char* suffix, int x, int y, int size,
                  uint16_t color);
  void draw_mode_prompt();

  Arduino_GFX& display_;
  Page page_ = Page::kMain;
  bool touch_latched_ = false;
  bool prompt_mode_ = false;
  bool full_redraw_ = true;
  RenderInvalidation invalidation_;
  uint32_t last_graph_refresh_ms_ = 0;
  uint32_t last_waveform_refresh_ms_ = 0;
};

}  // namespace lmv1::ui
