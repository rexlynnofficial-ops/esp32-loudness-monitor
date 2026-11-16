#include "meter_ui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace lmv1::ui {
namespace {
constexpr uint16_t kBackground = 0x0841;
constexpr uint16_t kPanel = 0x10a2;
constexpr uint16_t kText = 0xef7d;
constexpr uint16_t kMuted = 0x7bef;
constexpr uint16_t kAccent = 0x4fd3;
constexpr uint16_t kWarning = 0xfd20;
constexpr uint16_t kDanger = 0xf986;

const char* preset_name(lmv1::dsp::MeterPreset preset) {
  switch (preset) {
    case lmv1::dsp::MeterPreset::kEbuR128: return "EBU R128";
    case lmv1::dsp::MeterPreset::kAtscA85: return "ATSC A/85";
    case lmv1::dsp::MeterPreset::kStreaming: return "STREAM";
    case lmv1::dsp::MeterPreset::kCustom: return "CUSTOM";
  }
  return "CUSTOM";
}
}

void MeterUi::begin() {
  display_.fillScreen(kBackground);
  display_.setTextWrap(false);
}

void MeterUi::draw_button(int x, int y, int w, int h, const char* label,
                          uint16_t color) {
  display_.fillRoundRect(x, y, w, h, 5, color);
  display_.setTextColor(kBackground);
  display_.setTextSize(1);
  display_.setCursor(x + 8, y + h / 2 - 4);
  display_.print(label);
}

void MeterUi::draw_header(const UiState& state, const char* title) {
  display_.fillRect(0, 0, 480, 30, kPanel);
  display_.setTextSize(2);
  display_.setTextColor(kText);
  display_.setCursor(10, 8);
  display_.print(title);
  display_.setTextSize(1);
  display_.setTextColor(state.helper_connected ? kAccent : kMuted);
  display_.setCursor(345, 10);
  display_.print(state.status);
  display_.print(state.hybrid ? " / HYBRID" : " / LOCAL");
}

void MeterUi::draw_value(float value, const char* suffix, int x, int y,
                         int size, uint16_t color) {
  char text[32];
  if (std::isfinite(value)) std::snprintf(text, sizeof(text), "%.1f", value);
  else std::snprintf(text, sizeof(text), "--.-");
  display_.setTextColor(color);
  display_.setTextSize(size);
  display_.setCursor(x, y);
  display_.print(text);
  display_.setTextSize(1);
  display_.print(suffix);
}

