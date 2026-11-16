#pragma once

#include <Arduino.h>

#include <vector>

#include "../protocol/wire_protocol.h"

namespace lmv1::transport {

class UsbTransport {
 public:
  void begin();
  void poll(std::vector<lmv1::protocol::Frame>& frames);
  bool send(lmv1::protocol::MessageType type, const std::vector<uint8_t>& payload,
            uint16_t flags = 0);
  const lmv1::protocol::DecoderStats& decoder_stats() const {
    return decoder_.stats();
  }

 private:
  lmv1::protocol::StreamDecoder decoder_;
  uint32_t next_sequence_ = 1;
};

}  // namespace lmv1::transport
