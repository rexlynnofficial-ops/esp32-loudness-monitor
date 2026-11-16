#include "test_support.h"
#include "ui/render_invalidation.h"
#include "ui/waveform_path.h"

int main() {
  lmv1::ui::RenderInvalidation invalidation;
  CHECK(invalidation.needs_full_clear(0, false));
  CHECK(!invalidation.needs_full_clear(0, false));
  CHECK(!invalidation.needs_full_clear(0, false));
  CHECK(invalidation.needs_full_clear(1, false));
  CHECK(!invalidation.needs_full_clear(1, false));
  CHECK(invalidation.needs_full_clear(1, true));
  CHECK(!invalidation.needs_full_clear(1, true));
  CHECK(invalidation.needs_full_clear(1, false));
  CHECK(lmv1::ui::waveform_y(0.0F) == 154);
  CHECK(lmv1::ui::waveform_y(2.0F) == 66);
  CHECK(lmv1::ui::waveform_y(-2.0F) == 242);
  return EXIT_SUCCESS;
}