void MeterUi::draw_main(const UiState& state) {
  draw_header(state, "LOUDNESS");
  display_.fillRect(0, 40, 278, 80, kBackground);
  const float delta = state.meter.integrated_lufs - state.settings.target_lufs;
  const uint16_t main_color = !std::isfinite(delta) ? kMuted
      : std::fabs(delta) <= 1.0F ? kAccent
      : std::fabs(delta) <= 2.0F ? kWarning : kDanger;
  draw_value(state.meter.integrated_lufs, " LUFS-I", 18, 48, 5, main_color);

  if (full_redraw_) display_.fillRoundRect(280, 42, 190, 94, 5, kPanel);
  display_.fillRect(288, 50, 174, 76, kPanel);
  draw_value(state.meter.momentary_lufs, " M", 294, 56, 2, kText);
  draw_value(state.meter.short_term_lufs, " S", 294, 84, 2, kText);
  draw_value(state.meter.true_peak_dbtp, " dBTP", 294, 112, 2,
             state.meter.true_peak_dbtp > -1.0F ? kDanger : kText);

  const bool refresh_graph = full_redraw_ || millis() - last_graph_refresh_ms_ >= 100;
  const int gx = 12, gy = 150, gw = 456, gh = 94;
  if (refresh_graph) display_.fillRoundRect(gx, gy, gw, gh, 4, kPanel);
  if (refresh_graph) display_.drawFastHLine(gx + 1, gy + gh / 2, gw - 2, 0x31a6);
  if (refresh_graph && state.history != nullptr && state.history->size() > 1) {
    const std::size_t count = state.history->size();
    const std::size_t plot_count = std::min<std::size_t>(600, count);
    int previous_x = gx + 2;
    float previous_value = (*state.history)[0];
    int previous_y = gy + gh - 3 - static_cast<int>(
        std::max(0.0F, std::min(1.0F, (previous_value + 60.0F) / 60.0F)) * (gh - 6));
    for (std::size_t i = 1; i < plot_count; ++i) {
      const int x = gx + 2 + static_cast<int>(i * (gw - 4) /
                                               std::max<std::size_t>(1, plot_count - 1));
      const std::size_t source_index = plot_count == 1 ? 0 :
          std::min<std::size_t>(count - 1, i * (count - 1) / (plot_count - 1));
      const float value = (*state.history)[source_index];
      const int y = std::isfinite(value)
          ? gy + gh - 3 - static_cast<int>(
                std::max(0.0F, std::min(1.0F, (value + 60.0F) / 60.0F)) * (gh - 6))
          : gy + gh - 3;
      display_.drawLine(previous_x, previous_y, x, y, kAccent);
      previous_x = x;
      previous_y = y;
    }
    last_graph_refresh_ms_ = millis();
  }

  display_.fillRect(0, 244, 480, 32, kBackground);
  display_.setTextColor(kMuted);
  display_.setTextSize(1);
  display_.setCursor(14, 252);
  display_.printf("TARGET %.1f  %s   LRA %.1f   MAX TP %.1f",
                  state.settings.target_lufs, preset_name(state.settings.preset),
                  state.meter.loudness_range_lu, state.meter.max_true_peak_dbtp);
  if (full_redraw_) {
    draw_button(12, 280, 80, 30, "RESET", kDanger);
    draw_button(105, 280, 58, 30, "-0.1", kMuted);
    draw_button(174, 280, 58, 30, "+0.1", kAccent);
    draw_button(374, 280, 94, 30, "NEXT", kText);
  }
}

void MeterUi::draw_mic(const UiState& state) {
  draw_header(state, "MIC SPL");
  if (full_redraw_) {
    display_.fillRect(20, 50, 445, 220, kBackground);
  } else {
    display_.fillRect(30, 65, 330, 62, kBackground);
    display_.fillRect(35, 140, 190, 30, kBackground);
    display_.fillRect(35, 200, 430, 44, kBackground);
  }
  draw_value(state.spl_db, " dB SPL", 35, 70, 5,
             state.settings.spl_calibrated ? kAccent : kWarning);
  draw_value(state.spl_peak_db, " PEAK", 42, 145, 3, kText);
  display_.setTextSize(2);
  display_.setTextColor(state.settings.spl_calibrated ? kAccent : kWarning);
  display_.setCursor(42, 205);
  display_.print(state.settings.spl_calibrated ? "CALIBRATED" : "UNCALIBRATED");
  display_.setTextSize(1);
  display_.setTextColor(kMuted);
  display_.setCursor(42, 234);
  display_.printf("Offset %.1f dB  /  ES8311 onboard microphone",
                  state.settings.spl_offset_db);
  if (full_redraw_) {
    draw_button(12, 280, 100, 30, "CAL 94dB", kWarning);
    draw_button(125, 280, 120, 30, "RESET PEAK", kDanger);
    draw_button(374, 280, 94, 30, "NEXT", kText);
  }
}

