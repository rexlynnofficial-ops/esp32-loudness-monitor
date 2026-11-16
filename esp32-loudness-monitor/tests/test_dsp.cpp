#include "dsp/loudness_meter.h"
#include "dsp/third_octave.h"
#include "test_support.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {
constexpr double kPi = 3.14159265358979323846;
}

int main() {
  using namespace lmv1::dsp;

  LoudnessMeter meter({48000, 2, 30.0F});
  std::vector<int16_t> silence(4800 * 2, 0);
  meter.process_interleaved_s16(silence.data(), 4800);
  CHECK(!std::isfinite(meter.snapshot().momentary_lufs));
  CHECK(meter.history().size() == 5);

  std::vector<int16_t> tone(4800 * 2);
  const double amplitude = std::pow(10.0, -23.0 / 20.0);
  for (std::size_t frame = 0; frame < 4800; ++frame) {
    const auto sample = static_cast<int16_t>(
        std::sin(2.0 * kPi * 1000.0 * frame / 48000.0) * amplitude * 32767.0);
    tone[frame * 2] = sample;
    tone[frame * 2 + 1] = sample;
  }
  for (int block = 0; block < 40; ++block) {
    meter.process_interleaved_s16(tone.data(), 4800);
  }
  const auto snapshot = meter.snapshot();
  CHECK(std::isfinite(snapshot.momentary_lufs));
  CHECK(std::isfinite(snapshot.short_term_lufs));
  CHECK(std::isfinite(snapshot.integrated_lufs));
  CHECK_NEAR(snapshot.momentary_lufs, snapshot.integrated_lufs, 0.2F);
  CHECK(snapshot.true_peak_dbtp >= snapshot.sample_peak_dbfs - 0.01F);
  CHECK(meter.history().size() == 205);

  for (int block = 0; block < 2960; ++block) {
    meter.process_interleaved_s16(tone.data(), 4800);
  }
  CHECK(meter.history().size() == 3000);

  meter.reset();
  CHECK(!std::isfinite(meter.snapshot().integrated_lufs));
  CHECK(meter.history().empty());

  meter.set_target(-14.0F);
  CHECK_NEAR(meter.target_lufs(), -14.0F, 0.001F);
  meter.set_preset(MeterPreset::kEbuR128);
  CHECK_NEAR(meter.target_lufs(), -23.0F, 0.001F);

  ThirdOctaveAnalyzer analyzer(48000);
  std::vector<float> mono(4800);
  for (std::size_t i = 0; i < mono.size(); ++i) {
    mono[i] = static_cast<float>(std::sin(2.0 * kPi * 1000.0 * i / 48000.0));
  }
  analyzer.process(mono.data(), mono.size());
  const auto bands = analyzer.levels_dbfs();
  const auto max_band = std::max_element(bands.begin(), bands.end());
  CHECK(max_band != bands.end());
  CHECK_NEAR(analyzer.centers_hz()[static_cast<std::size_t>(max_band - bands.begin())],
             1000.0F, 1.0F);

  return EXIT_SUCCESS;
}
