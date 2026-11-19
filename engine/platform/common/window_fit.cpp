#include "platform/common/window_fit.h"

namespace ote {

int fitWindowScale(int logicalW, int logicalH, int usableW, int usableH) {
  if (logicalW < 1 || logicalH < 1 || usableW <= 0 || usableH <= 0) return 1;
  int s = 1;
  while (s < kMaxWindowScale && logicalW * (s + 1) + kWindowFrameW <= usableW &&
         logicalH * (s + 1) + kWindowFrameH <= usableH)
    ++s;
  return s;
}

}  // namespace ote
