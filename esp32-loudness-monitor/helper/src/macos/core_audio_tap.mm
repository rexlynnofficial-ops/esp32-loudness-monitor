#import <CoreAudio/AudioHardware.h>
#import <CoreAudio/AudioHardwareTapping.h>
#import <CoreAudio/CATapDescription.h>
#import <Foundation/Foundation.h>

#include "audio_capture.h"

#include <algorithm>
#include <array>
#include <sstream>

namespace lmv1::helper {
namespace {

std::string os_error(const char* operation, OSStatus status) {
  std::ostringstream stream;
  stream << operation << " failed (OSStatus " << status << ')';
  return stream.str();
}

class CoreAudioTapCapture final : public AudioCapture {
 public:
  explicit CoreAudioTapCapture(CaptureOptions options)
      : options_(std::move(options)) {}
  ~CoreAudioTapCapture() override { stop(); }

  bool start(AudioCallback callback, std::string& error) override {
    if (@available(macOS 14.2, *)) {
      callback_ = std::move(callback);
      CATapDescription* description =
          [[CATapDescription alloc] initStereoGlobalTapButExcludeProcesses:@[]];
      description.name = @"Loudness Bridge Global Mix";
      description.privateTap = YES;
      description.muteBehavior = CATapUnmuted;
      if (options_.preserve_channels) {
        description.mixdown = NO;
        if (!options_.device_uid.empty()) {
          description.deviceUID = [NSString stringWithUTF8String:options_.device_uid.c_str()];
          description.stream = @(options_.stream_index);
        }
      }

      OSStatus status = AudioHardwareCreateProcessTap(description, &tap_id_);
      if (status != noErr) {
        error = os_error("AudioHardwareCreateProcessTap", status) +
            "; allow System Audio Recording in Privacy & Security";
        return false;
      }

      CFStringRef tap_uid = nullptr;
      AudioObjectPropertyAddress uid_address{
          kAudioTapPropertyUID, kAudioObjectPropertyScopeGlobal,
          kAudioObjectPropertyElementMain};
      UInt32 uid_size = sizeof(tap_uid);
      status = AudioObjectGetPropertyData(tap_id_, &uid_address, 0, nullptr,
                                          &uid_size, &tap_uid);
      if (status != noErr || tap_uid == nullptr) {
        error = os_error("read tap UID", status);
        stop();
        return false;
      }

      NSDictionary* tap_entry = @{
        @kAudioSubTapUIDKey: (__bridge NSString*)tap_uid,
        @kAudioSubTapDriftCompensationKey: @YES
      };
      NSString* aggregate_uid =
          [NSString stringWithFormat:@"com.openai.loudnessbridge.%@",
                                     [[NSUUID UUID] UUIDString]];
      NSDictionary* aggregate_description = @{
        @kAudioAggregateDeviceNameKey: @"Loudness Bridge Capture",
        @kAudioAggregateDeviceUIDKey: aggregate_uid,
        @kAudioAggregateDeviceIsPrivateKey: @YES,
        @kAudioAggregateDeviceTapListKey: @[tap_entry],
        @kAudioAggregateDeviceTapAutoStartKey: @YES
      };
      status = AudioHardwareCreateAggregateDevice(
          (__bridge CFDictionaryRef)aggregate_description, &aggregate_id_);
      CFRelease(tap_uid);
      if (status != noErr) {
        error = os_error("AudioHardwareCreateAggregateDevice", status);
        stop();
        return false;
      }

      AudioObjectPropertyAddress format_address{
          kAudioTapPropertyFormat, kAudioObjectPropertyScopeGlobal,
          kAudioObjectPropertyElementMain};
      UInt32 format_size = sizeof(format_);
      status = AudioObjectGetPropertyData(tap_id_, &format_address, 0, nullptr,
                                          &format_size, &format_);
      if (status != noErr || format_.mFormatID != kAudioFormatLinearPCM ||
          (format_.mFormatFlags & kAudioFormatFlagIsFloat) == 0) {
        error = "Core Audio tap did not provide Float32 linear PCM";
        stop();
        return false;
      }

      CoreAudioTapCapture* owner = this;
      status = AudioDeviceCreateIOProcIDWithBlock(
          &io_proc_, aggregate_id_, nullptr,
          ^(const AudioTimeStamp*, const AudioBufferList* input,
            const AudioTimeStamp*, AudioBufferList*, const AudioTimeStamp*) {
            owner->consume(input);
          });
      if (status != noErr) {
        error = os_error("AudioDeviceCreateIOProcIDWithBlock", status);
        stop();
        return false;
      }
      status = AudioDeviceStart(aggregate_id_, io_proc_);
      if (status != noErr) {
        error = os_error("AudioDeviceStart", status);
        stop();
        return false;
      }
      running_ = true;
      return true;
    }
    error = "Core Audio Process Tap requires macOS 14.2 or newer";
    return false;
  }

