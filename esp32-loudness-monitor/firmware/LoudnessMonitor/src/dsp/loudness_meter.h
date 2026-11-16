#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

namespace lmv1::dsp {

enum class MeterPreset : uint8_t {
  kEbuR128,
  kAtscA85,
  kStreaming,
  kCustom,
};

struct MeterConfig {
  uint32_t sample_rate = 48000;
  uint8_t channels = 2;
  float silence_reset_seconds = 30.0F;
};

struct MeterSnapshot {
  float momentary_lufs;
  float short_term_lufs;
  float integrated_lufs;
  float loudness_range_lu;
  float sample_peak_dbfs;
  float true_peak_dbtp;
  float max_true_peak_dbtp;
  uint8_t peak_channel;
  uint64_t measured_frames;
};

class LoudnessMeter {
 public:
  explicit LoudnessMeter(MeterConfig config);

  void process_interleaved_s16(const int16_t* samples, std::size_t frames);
  void process_observation(float weighted_mean_square, float sample_peak,
                           float true_peak, uint8_t peak_channel,
                           uint32_t frames);
  void reset();
  void set_paused(bool paused) { paused_ = paused; }
  bool paused() const { return paused_; }
  void set_target(float lufs);
  void set_input_trim_db(float db);
  void set_preset(MeterPreset preset);
  float target_lufs() const { return target_lufs_; }
  MeterPreset preset() const { return preset_; }
  const MeterSnapshot& snapshot() const { return snapshot_; }
  const std::deque<float>& history() const { return history_; }

 private:
  struct Biquad {
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;
    double process(double input);
    void clear() { z1 = z2 = 0.0; }
  };

  struct TruePeak4x {
    std::array<double, 12> history{};
    std::array<std::array<double, 12>, 4> coefficients{};
    TruePeak4x();
    double process(double input);
    void clear();
  };

  void finalize_block(double energy, double sample_peak, double true_peak,
                      uint8_t peak_channel, uint32_t frames);
  void update_snapshot();
  static float energy_to_lufs(double energy);
  static double mean_tail(const std::vector<double>& values, std::size_t count);

  MeterConfig config_;
  std::vector<Biquad> shelf_;
  std::vector<Biquad> high_pass_;
  std::vector<TruePeak4x> true_peak_;
  std::vector<double> block_energies_;
  std::vector<double> gating_energies_;
  std::vector<double> short_term_energies_;
  std::deque<float> history_;
  MeterSnapshot snapshot_{};
  uint32_t block_frames_ = 0;
  double block_energy_sum_ = 0.0;
  double block_sample_peak_ = 0.0;
  double block_true_peak_ = 0.0;
  uint8_t block_peak_channel_ = 0;
  uint32_t silent_blocks_ = 0;
  uint32_t gating_decimation_ = 0;
  uint32_t short_term_decimation_ = 0;
  uint32_t lra_decimation_ = 0;
  double observation_energy_sum_ = 0.0;
  double observation_sample_peak_ = 0.0;
  double observation_true_peak_ = 0.0;
  uint8_t observation_peak_channel_ = 0;
  uint32_t observation_frames_ = 0;
  bool paused_ = false;
  MeterPreset preset_ = MeterPreset::kEbuR128;
  float target_lufs_ = -23.0F;
  float input_trim_db_ = 0.0F;
  double input_trim_gain_ = 1.0;
};

}  // namespace lmv1::dsp