void MeterUi::draw_waveform(const UiState& state) {
  draw_header(state, "WAVEFORM");
  const bool refresh_waveform = full_redraw_ || millis() - last_waveform_refresh_ms_ >= 100;
  if (!refresh_waveform) {
    if (full_redraw_) last_waveform_refresh_ms_ = millis();
    return;
  }
  display_.fillRect(12, 44, 456, 220, kPanel);
  display_.drawFastHLine(16, 154, 448, 0x31a6);
  display_.drawFastHLine(16, 110, 448, 0x18c3);
  display_.drawFastHLine(16, 198, 448, 0x18c3);
  display_.drawFastVLine(240, 54, 200, 0x18c3);
  display_.setTextSize(1);
  display_.setTextColor(kMuted);
  display_.setCursor(20, 56);
  display_.print("+1.0 FS / 0 dBFS");
  display_.setCursor(20, 148);
  display_.print("0 FS");
  display_.setCursor(20, 204);
  display_.print("-1.0 FS");
  if (state.waveform != nullptr) {
    int previous_x = 16;
    const std::size_t start = state.waveform_cursor % state.waveform->size();
    int previous_y = waveform_y((*state.waveform)[start] * state.settings.waveform_scale);
    for (std::size_t i = 1; i < state.waveform->size(); ++i) {
      const int x = 16 + static_cast<int>(i * 448 / (state.waveform->size() - 1));
      const int y = waveform_y((*state.waveform)[(start + i) % state.waveform->size()] *
                               state.settings.waveform_scale);
      display_.drawLine(previous_x, previous_y, x, y, kAccent);
      previous_x = x;
      previous_y = y;
    }
  }
  display_.setCursor(15, 268);
  display_.printf("TIME DOMAIN / FS   HEIGHT %.0f%%", state.settings.waveform_scale * 100.0F);
  last_waveform_refresh_ms_ = millis();
  if (full_redraw_) {
    draw_button(12, 280, 68, 30, "H-", kMuted);
    draw_button(90, 280, 68, 30, "H+", kAccent);
    draw_button(374, 280, 94, 30, "NEXT", kText);
  }
}

void MeterUi::draw_usb(const UiState& state) {
  draw_header(state, "USB / HELPER");
  if (full_redraw_) display_.fillRect(0, 30, 480, 242, kBackground);
  display_.setTextColor(kText);
  display_.setTextSize(2);
  display_.setCursor(30, 56);
  display_.print("Transport  USB CDC");
  display_.setCursor(30, 92);
  display_.printf("Helper     %s", state.helper_connected ? "CONNECTED" : "WAITING");
  display_.setCursor(30, 128);
  display_.printf("Engine     %s", state.hybrid ? "HYBRID" : "LOCAL DSP");
  display_.setCursor(30, 164);
  display_.printf("Mode       %s", state.status);
  display_.setCursor(30, 196);
  display_.printf("Input Trim %+.1f dB", state.settings.computer_trim_db);
  display_.setTextSize(1);
  display_.setTextColor(kMuted);
  display_.setCursor(30, 222);
  display_.print("macOS: Core Audio Tap   Windows: WASAPI Loopback");
  display_.setCursor(30, 238);
  display_.print("Helper source: helper/  /  mode is selected per USB connection");
  if (full_redraw_) {
    draw_button(30, 252, 68, 28, "TRIM-", kMuted);
    draw_button(105, 252, 68, 28, "TRIM+", kAccent);
    draw_button(185, 252, 140, 28, "MIC", kWarning);
    draw_button(330, 252, 140, 28, "COMPUTER", kAccent);
    draw_button(374, 280, 94, 30, "NEXT", kText);
  }
}

void MeterUi::draw_settings(const UiState& state) {
  draw_header(state, "DEVICE INFO");
  if (full_redraw_) display_.fillRect(0, 30, 480, 242, kBackground);
  display_.setTextColor(kText);
  display_.setTextSize(2);
  display_.setCursor(30, 60);
  display_.printf("Input   %lu Hz / %u ch", static_cast<unsigned long>(state.sample_rate),
                  state.channels);
  display_.setCursor(30, 100);
  display_.printf("Engine  %s", state.hybrid ? "HYBRID" : "LOCAL DSP");
  display_.setCursor(30, 140);
  display_.printf("Bridge  %s", state.helper_connected ? "CONNECTED" : "WAITING");
  display_.setCursor(30, 180);
  display_.printf("Mic     %s", state.mic_ok ? "READY" : "ERROR");
  display_.setTextSize(1);
  display_.setTextColor(kMuted);
  display_.setCursor(30, 230);
  display_.print("Firmware 0.1.0  /  Protocol 1.0  /  Arduino Core 3.2");
  if (full_redraw_) draw_button(374, 280, 94, 30, "NEXT", kText);
}

