#include "wire_protocol.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>

namespace lmv1::protocol {
namespace {
constexpr std::array<uint8_t, 4> kMagic{{'L', 'M', 'V', '1'}};

void write_at(std::vector<uint8_t>& output, std::size_t offset,
              uint64_t value, std::size_t width) {
  for (std::size_t i = 0; i < width; ++i) {
    output[offset + i] = static_cast<uint8_t>((value >> (i * 8U)) & 0xffU);
  }
}
}  // namespace

uint32_t crc32(const uint8_t* bytes, std::size_t size) {
  uint32_t crc = 0xffffffffU;
  for (std::size_t i = 0; i < size; ++i) {
    crc ^= bytes[i];
    for (int bit = 0; bit < 8; ++bit) {
      const uint32_t mask = 0U - (crc & 1U);
      crc = (crc >> 1U) ^ (0xedb88320U & mask);
    }
  }
  return ~crc;
}

std::vector<uint8_t> encode_frame(const Frame& frame) {
  if (frame.payload.size() > kMaxPayloadSize) {
    throw std::length_error("wire payload exceeds 4096 bytes");
  }
  std::vector<uint8_t> output(kHeaderSize, 0);
  std::copy(kMagic.begin(), kMagic.end(), output.begin());
  output[4] = kProtocolMajor;
  output[5] = static_cast<uint8_t>(frame.type);
  write_at(output, 6, frame.flags, 2);
  write_at(output, 8, frame.payload.size(), 2);
  write_at(output, 12, frame.sequence, 4);
  write_at(output, 16, frame.timestamp_us, 8);
  output.insert(output.end(), frame.payload.begin(), frame.payload.end());
  append_le<uint32_t>(output, crc32(output.data(), output.size()));
  return output;
}

void StreamDecoder::push(const uint8_t* bytes, std::size_t size,
                         std::vector<Frame>& output) {
  if (bytes == nullptr || size == 0) {
    return;
  }
  buffer_.insert(buffer_.end(), bytes, bytes + size);

  while (buffer_.size() >= kMagic.size()) {
    const auto magic = std::search(buffer_.begin(), buffer_.end(),
                                   kMagic.begin(), kMagic.end());
    if (magic != buffer_.begin()) {
      const std::size_t discard = magic == buffer_.end()
                                      ? buffer_.size() - (kMagic.size() - 1)
                                      : static_cast<std::size_t>(magic - buffer_.begin());
      buffer_.erase(buffer_.begin(), buffer_.begin() + discard);
      stats_.discarded_bytes += discard;
      if (buffer_.size() < kHeaderSize) {
        return;
      }
    }
    if (buffer_.size() < kHeaderSize) {
      return;
    }
    if (buffer_[4] != kProtocolMajor) {
      buffer_.erase(buffer_.begin());
      ++stats_.version_errors;
      ++stats_.discarded_bytes;
      continue;
    }
    const auto payload_size = read_le<uint16_t>(&buffer_[8]);
    if (payload_size > kMaxPayloadSize) {
      buffer_.erase(buffer_.begin());
      ++stats_.oversized_frames;
      ++stats_.discarded_bytes;
      continue;
    }
    const std::size_t frame_size = kHeaderSize + payload_size + kCrcSize;
    if (buffer_.size() < frame_size) {
      return;
    }
    const uint32_t expected = read_le<uint32_t>(&buffer_[frame_size - kCrcSize]);
    const uint32_t actual = crc32(buffer_.data(), frame_size - kCrcSize);
    if (actual != expected) {
      buffer_.erase(buffer_.begin());
      ++stats_.crc_errors;
      ++stats_.discarded_bytes;
      continue;
    }

    Frame frame;
    frame.type = static_cast<MessageType>(buffer_[5]);
    frame.flags = read_le<uint16_t>(&buffer_[6]);
    frame.sequence = read_le<uint32_t>(&buffer_[12]);
    frame.timestamp_us = read_le<uint64_t>(&buffer_[16]);
    frame.payload.assign(buffer_.begin() + kHeaderSize,
                         buffer_.begin() + kHeaderSize + payload_size);
    output.push_back(std::move(frame));
    buffer_.erase(buffer_.begin(), buffer_.begin() + frame_size);
    ++stats_.valid_frames;
  }
}

}  // namespace lmv1::protocol
