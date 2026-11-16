#include "loudness_meter.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace lmv1::dsp {
namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kAbsoluteGateLufs = -70.0;
constexpr double kLoudnessOffset = -0.691;
constexpr std::size_t kProcessingBlockHz = 50;
constexpr std::size_t kMomentaryBlocks = 20;   // 400 ms
constexpr std::size_t kShortTermBlocks = 150; // 3 seconds
constexpr std::size_t kHistoryCapacity = 3000; // 60 seconds at 50 Hz

double sinc(double value) {
  if (std::fabs(value) < 1.0e-12) {
    return 1.0;
  }
  return std::sin(kPi * value) / (kPi * value);
}

double lufs_to_energy(double lufs) {
  return std::pow(10.0, (lufs - kLoudnessOffset) / 10.0);
}

float linear_to_db(double value) {
  return value > 0.0 ? static_cast<float>(20.0 * std::log10(value))
                     : -std::numeric_limits<float>::infinity();
}
}  // namespace

double LoudnessMeter::Biquad::process(double input) {
  const double output = b0 * input + z1;
  z1 = b1 * input - a1 * output + z2;
  z2 = b2 * input - a2 * output;
  return output;
}

LoudnessMeter::TruePeak4x::TruePeak4x() {
  constexpr double center = 5.5;
  for (std::size_t phase = 0; phase < coefficients.size(); ++phase) {
    double sum = 0.0;
    for (std::size_t tap = 0; tap < history.size(); ++tap) {
      const double position = static_cast<double>(tap) - center +
                              static_cast<double>(phase) / 4.0;
      const double window = 0.5 - 0.5 * std::cos(
          2.0 * kPi * static_cast<double>(tap) / (history.size() - 1));
      coefficients[phase][tap] = sinc(position) * window;
      sum += coefficients[phase][tap];
    }
    for (double& coefficient : coefficients[phase]) {
      coefficient /= sum;
    }
  }
}

double LoudnessMeter::TruePeak4x::process(double input) {
  std::move_backward(history.begin(), history.end() - 1, history.end());
  history[0] = input;
  double peak = std::fabs(input);
  for (const auto& phase : coefficients) {
    double sample = 0.0;
    for (std::size_t tap = 0; tap < history.size(); ++tap) {
      sample += history[tap] * phase[tap];
    }
    peak = std::max(peak, std::fabs(sample));
  }
  return peak;
}

void LoudnessMeter::TruePeak4x::clear() { history.fill(0.0); }

LoudnessMeter::LoudnessMeter(MeterConfig config) : config_(config) {
  config_.channels = std::max<uint8_t>(1, std::min<uint8_t>(12, config_.channels));
  config_.sample_rate = std::max<uint32_t>(8000, config_.sample_rate);
  shelf_.resize(config_.channels);
  high_pass_.resize(config_.channels);
  true_peak_.resize(config_.channels);

  // ITU-R BS.1770 K-weighting coefficients for 48 kHz.
  for (auto& filter : shelf_) {
    filter = {1.53512485958697, -2.69169618940638, 1.19839281085285,
              -1.69065929318241, 0.73248077421585};
  }
  for (auto& filter : high_pass_) {
    filter = {1.0, -2.0, 1.0, -1.99004745483398, 0.99007225036621};
  }
  reset();
}

void LoudnessMeter::reset() {
  block_energies_.clear();
  gating_energies_.clear();
  short_term_energies_.clear();
  history_.clear();
  block_frames_ = 0;
  block_energy_sum_ = 0.0;
  block_sample_peak_ = 0.0;
  block_true_peak_ = 0.0;
  block_peak_channel_ = 0;
  silent_blocks_ = 0;
  gating_decimation_ = 0;
  short_term_decimation_ = 0;
  lra_decimation_ = 0;
  observation_energy_sum_ = 0.0;
  observation_sample_peak_ = 0.0;
  observation_true_peak_ = 0.0;
  observation_peak_channel_ = 0;
  observation_frames_ = 0;
  for (auto& filter : shelf_) filter.clear();
  for (auto& filter : high_pass_) filter.clear();
  for (auto& estimator : true_peak_) estimator.clear();
  const float empty = -std::numeric_limits<float>::infinity();
  snapshot_ = {empty, empty, empty, 0.0F, empty, empty, empty, 0, 0};
}