void MeterUi::draw_mode_prompt() {
  display_.fillRoundRect(55, 58, 370, 205, 8, kPanel);
  display_.drawRoundRect(55, 58, 370, 205, 8, kAccent);
  display_.setTextSize(2);
  display_.setTextColor(kText);
  display_.setCursor(105, 82);
  display_.print("USB BRIDGE CONNECTED");
  display_.setTextSize(1);
  display_.setTextColor(kMuted);
  display_.setCursor(97, 120);
  display_.print("Choose mode for this connection");
  draw_button(90, 160, 300, 34, "MONITOR COMPUTER AUDIO", kAccent);
  draw_button(90, 210, 300, 34, "POWER ONLY / MICROPHONE", kWarning);
}

void MeterUi::render(const UiState& state) {
  page_ = state.page;
  prompt_mode_ = state.prompt_mode;
  full_redraw_ = invalidation_.needs_full_clear(
      static_cast<uint8_t>(page_), prompt_mode_);
  if (full_redraw_) display_.fillScreen(kBackground);
  switch (page_) {
    case Page::kMain: draw_main(state); break;
    case Page::kMic: draw_mic(state); break;
    case Page::kWaveform: draw_waveform(state); break;
    case Page::kUsb: draw_usb(state); break;
    case Page::kSettings: draw_settings(state); break;
  }
  if (prompt_mode_) draw_mode_prompt();
}

UiAction MeterUi::handle_touch(const lmv1::bsp::TouchPoint& point) {
  if (!point.pressed) {
    touch_latched_ = false;
    return UiAction::kNone;
  }
  if (touch_latched_) return UiAction::kNone;
  touch_latched_ = true;
  if (prompt_mode_) {
    if (point.x >= 90 && point.x <= 390 && point.y >= 150 && point.y <= 200)
      return UiAction::kSelectComputer;
    if (point.x >= 90 && point.x <= 390 && point.y >= 205 && point.y <= 250)
      return UiAction::kSelectMic;
    return UiAction::kNone;
  }
  if (point.y >= 272 && point.x >= 365) return UiAction::kNextPage;
  if (page_ == Page::kUsb && point.y >= 245 && point.y <= 285) {
    if (point.x >= 20 && point.x < 100) return UiAction::kComputerTrimDown;
    if (point.x >= 100 && point.x < 180) return UiAction::kComputerTrimUp;
    if (point.x >= 180 && point.x < 325) return UiAction::kSelectMic;
    if (point.x >= 325 && point.x < 475) return UiAction::kSelectComputer;
  }
  if (page_ == Page::kWaveform && point.y >= 272) {
    if (point.x < 85) return UiAction::kWaveformDown;
    if (point.x >= 85 && point.x < 175) return UiAction::kWaveformUp;
  }
  if (page_ == Page::kMain && point.y >= 272) {
    if (point.x < 100) return UiAction::kReset;
    if (point.x >= 100 && point.x < 170) return UiAction::kTargetDown;
    if (point.x >= 170 && point.x < 245) return UiAction::kTargetUp;
  }
  if (page_ == Page::kMic && point.y >= 272 && point.x < 125)
    return UiAction::kCalibrate;
  if (page_ == Page::kMic && point.y >= 272 && point.x >= 120 && point.x < 255)
    return UiAction::kReset;
  return UiAction::kNone;
}

}  // namespace lmv1::ui
