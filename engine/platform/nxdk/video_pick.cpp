#include "platform/nxdk/video_pick.h"

namespace ote {

int pickVideoMode(const std::vector<VideoModeInfo>& modes, const VideoWish& wish) {
  int i480 = -1, i720 = -1;
  for (size_t i = 0; i < modes.size(); ++i) {
    if (modes[i].w == 640 && modes[i].h == 480 && i480 < 0) i480 = (int)i;
    if (modes[i].w == 1280 && modes[i].h == 720 && i720 < 0) i720 = (int)i;
  }
  // 720p only when asked for: the picture is made by the CPU and every frame is copied through the video memory, which is
  // about three times the work of 480 (docs/XBOX_OG.md "Speed"). The widescreen setting does not pick it: a 16:9 TV gets
  // the 640x480 picture stretched by the console (anamorphic), which the fit allows for.
  const bool want720 = wish.preference == "720p";
  if (want720 && i720 >= 0) return i720;
  if (i480 >= 0) return i480;
  return i720;  // 480 not listed (not seen on a real console): 720 is better than nothing
}

}  // namespace ote