void LoudnessMeter::set_target(float lufs) {
  target_lufs_ = std::max(-36.0F, std::min(-5.0F, lufs));
  preset_ = MeterPreset::kCustom;
}

void LoudnessMeter::set_input_trim_db(float db) {
  input_trim_db_ = std::max(-12.0F, std::min(12.0F, db));
  input_trim_gain_ = std::pow(10.0, input_trim_db_ / 20.0);
}

void LoudnessMeter::set_preset(MeterPreset preset) {
  preset_ = preset;
  switch (preset) {
    case MeterPreset::kEbuR128: target_lufs_ = -23.0F; break;
    case MeterPreset::kAtscA85: target_lufs_ = -24.0F; break;
    case MeterPreset::kStreaming: target_lufs_ = -14.0F; break;
    case MeterPreset::kCustom: break;
  }
}

void LoudnessMeter::process_interleaved_s16(const int16_t* samples,
                                             std::size_t frames) {
  if (samples == nullptr || paused_) return;
  const uint32_t frames_per_block = config_.sample_rate / kProcessingBlockHz;
  for (std::size_t frame = 0; frame < frames; ++frame) {
    double frame_energy = 0.0;
    for (uint8_t channel = 0; channel < config_.channels; ++channel) {
      const double input = samples[frame * config_.channels + channel] / 32768.0 *
                           input_trim_gain_;
      const double absolute = std::fabs(input);
      if (absolute > block_sample_peak_) {
        block_sample_peak_ = absolute;
        block_peak_channel_ = channel;
      }
      const double interpolated_peak = true_peak_[channel].process(input);
      if (interpolated_peak > block_true_peak_) {
        block_true_peak_ = interpolated_peak;
        block_peak_channel_ = channel;
      }
      const double weighted = high_pass_[channel].process(shelf_[channel].process(input));
      const double channel_weight = (config_.channels >= 4 && channel == 3) ? 0.0 : 1.0;
      frame_energy += channel_weight * weighted * weighted;
    }
    block_energy_sum_ += frame_energy;
    ++block_frames_;
    ++snapshot_.measured_frames;
    if (block_frames_ >= frames_per_block) {
      finalize_block(block_energy_sum_ / block_frames_, block_sample_peak_,
                     block_true_peak_, block_peak_channel_, block_frames_);
      block_frames_ = 0;
      block_energy_sum_ = block_sample_peak_ = block_true_peak_ = 0.0;
    }
  }
}

void LoudnessMeter::process_observation(float weighted_mean_square,
                                        float sample_peak, float true_peak,
                                        uint8_t peak_channel,
                                        uint32_t frames) {
  if (paused_) return;
  snapshot_.measured_frames += frames;
  const double trim_energy = input_trim_gain_ * input_trim_gain_;
  const double trimmed_sample_peak = std::max(0.0F, sample_peak) * input_trim_gain_;
  const double trimmed_true_peak = std::max(0.0F, true_peak) * input_trim_gain_;
  observation_energy_sum_ += std::max(0.0F, weighted_mean_square) * trim_energy * frames;
  if (trimmed_sample_peak >= observation_sample_peak_) {
    observation_sample_peak_ = trimmed_sample_peak;
    observation_peak_channel_ = peak_channel;
  }
  observation_true_peak_ = std::max(observation_true_peak_, trimmed_true_peak);
  observation_frames_ += frames;
  const uint32_t block_frames = config_.sample_rate / kProcessingBlockHz;
  if (observation_frames_ >= block_frames) {
    finalize_block(observation_energy_sum_ / observation_frames_,
                   observation_sample_peak_, observation_true_peak_,
                   observation_peak_channel_, observation_frames_);
    observation_energy_sum_ = 0.0;
    observation_sample_peak_ = 0.0;
    observation_true_peak_ = 0.0;
    observation_peak_channel_ = 0;
    observation_frames_ = 0;
  }
}

float LoudnessMeter::energy_to_lufs(double energy) {
  return energy >= lufs_to_energy(-120.0)
             ? static_cast<float>(kLoudnessOffset + 10.0 * std::log10(energy))
             : -std::numeric_limits<float>::infinity();
}

double LoudnessMeter::mean_tail(const std::vector<double>& values,
                                std::size_t count) {
  if (values.empty()) return 0.0;
  const std::size_t begin = values.size() > count ? values.size() - count : 0;
  return std::accumulate(values.begin() + begin, values.end(), 0.0) /
         static_cast<double>(values.size() - begin);
}

