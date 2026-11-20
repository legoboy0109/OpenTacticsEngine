// The optional on-screen diagnostics of the original Xbox build (OTE_XBOX_OVERLAY=1 in args.txt): frame time, memory and
// the newest log lines, drawn through the normal Renderer so they follow the safe area like everything else.
#ifdef OTE_NXDK
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>

#include <cstdio>
#include <cstdlib>
#include <string>

#include "platform/nxdk/nxdk.h"
#include "platform/platform.h"

namespace ote {

void nxdkDrawOverlay(Renderer& r) {
  static int enabled = -1;
  if (enabled < 0) {
    const char* e = std::getenv("OTE_XBOX_OVERLAY");
    enabled = e && *e && *e != '0';
  }
  if (!enabled) return;
  static const double freq = (double)KeQueryPerformanceFrequency();
  static double last = 0, avg = 0;
  static unsigned long frames = 0;
  static std::string mem;
  const double now = (double)KeQueryPerformanceCounter() * 1000.0 / freq;
  if (last > 0) avg = avg * 0.9 + (now - last) * 0.1;
  last = now;
  if ((frames++ % 30) == 0) mem = nxdkMemoryStatus();  // the kernel call is cheap but not free
  char head[96];
  const int tenths = (int)(avg * 10.0 + 0.5);  // (no %f in nxdk's printf)
  std::snprintf(head, sizeof head, "%d.%d ms/frame  %s  %dx%d", tenths / 10, tenths % 10, mem.c_str(), r.width(), r.height());
  std::vector<std::string> lines = nxdkRecentLog(6);
  const int lh = r.lineHeight();
  const int h = lh * (int)(lines.size() + 1) + 4;
  r.fillRect(0, r.height() - h, r.width(), h, Color(0, 0, 0, 170));
  int y = r.height() - h + 2;
  r.drawText(2, y, head, Color(255, 255, 0));
  for (const std::string& l : lines) {
    y += lh;
    r.drawText(2, y, l.c_str(), Color(220, 220, 220));
  }
}

}  // namespace ote
#endif  // OTE_NXDK
