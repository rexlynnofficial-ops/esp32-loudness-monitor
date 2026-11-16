#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace lmv1::protocol {

constexpr std::size_t kHeaderSize = 24;
constexpr std::size_t kCrcSize = 4;
constexpr std::size_t kMaxPayloadSize = 4096;
constexpr uint8_t kProtocolMajor = 1;

enum class MessageType : uint8_t {
  kHello = 1,
  kCapabilities = 2,
  kPcmS16 = 3,
  kObservation = 4,
  kControl = 5,
  kStatus = 6,
  kPing = 7,
  kPong = 8,
};

struct Frame {
  MessageType type = MessageType::kHello;
  uint16_t flags = 0;
  uint32_t sequence = 0;
  uint64_t timestamp_us = 0;
  std::vector<uint8_t> payload;

  bool operator==(const Frame& other) const {
    return type == other.type && flags == other.flags &&
           sequence == other.sequence && timestamp_us == other.timestamp_us &&
           payload == other.payload;
  }
};

struct DecoderStats {
  uint64_t valid_frames = 0;
  uint64_t crc_errors = 0;
  uint64_t version_errors = 0;
  uint64_t oversized_frames = 0;
  uint64_t discarded_bytes = 0;
};

uint32_t crc32(const uint8_t* bytes, std::size_t size);
std::vector<uint8_t> encode_frame(const Frame& frame);

class StreamDecoder {
 public:
  void push(const uint8_t* bytes, std::size_t size, std::vector<Frame>& output);
  const DecoderStats& stats() const { return stats_; }

 private:
  std::vector<uint8_t> buffer_;
  DecoderStats stats_;
};

template <typename T>
void append_le(std::vector<uint8_t>& output, T value) {
  for (std::size_t i = 0; i < sizeof(T); ++i) {
    output.push_back(static_cast<uint8_t>(
        (static_cast<uint64_t>(value) >> (i * 8U)) & 0xffU));
  }
}

template <typename T>
T read_le(const uint8_t* input) {
  uint64_t value = 0;
  for (std::size_t i = 0; i < sizeof(T); ++i) {
    value |= static_cast<uint64_t>(input[i]) << (i * 8U);
  }
  return static_cast<T>(value);
}

}  // namespace lmv1::protocol

