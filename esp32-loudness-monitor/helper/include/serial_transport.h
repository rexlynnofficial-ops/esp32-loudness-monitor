#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "protocol/wire_protocol.h"

namespace lmv1::helper {

class SerialTransport {
 public:
  ~SerialTransport();
  bool open(const std::string& requested_path, std::string& error);
  void close();
  bool is_open() const;
  bool write_frame(const protocol::Frame& frame, std::string& error);
  void read_frames(std::vector<protocol::Frame>& frames);
  const std::string& path() const { return path_; }

 private:
  intptr_t handle_ = -1;
  std::string path_;
  protocol::StreamDecoder decoder_;
};

}  // namespace lmv1::helper
