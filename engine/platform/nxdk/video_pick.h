// Which video mode the original Xbox player asks for (docs/XBOX_OG.md "Picture"). No nxdk in here: the caller lists the
// modes the console offers (XVideoListModes) and says what the dashboard / EEPROM settings allow, so it is unit-tested
// (tests/test_video_pick.cpp).
#pragma once
#include <string>
#include <vector>

namespace ote {

struct VideoModeInfo {
  int w = 0, h = 0;
};

struct VideoWish {
  bool widescreen = false;  // the dashboard's 16:9 setting (VIDEO_WIDESCREEN in XVideoGetEncoderSettings())
  // "auto" (the default: 1280x720 when the console offers it and the TV is set to widescreen, else 640x480),
  // "720p" (720 when offered), "480" (640x480 always). Anything else is "auto".
  std::string preference = "auto";
};

// Index into `modes` of the mode to set, or -1 when the list has nothing usable (the caller then keeps what the
// console is showing). Only 640x480 and 1280x720 are ever chosen: the engine's canvas scales cleanly to them.
int pickVideoMode(const std::vector<VideoModeInfo>& modes, const VideoWish& wish);

}  // namespace ote
