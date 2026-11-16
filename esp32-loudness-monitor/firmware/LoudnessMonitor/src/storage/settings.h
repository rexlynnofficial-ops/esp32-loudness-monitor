#pragma once

#include <Preferences.h>

#include "../dsp/loudness_meter.h"

namespace lmv1::storage {

struct SettingsData {
  float target_lufs = -23.0F;
  lmv1::dsp::MeterPreset preset = lmv1::dsp::MeterPreset::kEbuR128;
  float spl_offset_db = 120.0F;
  bool spl_calibrated = false;
  uint32_t silence_reset_seconds = 30;
  float waveform_scale = 1.0F;
  float computer_trim_db = 0.0F;
};

class SettingsStore {
 public:
  SettingsData load();
  bool save(const SettingsData& data);
};

}  // namespace lmv1::storage
