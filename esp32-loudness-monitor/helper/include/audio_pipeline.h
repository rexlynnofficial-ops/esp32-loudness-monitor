#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "protocol/wire_protocol.h"

namespace lmv1::helper {

class AudioPipeline {
 public:
  AudioPipeline();
  void push(const float* interleaved, std::size_t frames, uint8_t channels,
            uint32_t sample_rate);
  bool pop(protocol::Frame& frame);
  uint64_t dropped_blocks() const { return dropped_.load(); }

 private:
  static constexpr std::size_t kQueueDepth = 128;
  static constexpr std::size_t kMaxSamples = 4096;
  struct Block {
    std::array<float, kMaxSamples> samples{};
    std::size_t frames = 0;
    uint8_t channels = 0;
    uint32_t sample_rate = 0;
  };

  struct BiquadState {
    double z1 = 0.0;
    double z2 = 0.0;
    double process(double input, double b0, double b1, double b2,
                   double a1, double a2);
  };

  std::unique_ptr<Block[]> queue_;
  Block pending_{};
  std::size_t pending_frames_ = 0;
  std::atomic<std::size_t> write_index_{0};
  std::atomic<std::size_t> read_index_{0};
  std::atomic<uint64_t> dropped_{0};
  uint32_t sequence_ = 1;
  uint64_t resample_remainder_ = 0;
  uint32_t previous_sample_rate_ = 0;
  uint8_t previous_hybrid_channels_ = 0;
  std::array<BiquadState, 12> hybrid_shelf_{};
  std::array<BiquadState, 12> hybrid_high_pass_{};

  bool enqueue_pending();
};

}  // namespace lmv1::helper
