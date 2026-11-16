#include "usb_transport.h"

namespace lmv1::transport {

void UsbTransport::begin() {
  Serial.begin(2000000);
  Serial.setRxBufferSize(8192);
}

void UsbTransport::poll(std::vector<lmv1::protocol::Frame>& frames) {
  uint8_t buffer[512];
  while (Serial.available() > 0) {
    const std::size_t wanted = std::min<std::size_t>(Serial.available(), sizeof(buffer));
    const std::size_t read = Serial.readBytes(reinterpret_cast<char*>(buffer), wanted);
    decoder_.push(buffer, read, frames);
  }
}

bool UsbTransport::send(lmv1::protocol::MessageType type,
                        const std::vector<uint8_t>& payload, uint16_t flags) {
  lmv1::protocol::Frame frame;
  frame.type = type;
  frame.flags = flags;
  frame.sequence = next_sequence_++;
  frame.timestamp_us = micros();
  frame.payload = payload;
  const auto encoded = lmv1::protocol::encode_frame(frame);
  return Serial.write(encoded.data(), encoded.size()) == encoded.size();
}

}  // namespace lmv1::transport
