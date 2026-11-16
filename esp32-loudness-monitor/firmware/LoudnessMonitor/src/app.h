#pragma once

#include <array>
#include <memory>
#include <vector>

#include "bsp/board.h"
#include "dsp/loudness_meter.h"
#include "dsp/third_octave.h"
#include "storage/settings.h"
#include "transport/usb_transport.h"
#include "ui/meter_ui.h"

namespace lmv1 {

class App {
 public:
  App();
  void begin();
  void tick();

 private:
  enum class Mode : uint8_t { kMic, kComputer };
  void handle_frame(const protocol::Frame& frame);
  void handle_action(ui::UiAction action);
  void update_microphone();
  void update_ui();
  void send_capabilities();
  void send_status();
  void push_waveform_sample(float sample);

  bsp::Board board_;
  transport::UsbTransport transport_;
  storage::SettingsStore settings_store_;
  storage::SettingsData settings_;
  dsp::LoudnessMeter meter_;
  dsp::ThirdOctaveAnalyzer octave_;
  std::unique_ptr<ui::MeterUi> ui_;
  std::vector<int16_t> mic_stereo_;
  std::vector<float> octave_window_;
  std::array<float, 240> waveform_{};
  std::size_t waveform_cursor_ = 0;
  std::size_t octave_cursor_ = 0;
  Mode mode_ = Mode::kMic;
  ui::Page page_ = ui::Page::kMic;
  bool helper_connected_ = false;
  bool prompt_mode_ = false;
  bool hybrid_ = false;
  float spl_db_ = -INFINITY;
  float spl_peak_db_ = -INFINITY;
  uint8_t input_channels_ = 1;
  uint32_t input_sample_rate_ = 48000;
  uint32_t last_helper_ms_ = 0;
  uint32_t last_ui_ms_ = 0;
  uint32_t last_status_ms_ = 0;
};

}  // namespace lmv1
