// UWP view setup and memory budget: the only code of the engine that needs C++/CX (/ZW). CMake compiles this one
// file (and SDL's own WinRT entry point) with /ZW /GR; nothing else may include WinRT headers (docs/XBOX.md).
#ifdef OTE_UWP
#include <cstdio>
#include <string>

#include "core/base/log.h"
#include "platform/common/uwp.h"

using namespace Windows::Foundation;
using namespace Windows::System;
using namespace Windows::UI::ViewManagement;

namespace ote {

void uwpPrepareView() {
  // Xbox (and tablets) give an app a view that leaves a margin for TV overscan; the app then draws on a black frame.
  // UseCoreWindow makes the view the full window. The game's picture is scaled to the window by the renderer
  // (SDL_RenderSetLogicalSize), exactly as in a desktop window, so no other choice of size is needed.
  try {
    ApplicationView^ view = ApplicationView::GetForCurrentView();
    view->SetDesiredBoundsMode(ApplicationViewBoundsMode::UseCoreWindow);
    // Xbox renders UWP apps at 200% layout scale (a 1080p screen is a 960x540 window). Turning that off gives a window
    // of the real pixel size, so the picture is scaled once, not twice. Returns false (and does nothing) on a PC.
    bool off = ApplicationViewScaling::TrySetDisableLayoutScaling(true);
    OTE_INFO("view: full-window bounds requested; layout scaling disabled: %s", off ? "yes" : "no (not supported here)");
  } catch (Platform::Exception^ e) {
    OTE_WARN("view setup failed (0x%08x); using the system defaults", (unsigned)e->HResult);
  }
}

std::string uwpMemoryStatus() {
  try {
    double use = (double)MemoryManager::AppMemoryUsage / (1024.0 * 1024.0);
    double limit = (double)MemoryManager::AppMemoryUsageLimit / (1024.0 * 1024.0);
    char buf[160];
    std::snprintf(buf, sizeof(buf), "app memory %.0f MB of limit %.0f MB", use, limit);
    return buf;
  } catch (Platform::Exception^) {
    return "";
  }
}

}  // namespace ote
#endif  // OTE_UWP
