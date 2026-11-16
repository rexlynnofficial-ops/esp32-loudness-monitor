#pragma once

#include <algorithm>
#include <cmath>

namespace lmv1::ui {

inline int waveform_y(float sample, int center_y = 154, int amplitude = 88) {
  const float clamped = std::max(-1.0F, std::min(1.0F,
      std::isfinite(sample) ? sample : 0.0F));
  return center_y - static_cast<int>(clamped * amplitude);
}

}  // namespace lmv1::ui