void LoudnessMeter::finalize_block(double energy, double sample_peak,
                                   double true_peak, uint8_t peak_channel,
                                   uint32_t) {
  block_energies_.push_back(energy);
  snapshot_.sample_peak_dbfs = linear_to_db(sample_peak);
  snapshot_.true_peak_dbtp = linear_to_db(true_peak);
  snapshot_.peak_channel = peak_channel;
  snapshot_.max_true_peak_dbtp =
      std::max(snapshot_.max_true_peak_dbtp, snapshot_.true_peak_dbtp);

  const bool silent = energy_to_lufs(energy) < kAbsoluteGateLufs;
  silent_blocks_ = silent ? silent_blocks_ + 1U : 0U;
  const uint32_t reset_blocks = static_cast<uint32_t>(
      std::max(1.0F, config_.silence_reset_seconds * kProcessingBlockHz));
  if (silent_blocks_ >= reset_blocks && block_energies_.size() > reset_blocks) {
    reset();
    return;
  }
  update_snapshot();
}

void LoudnessMeter::update_snapshot() {
  snapshot_.momentary_lufs = block_energies_.size() >= kMomentaryBlocks
                                  ? energy_to_lufs(mean_tail(block_energies_, kMomentaryBlocks))
                                  : -std::numeric_limits<float>::infinity();
  snapshot_.short_term_lufs = block_energies_.size() >= kShortTermBlocks
                                  ? energy_to_lufs(mean_tail(block_energies_, kShortTermBlocks))
                                  : -std::numeric_limits<float>::infinity();

  // BS.1770 integration uses 400 ms gating blocks with 100 ms overlap.
  if (block_energies_.size() >= kMomentaryBlocks && ++gating_decimation_ >= 5) {
    gating_energies_.push_back(mean_tail(block_energies_, kMomentaryBlocks));
    gating_decimation_ = 0;
  }
  const double absolute_threshold = lufs_to_energy(kAbsoluteGateLufs);
  const bool refresh_integrated = gating_energies_.empty() ||
      !std::isfinite(snapshot_.integrated_lufs) || gating_energies_.size() % 10 == 0;
  if (refresh_integrated) {
    std::vector<double> absolute_gated;
    for (double energy : gating_energies_) {
      if (energy >= absolute_threshold) absolute_gated.push_back(energy);
    }
    if (!absolute_gated.empty()) {
      const double ungated_mean = std::accumulate(absolute_gated.begin(),
          absolute_gated.end(), 0.0) / absolute_gated.size();
      const double relative_threshold = ungated_mean * 0.1;
      double sum = 0.0;
      std::size_t count = 0;
      for (double energy : absolute_gated) {
        if (energy >= relative_threshold) {
          sum += energy;
          ++count;
        }
      }
      snapshot_.integrated_lufs = count ? energy_to_lufs(sum / count)
                                        : -std::numeric_limits<float>::infinity();
    } else {
      snapshot_.integrated_lufs = -std::numeric_limits<float>::infinity();
    }
  }

  history_.push_back(snapshot_.momentary_lufs);
  while (history_.size() > kHistoryCapacity) history_.pop_front();

  if (std::isfinite(snapshot_.short_term_lufs) && ++short_term_decimation_ >= 5) {
    short_term_energies_.push_back(mean_tail(block_energies_, kShortTermBlocks));
    short_term_decimation_ = 0;
  }
  if (short_term_energies_.size() >= 2 && ++lra_decimation_ >= 50) {
    lra_decimation_ = 0;
    const double mean = std::accumulate(short_term_energies_.begin(),
        short_term_energies_.end(), 0.0) / short_term_energies_.size();
    const double threshold = std::max(absolute_threshold, mean * 0.01);
    std::vector<float> gated;
    for (double energy : short_term_energies_) {
      if (energy >= threshold) gated.push_back(energy_to_lufs(energy));
    }
    if (gated.size() >= 2) {
      std::sort(gated.begin(), gated.end());
      const std::size_t p10 = static_cast<std::size_t>((gated.size() - 1) * 0.10);
      const std::size_t p95 = static_cast<std::size_t>((gated.size() - 1) * 0.95);
      snapshot_.loudness_range_lu = gated[p95] - gated[p10];
    }
  }
}

}  // namespace lmv1::dsp
