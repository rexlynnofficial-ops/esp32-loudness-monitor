#include "settings.h"

namespace lmv1::storage {

SettingsData SettingsStore::load() {
  SettingsData data;
  Preferences preferences;
  if (!preferences.begin("lmv1", true)) return data;
  data.target_lufs = preferences.getFloat("target", data.target_lufs);
  data.preset = static_cast<lmv1::dsp::MeterPreset>(
      preferences.getUChar("preset", static_cast<uint8_t>(data.preset)));
  data.spl_offset_db = preferences.getFloat("splOffset", data.spl_offset_db);
  data.spl_calibrated = preferences.getBool("splCal", false);
  data.silence_reset_seconds = preferences.getUInt("silence", 30);
  data.waveform_scale = preferences.getFloat("waveScale", data.waveform_scale);
  if (data.waveform_scale < 0.25F || data.waveform_scale > 4.0F)
    data.waveform_scale = 1.0F;
  data.computer_trim_db = preferences.getFloat("computerTrim", 0.0F);
  if (data.computer_trim_db < -12.0F || data.computer_trim_db > 12.0F)
    data.computer_trim_db = 0.0F;
  preferences.end();
  return data;
}

bool SettingsStore::save(const SettingsData& data) {
  Preferences preferences;
  if (!preferences.begin("lmv1", false)) return false;
  bool ok = true;
  ok &= preferences.putFloat("target", data.target_lufs) == sizeof(float);
  ok &= preferences.putUChar("preset", static_cast<uint8_t>(data.preset)) == 1;
  ok &= preferences.putFloat("splOffset", data.spl_offset_db) == sizeof(float);
  ok &= preferences.putBool("splCal", data.spl_calibrated) == 1;
  ok &= preferences.putUInt("silence", data.silence_reset_seconds) == sizeof(uint32_t);
  ok &= preferences.putFloat("waveScale", data.waveform_scale) == sizeof(float);
  ok &= preferences.putFloat("computerTrim", data.computer_trim_db) == sizeof(float);
  preferences.end();
  return ok;
}

}  // namespace lmv1::storage
