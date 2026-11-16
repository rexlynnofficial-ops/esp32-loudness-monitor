#include "audio_capture.h"

#include <Audioclient.h>
#include <Ksmedia.h>
#include <Mmdeviceapi.h>
#include <Windows.h>
#include <wrl/client.h>

#include <atomic>
#include <thread>
#include <vector>

namespace lmv1::helper {
using Microsoft::WRL::ComPtr;

class WasapiCapture final : public AudioCapture {
 public:
  ~WasapiCapture() override { stop(); }

  bool start(AudioCallback callback, std::string& error) override {
    callback_ = std::move(callback);
    HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(result) && result != RPC_E_CHANGED_MODE) {
      error = "CoInitializeEx failed";
      return false;
    }
    ComPtr<IMMDeviceEnumerator> enumerator;
    result = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                              IID_PPV_ARGS(&enumerator));
    if (FAILED(result) || FAILED(enumerator->GetDefaultAudioEndpoint(
            eRender, eConsole, &device_))) {
      error = "default WASAPI render endpoint unavailable";
      return false;
    }
    result = device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                               reinterpret_cast<void**>(client_.GetAddressOf()));
    if (FAILED(result)) { error = "IAudioClient activation failed"; return false; }
    WAVEFORMATEX* raw_format = nullptr;
    if (FAILED(client_->GetMixFormat(&raw_format))) {
      error = "WASAPI mix format unavailable"; return false;
    }
    format_.reset(raw_format);
    result = client_->Initialize(AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK, 0, 0, format_.get(), nullptr);
    if (FAILED(result) || FAILED(client_->GetService(IID_PPV_ARGS(&capture_)))) {
      error = "WASAPI loopback initialization failed"; return false;
    }
    if (FAILED(client_->Start())) { error = "WASAPI capture start failed"; return false; }
    running_ = true;
    thread_ = std::thread([this] { run(); });
    return true;
  }

  void stop() override {
    running_ = false;
    if (thread_.joinable()) thread_.join();
    if (client_) client_->Stop();
    capture_.Reset(); client_.Reset(); device_.Reset();
  }

 private:
  struct CoTaskMemDeleter { void operator()(WAVEFORMATEX* p) const { CoTaskMemFree(p); } };
  void run() {
    while (running_) {
      UINT32 packets = 0;
      if (FAILED(capture_->GetNextPacketSize(&packets))) break;
      if (packets == 0) { Sleep(5); continue; }
      BYTE* data = nullptr; UINT32 frames = 0; DWORD flags = 0;
      if (FAILED(capture_->GetBuffer(&data, &frames, &flags, nullptr, nullptr))) break;
      const uint8_t channels = static_cast<uint8_t>(format_->nChannels);
      if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
        silence_.assign(static_cast<std::size_t>(frames) * channels, 0.0F);
        callback_(silence_.data(), frames, channels, format_->nSamplesPerSec);
      } else if (is_float_format()) {
        callback_(reinterpret_cast<const float*>(data), frames, channels,
                  format_->nSamplesPerSec);
      } else if (is_pcm_format()) {
        const std::size_t sample_count = static_cast<std::size_t>(frames) * channels;
        conversion_.resize(sample_count);
        if (format_->wBitsPerSample == 16) {
          const auto* input = reinterpret_cast<const int16_t*>(data);
          for (std::size_t i = 0; i < sample_count; ++i)
            conversion_[i] = input[i] / 32768.0F;
        } else if (format_->wBitsPerSample == 32) {
          const auto* input = reinterpret_cast<const int32_t*>(data);
          for (std::size_t i = 0; i < sample_count; ++i)
            conversion_[i] = input[i] / 2147483648.0F;
        } else {
          capture_->ReleaseBuffer(frames);
          continue;
        }
        callback_(conversion_.data(), frames, channels, format_->nSamplesPerSec);
      }
      capture_->ReleaseBuffer(frames);
    }
  }
  bool is_float_format() const {
    if (format_->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) return true;
    if (format_->wFormatTag != WAVE_FORMAT_EXTENSIBLE ||
        format_->cbSize < sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX))
      return false;
    const auto* extended = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format_.get());
    return IsEqualGUID(extended->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
  }
  bool is_pcm_format() const {
    if (format_->wFormatTag == WAVE_FORMAT_PCM) return true;
    if (format_->wFormatTag != WAVE_FORMAT_EXTENSIBLE ||
        format_->cbSize < sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX))
      return false;
    const auto* extended = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format_.get());
    return IsEqualGUID(extended->SubFormat, KSDATAFORMAT_SUBTYPE_PCM);
  }
  AudioCallback callback_;
  ComPtr<IMMDevice> device_;
  ComPtr<IAudioClient> client_;
  ComPtr<IAudioCaptureClient> capture_;
  std::unique_ptr<WAVEFORMATEX, CoTaskMemDeleter> format_;
  std::vector<float> silence_;
  std::vector<float> conversion_;
  std::atomic<bool> running_{false};
  std::thread thread_;
};

std::unique_ptr<AudioCapture> make_platform_capture(const CaptureOptions&) {
  return std::make_unique<WasapiCapture>();
}
}  // namespace lmv1::helper
