#include "serial_transport.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <cerrno>
#include <cstring>

namespace lmv1::helper {

SerialTransport::~SerialTransport() { close(); }

bool SerialTransport::open(const std::string& requested_path, std::string& error) {
#if defined(_WIN32)
  if (requested_path.empty()) {
    error = "pass --device COM3 (or another ESP32 COM port)";
    return false;
  }
  std::string device = requested_path;
  if (device.rfind("COM", 0) == 0 && device.size() > 4)
    device = "\\\\.\\" + device;
  HANDLE serial = CreateFileA(device.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                              nullptr, OPEN_EXISTING, 0, nullptr);
  if (serial == INVALID_HANDLE_VALUE) {
    error = "unable to open " + requested_path;
    return false;
  }
  DCB settings{};
  settings.DCBlength = sizeof(settings);
  if (!GetCommState(serial, &settings)) {
    CloseHandle(serial);
    error = "GetCommState failed";
    return false;
  }
  settings.BaudRate = 2000000;
  settings.ByteSize = 8;
  settings.StopBits = ONESTOPBIT;
  settings.Parity = NOPARITY;
  if (!SetCommState(serial, &settings)) {
    CloseHandle(serial);
    error = "SetCommState failed";
    return false;
  }
  COMMTIMEOUTS timeouts{};
  timeouts.ReadIntervalTimeout = MAXDWORD;
  SetCommTimeouts(serial, &timeouts);
  handle_ = reinterpret_cast<intptr_t>(serial);
  path_ = requested_path;
  return true;
#else
  std::string selected = requested_path;
  if (selected.empty()) {
    DIR* directory = opendir("/dev");
    if (directory != nullptr) {
      while (dirent* entry = readdir(directory)) {
        const std::string name = entry->d_name;
        if (name.rfind("cu.usbmodem", 0) == 0) {
          selected = "/dev/" + name;
          break;
        }
      }
      closedir(directory);
    }
  }
  if (selected.empty()) {
    error = "no /dev/cu.usbmodem* device found; pass --device PATH";
    return false;
  }
  handle_ = ::open(selected.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (handle_ < 0) {
    error = selected + ": " + std::strerror(errno);
    return false;
  }
  termios options{};
  if (tcgetattr(handle_, &options) == 0) {
    cfmakeraw(&options);
    // The ESP32 transport is USB CDC, but some macOS serial drivers still
    // apply the configured line rate while draining their device queue.
    // Use 2 Mbit/s so the stereo PCM stream has headroom.
    options.c_ispeed = 2000000;
    options.c_ospeed = 2000000;
    options.c_cflag |= CLOCAL | CREAD;
    tcsetattr(handle_, TCSANOW, &options);
  }
  path_ = selected;
  return true;
#endif
}

void SerialTransport::close() {
#if defined(_WIN32)
  if (handle_ >= 0) CloseHandle(reinterpret_cast<HANDLE>(handle_));
#else
  if (handle_ >= 0) ::close(handle_);
#endif
  handle_ = -1;
  path_.clear();
}

bool SerialTransport::is_open() const { return handle_ >= 0; }

bool SerialTransport::write_frame(const protocol::Frame& frame, std::string& error) {
  if (!is_open()) {
    error = "serial transport is closed";
    return false;
  }
  const auto bytes = protocol::encode_frame(frame);
#if defined(_WIN32)
  std::size_t offset = 0;
  while (offset < bytes.size()) {
    DWORD written = 0;
    if (!WriteFile(reinterpret_cast<HANDLE>(handle_), bytes.data() + offset,
                   static_cast<DWORD>(bytes.size() - offset), &written, nullptr)) {
      error = "WriteFile failed";
      return false;
    }
    offset += written;
  }
  return true;
#else
  std::size_t offset = 0;
  while (offset < bytes.size()) {
    const ssize_t count = ::write(handle_, bytes.data() + offset, bytes.size() - offset);
    if (count > 0) offset += static_cast<std::size_t>(count);
    else if (errno == EAGAIN || errno == EINTR) usleep(1000);
    else {
      error = std::strerror(errno);
      return false;
    }
  }
  return true;
#endif
}

void SerialTransport::read_frames(std::vector<protocol::Frame>& frames) {
#if defined(_WIN32)
  if (handle_ < 0) return;
  COMSTAT status{};
  DWORD errors = 0;
  if (!ClearCommError(reinterpret_cast<HANDLE>(handle_), &errors, &status) ||
      status.cbInQue == 0) return;
  uint8_t bytes[1024];
  DWORD count = 0;
  if (ReadFile(reinterpret_cast<HANDLE>(handle_), bytes,
               std::min<DWORD>(status.cbInQue, sizeof(bytes)), &count, nullptr) && count > 0)
    decoder_.push(bytes, count, frames);
#else
  uint8_t bytes[1024];
  while (handle_ >= 0) {
    const ssize_t count = ::read(handle_, bytes, sizeof(bytes));
    if (count <= 0) break;
    decoder_.push(bytes, static_cast<std::size_t>(count), frames);
  }
#endif
}

}  // namespace lmv1::helper
