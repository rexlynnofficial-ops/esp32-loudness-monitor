#include "audio_capture.h"
#include "audio_pipeline.h"
#include "serial_transport.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <cstring>
#include <string>
#include <thread>

namespace {
std::atomic<bool> running{true};

void stop_handler(int) { running = false; }

float read_float_le(const uint8_t* bytes) {
  const uint32_t bits = lmv1::protocol::read_le<uint32_t>(bytes);
  float value = 0.0F;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

int self_test() {
  lmv1::helper::AudioPipeline pipeline;
  float samples[960 * 2];
  for (std::size_t i = 0; i < 960; ++i) {
    const float value = 0.1F * std::sin(
        2.0 * 3.14159265358979323846 * 1000.0 * i / 48000.0);
    samples[i * 2] = samples[i * 2 + 1] = value;
  }
  pipeline.push(samples, 960, 2, 48000);
  lmv1::protocol::Frame frame;
  if (!pipeline.pop(frame) || frame.type != lmv1::protocol::MessageType::kPcmS16 ||
      frame.payload.size() != 8 + 960 * 2 * 2) {
    std::cerr << "pipeline self-test failed\n";
    return EXIT_FAILURE;
  }
  const auto bytes = lmv1::protocol::encode_frame(frame);
  lmv1::protocol::StreamDecoder decoder;
  std::vector<lmv1::protocol::Frame> decoded;
  decoder.push(bytes.data(), bytes.size(), decoded);
  if (decoded.size() != 1 || !(decoded[0] == frame)) {
    std::cerr << "wire self-test failed\n";
    return EXIT_FAILURE;
  }
  lmv1::helper::AudioPipeline resampling_pipeline;
  float source_44100[960 * 2]{};
  resampling_pipeline.push(source_44100, 960, 2, 44100);
  if (!resampling_pipeline.pop(frame) ||
      lmv1::protocol::read_le<uint16_t>(&frame.payload[6]) != 960) {
    std::cerr << "44.1-to-48 kHz packet cadence self-test failed\n";
    return EXIT_FAILURE;
  }
  lmv1::helper::AudioPipeline hybrid_pipeline;
  float source_12ch[960 * 12]{};
  source_12ch[8] = 0.5F;
  hybrid_pipeline.push(source_12ch, 960, 12, 48000);
  if (!hybrid_pipeline.pop(frame) ||
      frame.type != lmv1::protocol::MessageType::kObservation ||
      frame.payload.size() != 22) {
    std::cerr << "hybrid observation self-test failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "Loudness Bridge self-test PASS\n";
  return EXIT_SUCCESS;
}
}

int main(int argc, char** argv) {
  std::string device_path;
  bool select_computer = false;
  bool select_mic = false;
  lmv1::helper::CaptureOptions capture_options;
  for (int i = 1; i < argc; ++i) {
    const std::string argument = argv[i];
    if (argument == "--self-test") return self_test();
    if (argument == "--device" && i + 1 < argc) device_path = argv[++i];
    else if (argument == "--select-computer") select_computer = true;
    else if (argument == "--select-mic") select_mic = true;
    else if (argument == "--preserve-channels") capture_options.preserve_channels = true;
    else if (argument == "--device-uid" && i + 1 < argc)
      capture_options.device_uid = argv[++i];
    else if (argument == "--stream" && i + 1 < argc)
      capture_options.stream_index = std::stoi(argv[++i]);
    else if (argument == "--help") {
      std::cout << "Usage: loudness-bridge [--device PATH] "
                   "[--select-computer|--select-mic] [--preserve-channels "
                   "--device-uid UID --stream N]\n";
      return EXIT_SUCCESS;
    }
  }

  std::signal(SIGINT, stop_handler);
  std::signal(SIGTERM, stop_handler);

  lmv1::helper::SerialTransport serial;
  std::string error;
  if (!serial.open(device_path, error)) {
    std::cerr << "Device connection failed: " << error << '\n';
    return EXIT_FAILURE;
  }
  std::cout << "Connected to " << serial.path() << '\n';

  lmv1::protocol::Frame hello;
  hello.type = lmv1::protocol::MessageType::kHello;
  hello.sequence = 1;
  hello.payload.assign({'B', 'R', 'I', 'D', 'G', 'E', 1, 0});
  if (!serial.write_frame(hello, error)) {
    std::cerr << "Handshake failed: " << error << '\n';
    return EXIT_FAILURE;
  }
  if (select_computer || select_mic) {
    lmv1::protocol::Frame control;
    control.type = lmv1::protocol::MessageType::kControl;
    control.sequence = 2;
    control.payload = {static_cast<uint8_t>(select_computer ? 1 : 2)};
    if (!serial.write_frame(control, error)) {
      std::cerr << "Mode selection failed: " << error << '\n';
      return EXIT_FAILURE;
    }
  }

  lmv1::helper::AudioPipeline pipeline;
  auto capture = lmv1::helper::make_platform_capture(capture_options);
  if (!capture->start(
          [&pipeline](const float* samples, std::size_t frames, uint8_t channels,
                      uint32_t sample_rate) {
            pipeline.push(samples, frames, channels, sample_rate);
          }, error)) {
    std::cerr << "Audio capture failed: " << error << '\n';
    return EXIT_FAILURE;
  }
  std::cout << "System audio capture active; press Ctrl-C to stop\n";

  uint32_t ping_sequence = 2;
  auto next_ping = std::chrono::steady_clock::now();
  auto next_report = next_ping + std::chrono::seconds(1);
  uint64_t audio_frames_sent = 0;
  while (running) {
    lmv1::protocol::Frame audio;
    while (pipeline.pop(audio)) {
      if (!serial.write_frame(audio, error)) {
        std::cerr << "USB write failed: " << error << '\n';
        running = false;
        break;
      }
      ++audio_frames_sent;
    }
    std::vector<lmv1::protocol::Frame> incoming;
    serial.read_frames(incoming);
    for (const auto& frame : incoming) {
      if (frame.type == lmv1::protocol::MessageType::kCapabilities)
        std::cout << "ESP32 capabilities received\n";
      else if (frame.type == lmv1::protocol::MessageType::kStatus &&
               frame.payload.size() >= 36) {
        std::cout << "Meter M " << read_float_le(&frame.payload[0])
                  << " / S " << read_float_le(&frame.payload[4])
                  << " / I " << read_float_le(&frame.payload[8])
                  << " LUFS / TP " << read_float_le(&frame.payload[16])
                  << " dBTP\n";
        if (frame.payload.size() >= 41) {
          const uint8_t hardware = frame.payload[36];
          std::cout << "Hardware LCD " << ((hardware & 1) ? "OK" : "FAIL")
                    << " / Touch " << ((hardware & 2) ? "OK" : "FAIL")
                    << " / Mic " << ((hardware & 4) ? "OK" : "FAIL")
                    << " / SPL " << read_float_le(&frame.payload[37]) << " dB\n";
        }
      }
    }
    if (std::chrono::steady_clock::now() >= next_ping) {
      lmv1::protocol::Frame ping;
      ping.type = lmv1::protocol::MessageType::kPing;
      ping.sequence = ping_sequence++;
      serial.write_frame(ping, error);
      next_ping = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    }
    if (std::chrono::steady_clock::now() >= next_report) {
      std::cout << "Audio packets sent: " << audio_frames_sent
                << ", dropped blocks: " << pipeline.dropped_blocks() << '\n';
      next_report = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  capture->stop();
  std::cout << "Stopped. Dropped audio blocks: " << pipeline.dropped_blocks() << '\n';
  return EXIT_SUCCESS;
}
