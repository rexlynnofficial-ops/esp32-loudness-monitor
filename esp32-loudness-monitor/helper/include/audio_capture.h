#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace lmv1::helper {

using AudioCallback = std::function<void(const float* interleaved,
                                         std::size_t frames,
                                         uint8_t channels,
                                         uint32_t sample_rate)>;

struct CaptureOptions {
  std::string device_uid;
  int stream_index = 0;
  bool preserve_channels = false;
};

class AudioCapture {
 public:
  virtual ~AudioCapture() = default;
  virtual bool start(AudioCallback callback, std::string& error) = 0;
  virtual void stop() = 0;
};

std::unique_ptr<AudioCapture> make_platform_capture(const CaptureOptions& options);

}  // namespace lmv1::helper
