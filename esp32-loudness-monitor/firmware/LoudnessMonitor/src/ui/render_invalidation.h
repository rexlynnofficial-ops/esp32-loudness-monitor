#pragma once

#include <cstdint>

namespace lmv1::ui {

class RenderInvalidation {
 public:
  bool needs_full_clear(uint8_t page, bool prompt_visible) {
    const bool changed = !initialized_ || page != page_ ||
                         prompt_visible != prompt_visible_;
    initialized_ = true;
    page_ = page;
    prompt_visible_ = prompt_visible;
    return changed;
  }

 private:
  bool initialized_ = false;
  uint8_t page_ = 0;
  bool prompt_visible_ = false;
};

}  // namespace lmv1::ui

