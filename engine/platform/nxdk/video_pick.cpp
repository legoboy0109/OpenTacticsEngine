#include "platform/nxdk/video_pick.h"

namespace ote {

int pickVideoMode(const std::vector<VideoModeInfo>& modes, const VideoWish& wish) {
  int i480 = -1, i720 = -1;
  for (size_t i = 0; i < modes.size(); ++i) {
    if (modes[i].w == 640 && modes[i].h == 480 && i480 < 0) i480 = (int)i;
    if (modes[i].w == 1280 && modes[i].h == 720 && i720 < 0) i720 = (int)i;
  }
  bool want720;
  if (wish.preference == "480") want720 = false;
  else if (wish.preference == "720p") want720 = true;
  else want720 = wish.widescreen;
  if (want720 && i720 >= 0) return i720;
  if (i480 >= 0) return i480;
  return i720;  // 480 not listed (not seen on a real console): 720 is better than nothing
}

}  // namespace ote
