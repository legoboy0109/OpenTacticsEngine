// C library gaps of xsdk's newlib that the engine or Lua reach (docs/XBOX360.md). Only built for the 360.
#include <sys/times.h>
#include <time.h>

extern "C" {

// newlib's clock() (Lua's os.clock, table.sort's seed) calls times(), which xsdk does not provide. Time since the
// first call from the monotonic clock, in clock ticks (CLK_TCK = 1000 in this newlib); only differences matter to callers.
clock_t times(struct tms* buf) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  static long startSec = ts.tv_sec;
  const clock_t ticks = (clock_t)((ts.tv_sec - startSec) * CLK_TCK + ts.tv_nsec / (1000000000L / CLK_TCK));
  if (buf) {
    buf->tms_utime = ticks;
    buf->tms_stime = 0;
    buf->tms_cutime = 0;
    buf->tms_cstime = 0;
  }
  return ticks;
}

}  // extern "C"
