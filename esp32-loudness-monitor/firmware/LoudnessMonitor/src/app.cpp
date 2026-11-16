#include "app.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace lmv1 {
namespace {
float read_float_le(const uint8_t* bytes) {
  uint32_t bits = protocol::read_le<uint32_t>(bytes);
  float value = 0.0F;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

void append_float_le(std::vector<uint8_t>& output, float value) {
  uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  protocol::append_le<uint32_t>(output, bits);
}
}

App::App()
    : meter_({48000, 2, 30.0F}),
      octave_(48000),
      mic_stereo_(960),
      octave_window_(4800, 0.0F) {
  waveform_.fill(0.0F);
}

void App::begin() {
  transport_.begin();
  settings_ = settings_store_.load();
  meter_.set_preset(settings_.preset);
  meter_.set_target(settings_.target_lufs);
  meter_.set_input_trim_db(settings_.computer_trim_db);

  if (board_.begin() && board_.display_ok()) {
    ui_ = std::make_unique<ui::MeterUi>(board_.display());
    ui_->begin();
    update_ui();
  }
}

void App::tick() {
  std::vector<protocol::Frame> frames;
  transport_.poll(frames);
  for (const auto& frame : frames) handle_frame(frame);

  if (helper_connected_ && millis() - last_helper_ms_ > 2500) {
    helper_connected_ = false;
    prompt_mode_ = false;
    if (mode_ == Mode::kComputer) {
      mode_ = Mode::kMic;
      page_ = ui::Page::kMic;
    }
  }
  if (mode_ == Mode::kMic) update_microphone();
  if (helper_connected_ && millis() - last_status_ms_ >= 500) send_status();

  if (ui_) {
    handle_action(ui_->handle_touch(board_.read_touch()));
    if (millis() - last_ui_ms_ >= 33) update_ui();
  }
  delay(1);
}

void App::handle_frame(const protocol::Frame& frame) {
  last_helper_ms_ = millis();
  if (frame.type == protocol::MessageType::kHello) {
    if (!helper_connected_) prompt_mode_ = true;
    helper_connected_ = true;
    send_capabilities();
    return;
  }
  if (frame.type == protocol::MessageType::kPing) {
    transport_.send(protocol::MessageType::kPong, frame.payload);
    return;
  }
  if (frame.type == protocol::MessageType::kControl &&
      frame.payload.size() == 1 &&
      (frame.payload[0] == 1 || frame.payload[0] == 2)) {
    mode_ = frame.payload[0] == 1 ? Mode::kComputer : Mode::kMic;
    page_ = mode_ == Mode::kComputer ? ui::Page::kMain : ui::Page::kMic;
    prompt_mode_ = false;
    if (mode_ == Mode::kComputer) meter_.reset();
    return;
  }
  if (mode_ != Mode::kComputer) return;

  if (frame.type == protocol::MessageType::kPcmS16 && frame.payload.size() >= 8) {
    const uint32_t sample_rate = protocol::read_le<uint32_t>(frame.payload.data());
    const uint8_t channels = frame.payload[4];
    const uint16_t sample_frames = protocol::read_le<uint16_t>(&frame.payload[6]);
    const std::size_t pcm_bytes = static_cast<std::size_t>(sample_frames) * channels * 2;
    if (sample_rate != 48000 || (channels != 1 && channels != 2) ||
        frame.payload.size() != 8 + pcm_bytes) return;
    input_sample_rate_ = sample_rate;
    input_channels_ = channels;
    hybrid_ = false;
    const int16_t* pcm = reinterpret_cast<const int16_t*>(&frame.payload[8]);
    if (channels == 2) {
      meter_.process_interleaved_s16(pcm, sample_frames);
    } else {
      std::vector<int16_t> stereo(static_cast<std::size_t>(sample_frames) * 2);
      for (uint16_t i = 0; i < sample_frames; ++i) {
        stereo[i * 2] = stereo[i * 2 + 1] = pcm[i];
      }
      meter_.process_interleaved_s16(stereo.data(), sample_frames);
    }
    const std::size_t stride = std::max<std::size_t>(1, sample_frames / 240);
    for (std::size_t i = 0; i < sample_frames; i += stride) {
      const float left = pcm[i * channels] / 32768.0F;
      const float right = channels == 2 ? pcm[i * channels + 1] / 32768.0F : left;
      push_waveform_sample((left + right) * 0.5F);
    }
  } else if (frame.type == protocol::MessageType::kObservation &&
             frame.payload.size() == 22) {
    const uint32_t sample_rate = protocol::read_le<uint32_t>(frame.payload.data());
    const uint8_t channels = frame.payload[4];
    const float energy = read_float_le(&frame.payload[5]);
    const float sample_peak = read_float_le(&frame.payload[9]);
    const float true_peak = read_float_le(&frame.payload[13]);
    const uint8_t peak_channel = frame.payload[17];
    const uint32_t frames = protocol::read_le<uint32_t>(&frame.payload[18]);
    input_sample_rate_ = sample_rate;
    input_channels_ = channels;
    hybrid_ = true;
    meter_.process_observation(energy, sample_peak, true_peak, peak_channel, frames);
  }
}

void App::send_capabilities() {
  std::vector<uint8_t> payload;
  payload.push_back(1);   // protocol major
  payload.push_back(2);   // local PCM max channels
  payload.push_back(12);  // hybrid max channels
  payload.push_back(0x0f);  // PCM, observation, controls, status
  protocol::append_le<uint32_t>(payload, 48000);
  protocol::append_le<uint16_t>(payload, 1000);  // maximum PCM payload frames/packet
  transport_.send(protocol::MessageType::kCapabilities, payload);
}

void App::send_status() {
  const auto snapshot = meter_.snapshot();
  std::vector<uint8_t> payload;
  append_float_le(payload, snapshot.momentary_lufs);
  append_float_le(payload, snapshot.short_term_lufs);
  append_float_le(payload, snapshot.integrated_lufs);
  append_float_le(payload, snapshot.loudness_range_lu);
  append_float_le(payload, snapshot.true_peak_dbtp);
  append_float_le(payload, snapshot.max_true_peak_dbtp);
  append_float_le(payload, settings_.target_lufs);
  payload.push_back(mode_ == Mode::kComputer ? 1 : 0);
  payload.push_back(hybrid_ ? 1 : 0);
  payload.push_back(input_channels_);
  payload.push_back(0);
  protocol::append_le<uint32_t>(payload, input_sample_rate_);
  uint8_t hardware_flags = 0;
  if (board_.display_ok()) hardware_flags |= 0x01;
  if (board_.touch_ok()) hardware_flags |= 0x02;
  if (board_.microphone_ok()) hardware_flags |= 0x04;
  payload.push_back(hardware_flags);
  append_float_le(payload, spl_db_);
  transport_.send(protocol::MessageType::kStatus, payload);
  last_status_ms_ = millis();
}

void App::update_microphone() {
  const std::size_t frames = board_.read_microphone(mic_stereo_.data(), 480);
  if (frames == 0) return;
  double sum_square = 0.0;
  float peak = 0.0F;
  for (std::size_t i = 0; i < frames; ++i) {
    const int16_t left = mic_stereo_[i * 2];
    const int16_t right = mic_stereo_[i * 2 + 1];
    const float mono = (std::abs(left) >= std::abs(right) ? left : right) / 32768.0F;
    sum_square += mono * mono;
    peak = std::max(peak, std::fabs(mono));
    octave_window_[octave_cursor_++] = mono;
    if ((i % 2) == 0) push_waveform_sample(mono);
    if (octave_cursor_ >= octave_window_.size()) {
      octave_.process(octave_window_.data(), octave_window_.size());
      octave_cursor_ = 0;
    }
  }
  const float rms = std::sqrt(sum_square / frames);
  const float current = rms > 0.0F
      ? 20.0F * std::log10(rms) + settings_.spl_offset_db : -INFINITY;
  spl_db_ = std::isfinite(spl_db_) ? 0.85F * spl_db_ + 0.15F * current : current;
  const float peak_spl = peak > 0.0F
      ? 20.0F * std::log10(peak) + settings_.spl_offset_db : -INFINITY;
  if (!std::isfinite(spl_peak_db_) || peak_spl > spl_peak_db_) spl_peak_db_ = peak_spl;
}

void App::push_waveform_sample(float sample) {
  waveform_[waveform_cursor_] = sample;
  waveform_cursor_ = (waveform_cursor_ + 1) % waveform_.size();
}

void App::handle_action(ui::UiAction action) {
  switch (action) {
    case ui::UiAction::kReset:
      meter_.reset();
      spl_peak_db_ = -INFINITY;
      break;
    case ui::UiAction::kTargetDown:
      settings_.target_lufs = std::max(-36.0F, settings_.target_lufs - 0.1F);
      meter_.set_target(settings_.target_lufs);
      settings_.preset = dsp::MeterPreset::kCustom;
      settings_store_.save(settings_);
      break;
    case ui::UiAction::kTargetUp:
      settings_.target_lufs = std::min(-5.0F, settings_.target_lufs + 0.1F);
      meter_.set_target(settings_.target_lufs);
      settings_.preset = dsp::MeterPreset::kCustom;
      settings_store_.save(settings_);
      break;
    case ui::UiAction::kNextPage:
      page_ = static_cast<ui::Page>((static_cast<uint8_t>(page_) + 1) % 5);
      break;
    case ui::UiAction::kSelectComputer:
      mode_ = Mode::kComputer;
      page_ = ui::Page::kMain;
      prompt_mode_ = false;
      meter_.reset();
      break;
    case ui::UiAction::kSelectMic:
      mode_ = Mode::kMic;
      page_ = ui::Page::kMic;
      prompt_mode_ = false;
      break;
    case ui::UiAction::kCalibrate:
      if (std::isfinite(spl_db_)) {
        settings_.spl_offset_db += 94.0F - spl_db_;
        settings_.spl_calibrated = true;
        settings_store_.save(settings_);
      }
      break;
    case ui::UiAction::kWaveformDown:
      settings_.waveform_scale = std::max(0.25F, settings_.waveform_scale - 0.25F);
      settings_store_.save(settings_);
      break;
    case ui::UiAction::kWaveformUp:
      settings_.waveform_scale = std::min(4.0F, settings_.waveform_scale + 0.25F);
      settings_store_.save(settings_);
      break;
    case ui::UiAction::kComputerTrimDown:
      settings_.computer_trim_db = std::max(-12.0F, settings_.computer_trim_db - 0.5F);
      meter_.set_input_trim_db(settings_.computer_trim_db);
      settings_store_.save(settings_);
      break;
    case ui::UiAction::kComputerTrimUp:
      settings_.computer_trim_db = std::min(12.0F, settings_.computer_trim_db + 0.5F);
      meter_.set_input_trim_db(settings_.computer_trim_db);
      settings_store_.save(settings_);
      break;
    case ui::UiAction::kNone: break;
  }
}

void App::update_ui() {
  if (!ui_) return;
  ui::UiState state;
  state.meter = meter_.snapshot();
  state.history = &meter_.history();
  state.octave = &octave_.levels_dbfs();
  state.waveform = &waveform_;
  state.waveform_cursor = waveform_cursor_;
  state.settings = settings_;
  state.page = page_;
  state.spl_db = spl_db_;
  state.spl_peak_db = spl_peak_db_;
  state.helper_connected = helper_connected_;
  state.hybrid = hybrid_;
  state.prompt_mode = prompt_mode_;
  state.mic_ok = board_.microphone_ok();
  state.channels = input_channels_;
  state.sample_rate = input_sample_rate_;
  state.status = mode_ == Mode::kComputer ? "USB" : "MIC";
  ui_->render(state);
  last_ui_ms_ = millis();
}

}  // namespace lmv1
