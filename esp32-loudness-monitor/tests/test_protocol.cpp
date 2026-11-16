#include "protocol/wire_protocol.h"
#include "test_support.h"

#include <array>
#include <cstdint>
#include <vector>

int main() {
  using namespace lmv1::protocol;

  Frame source;
  source.type = MessageType::kPing;
  source.flags = 3;
  source.sequence = 42;
  source.timestamp_us = 123456789;
  source.payload = {0x10, 0x20, 0x30, 0x40};

  const auto encoded = encode_frame(source);
  CHECK(encoded.size() == kHeaderSize + source.payload.size() + kCrcSize);

  StreamDecoder decoder;
  std::vector<Frame> decoded;
  for (uint8_t byte : encoded) {
    decoder.push(&byte, 1, decoded);
  }
  CHECK(decoded.size() == 1);
  CHECK(decoded[0] == source);
  CHECK(decoder.stats().valid_frames == 1);

  std::vector<uint8_t> two_frames = encoded;
  two_frames.insert(two_frames.end(), encoded.begin(), encoded.end());
  decoded.clear();
  decoder.push(two_frames.data(), two_frames.size(), decoded);
  CHECK(decoded.size() == 2);

  auto corrupt = encoded;
  corrupt[kHeaderSize] ^= 0x80;
  decoded.clear();
  decoder.push(corrupt.data(), corrupt.size(), decoded);
  CHECK(decoded.empty());
  CHECK(decoder.stats().crc_errors == 1);

  std::array<uint8_t, 7> noise{{1, 2, 'L', 'M', 9, 8, 7}};
  decoded.clear();
  decoder.push(noise.data(), noise.size(), decoded);
  decoder.push(encoded.data(), encoded.size(), decoded);
  CHECK(decoded.size() == 1);
  CHECK(decoder.stats().discarded_bytes >= noise.size());

  return EXIT_SUCCESS;
}

