#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace lmv1::dsp {

class ThirdOctaveAnalyzer {
 public:
  static constexpr std::size_t kBandCount = 30;
  explicit ThirdOctaveAnalyzer(uint32_t sample_rate);
  void process(const float* mono, std::size_t frames);
  const std::array<float, kBandCount>& centers_hz() const { return centers_hz_; }
  const std::array<float, kBandCount>& levels_dbfs() const { return levels_dbfs_; }

 private:
  uint32_t sample_rate_;
  std::array<float, kBandCount> centers_hz_{};
  std::array<float, kBandCount> levels_dbfs_{};
};

}  // namespace lmv1::dsp

