#include "audio_capture.h"

namespace lmv1::helper {
class StubCapture final : public AudioCapture {
 public:
  bool start(AudioCallback, std::string& error) override {
    error = "audio capture is supported only on macOS and Windows";
    return false;
  }
  void stop() override {}
};
std::unique_ptr<AudioCapture> make_platform_capture(const CaptureOptions&) {
  return std::make_unique<StubCapture>();
}
}  // namespace lmv1::helper
