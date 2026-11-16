#include "audio_pipeline.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

namespace lmv1::helper {
static uint64_t now_us() {
  return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count());
}

AudioPipeline::AudioPipeline() : queue_(std::make_unique<Block[]>(kQueueDepth)) {}

double AudioPipeline::BiquadState::process(double input, double b0, double b1,
                                           double b2, double a1, double a2) {
  const double output = b0 * input + z1;
  z1 = b1 * input - a1 * output + z2;
  z2 = b2 * input - a2 * output;
  return output;
}

static void append_float(std::vector<uint8_t>& output, float value) {
  uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  protocol::append_le<uint32_t>(output, bits);
}

void AudioPipeline::push(const float* interleaved, std::size_t frames,
                         uint8_t channels, uint32_t sample_rate) {
  if (interleaved == nullptr || frames == 0 || channels == 0) return;
  const uint8_t bounded_channels = std::min<uint8_t>(channels, 12);
  if (pending_frames_ > 0 && (pending_.channels != bounded_channels ||
                              pending_.sample_rate != sample_rate)) {
    pending_frames_ = 0;
  }
  pending_.channels = bounded_channels;
  pending_.sample_rate = sample_rate;
  std::size_t offset = 0;
  const std::size_t capacity = kMaxSamples / bounded_channels;
  const std::size_t target_frames = std::min<std::size_t>(960, capacity);
  while (offset < frames) {
    const std::size_t take = std::min(frames - offset,
                                      target_frames - pending_frames_);
    std::copy_n(interleaved + offset * bounded_channels,
                take * bounded_channels,
                pending_.samples.begin() + pending_frames_ * bounded_channels);
    pending_frames_ += take;
    offset += take;
    if (pending_frames_ == target_frames) {
      if (!enqueue_pending()) ++dropped_;
      pending_frames_ = 0;
    }
  }
}

bool AudioPipeline::enqueue_pending() {
  const std::size_t write = write_index_.load(std::memory_order_relaxed);
  const std::size_t next = (write + 1) % kQueueDepth;
  if (next == read_index_.load(std::memory_order_acquire)) return false;
  Block& block = queue_[write];
  block.channels = pending_.channels;
  block.frames = pending_frames_;
  block.sample_rate = pending_.sample_rate;
  std::copy_n(pending_.samples.begin(), pending_frames_ * block.channels,
              block.samples.begin());
  write_index_.store(next, std::memory_order_release);
  return true;
}

bool AudioPipeline::pop(protocol::Frame& frame) {
  const std::size_t read = read_index_.load(std::memory_order_relaxed);
  if (read == write_index_.load(std::memory_order_acquire)) return false;
  const Block& block = queue_[read];
  frame = {};
  frame.sequence = sequence_++;
  frame.timestamp_us = now_us();

  if (block.channels <= 2) {
    // Linear resampling is intentionally simple and deterministic. Capture APIs
    // normally deliver 44.1 or 48 kHz; the device contract is fixed at 48 kHz.
    if (previous_sample_rate_ != block.sample_rate) {
      resample_remainder_ = 0;
      previous_sample_rate_ = block.sample_rate;
    }
    resample_remainder_ += static_cast<uint64_t>(block.frames) * 48000U;
    const std::size_t output_frames = std::min<std::size_t>(
        960, resample_remainder_ / block.sample_rate);
    resample_remainder_ %= block.sample_rate;
    const double ratio = output_frames > 1
        ? static_cast<double>(block.frames - 1) / (output_frames - 1) : 0.0;
    frame.type = protocol::MessageType::kPcmS16;
    protocol::append_le<uint32_t>(frame.payload, 48000);
    frame.payload.push_back(block.channels);
    frame.payload.push_back(0);  // standard L/R channel map
    protocol::append_le<uint16_t>(frame.payload,
                                  static_cast<uint16_t>(output_frames));
    for (std::size_t i = 0; i < output_frames; ++i) {
      const double position = std::min<double>(block.frames - 1, i * ratio);
      const std::size_t index = static_cast<std::size_t>(position);
      const std::size_t next = std::min(index + 1, block.frames - 1);
      const float fraction = static_cast<float>(position - index);
      for (uint8_t channel = 0; channel < block.channels; ++channel) {
        const float a = block.samples[index * block.channels + channel];
        const float b = block.samples[next * block.channels + channel];
        const float sample = a + (b - a) * fraction;
        const int16_t quantized = static_cast<int16_t>(std::lrint(
            std::max(-1.0F, std::min(0.999969F, sample)) * 32768.0F));
        protocol::append_le<uint16_t>(frame.payload,
                                      static_cast<uint16_t>(quantized));
      }
    }
  } else {
    frame.type = protocol::MessageType::kObservation;
    if (previous_hybrid_channels_ != block.channels) {
      hybrid_shelf_ = {};
      hybrid_high_pass_ = {};
      previous_hybrid_channels_ = block.channels;
    }
    double energy = 0.0;
    float sample_peak = 0.0F;
    uint8_t peak_channel = 0;
    for (std::size_t i = 0; i < block.frames; ++i) {
      for (uint8_t channel = 0; channel < block.channels; ++channel) {
        const float sample = block.samples[i * block.channels + channel];
        const float absolute = std::fabs(sample);
        if (absolute > sample_peak) {
          sample_peak = absolute;
          peak_channel = channel;
        }
        // ITU-R BS.1770 K-weighting coefficients at the V1 hybrid rate of 48 kHz.
        const double shelf = hybrid_shelf_[channel].process(
            sample, 1.53512485958697, -2.69169618940638, 1.19839281085285,
            -1.69065929318241, 0.73248077421585);
        const double weighted = hybrid_high_pass_[channel].process(
            shelf, 1.0, -2.0, 1.0, -1.99004745483398, 0.99007225036621);
        if (channel != 3) energy += weighted * weighted;
      }
    }
    energy /= std::max<std::size_t>(1, block.frames);
    protocol::append_le<uint32_t>(frame.payload, block.sample_rate);
    frame.payload.push_back(block.channels);
    append_float(frame.payload, static_cast<float>(energy));
    append_float(frame.payload, sample_peak);
    append_float(frame.payload, sample_peak);  // architecture field; backend TP upgradeable
    frame.payload.push_back(peak_channel);
    protocol::append_le<uint32_t>(frame.payload,
                                  static_cast<uint32_t>(block.frames));
  }
  read_index_.store((read + 1) % kQueueDepth, std::memory_order_release);
  return true;
}

}  // namespace lmv1::helper