  void stop() override {
    if (running_ && aggregate_id_ != kAudioObjectUnknown && io_proc_ != nullptr)
      AudioDeviceStop(aggregate_id_, io_proc_);
    running_ = false;
    if (aggregate_id_ != kAudioObjectUnknown && io_proc_ != nullptr)
      AudioDeviceDestroyIOProcID(aggregate_id_, io_proc_);
    io_proc_ = nullptr;
    if (aggregate_id_ != kAudioObjectUnknown)
      AudioHardwareDestroyAggregateDevice(aggregate_id_);
    aggregate_id_ = kAudioObjectUnknown;
    if (tap_id_ != kAudioObjectUnknown) AudioHardwareDestroyProcessTap(tap_id_);
    tap_id_ = kAudioObjectUnknown;
  }

 private:
  void consume(const AudioBufferList* input) {
    if (input == nullptr || input->mNumberBuffers == 0 || !callback_) return;
    const uint8_t channels = static_cast<uint8_t>(std::min<UInt32>(
        12, std::max<UInt32>(1, format_.mChannelsPerFrame)));
    std::size_t frames = 0;
    if (input->mNumberBuffers == 1) {
      const AudioBuffer& buffer = input->mBuffers[0];
      if (buffer.mData == nullptr) return;
      const uint8_t buffer_channels = static_cast<uint8_t>(
          std::max<UInt32>(1, buffer.mNumberChannels));
      frames = buffer.mDataByteSize / (sizeof(float) * buffer_channels);
      callback_(static_cast<const float*>(buffer.mData), frames, buffer_channels,
                static_cast<uint32_t>(format_.mSampleRate));
      return;
    }

    frames = input->mBuffers[0].mDataByteSize / sizeof(float);
    frames = std::min<std::size_t>(frames, scratch_.size() / channels);
    for (std::size_t frame = 0; frame < frames; ++frame) {
      for (uint8_t channel = 0; channel < channels; ++channel) {
        const UInt32 buffer_index = std::min<UInt32>(channel,
                                                     input->mNumberBuffers - 1);
        const AudioBuffer& buffer = input->mBuffers[buffer_index];
        scratch_[frame * channels + channel] = buffer.mData
            ? static_cast<const float*>(buffer.mData)[frame] : 0.0F;
      }
    }
    callback_(scratch_.data(), frames, channels,
              static_cast<uint32_t>(format_.mSampleRate));
  }

  AudioCallback callback_;
  CaptureOptions options_;
  AudioObjectID tap_id_ = kAudioObjectUnknown;
  AudioObjectID aggregate_id_ = kAudioObjectUnknown;
  AudioDeviceIOProcID io_proc_ = nullptr;
  AudioStreamBasicDescription format_{};
  std::array<float, 8192 * 12> scratch_{};
  bool running_ = false;
};
}

std::unique_ptr<AudioCapture> make_platform_capture(const CaptureOptions& options) {
  return std::make_unique<CoreAudioTapCapture>(options);
}

}  // namespace lmv1::helper
