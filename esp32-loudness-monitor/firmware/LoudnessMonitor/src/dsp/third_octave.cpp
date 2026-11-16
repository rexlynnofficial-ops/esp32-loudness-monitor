#include "third_octave.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace lmv1::dsp {
namespace {
constexpr float kPi = 3.14159265358979323846F;
constexpr std::array<float, ThirdOctaveAnalyzer::kBandCount> kCenters{{
    20, 25, 31.5F, 40, 50, 63, 80, 100, 125, 160, 200, 250, 315, 400,
    500, 630, 800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000,
    6300, 8000, 10000, 12500, 16000}};
}

ThirdOctaveAnalyzer::ThirdOctaveAnalyzer(uint32_t sample_rate)
    : sample_rate_(sample_rate), centers_hz_(kCenters) {
  levels_dbfs_.fill(-std::numeric_limits<float>::infinity());
}

void ThirdOctaveAnalyzer::process(const float* mono, std::size_t frames) {
  if (mono == nullptr || frames == 0) return;
  for (std::size_t band = 0; band < kBandCount; ++band) {
    const float frequency = centers_hz_[band];
    if (frequency >= sample_rate_ * 0.5F) {
      levels_dbfs_[band] = -std::numeric_limits<float>::infinity();
      continue;
    }
    const float omega = 2.0F * kPi * frequency / sample_rate_;
    const float coefficient = 2.0F * std::cos(omega);
    float s0 = 0.0F, s1 = 0.0F, s2 = 0.0F;
    for (std::size_t i = 0; i < frames; ++i) {
      const float window = 0.5F - 0.5F * std::cos(
          2.0F * kPi * static_cast<float>(i) / std::max<std::size_t>(1, frames - 1));
      s0 = mono[i] * window + coefficient * s1 - s2;
      s2 = s1;
      s1 = s0;
    }
    const float power = std::max(0.0F, s1 * s1 + s2 * s2 - coefficient * s1 * s2);
    const float amplitude = 2.0F * std::sqrt(power) / frames;
    levels_dbfs_[band] = amplitude > 0.0F ? 20.0F * std::log10(amplitude)
                                         : -std::numeric_limits<float>::infinity();
  }
}

}  // namespace lmv1::dsp
