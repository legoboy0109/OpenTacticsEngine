// Where a speech bubble goes (docs/EVENTS.md "Speech bubbles"): pure arithmetic on rectangles, kept free of drawing so it can be
// tested. The bubble (with a portrait beside it, when the game wants one) goes above the speaker when it fits, else below, moved
// sideways to stay inside the view; a tail runs from the bubble's edge to the speaker. It covers the speaker only when the view has
// no room on either side (then on the side with more room, kept inside the view).
#pragma once

namespace ote {

struct BubbleAnchor {
  int x = 0, y = 0, w = 0, h = 0;  // the speaker on the screen (its square, raised by the unit's height where it stands up)
};

struct BubbleRequest {
  int areaX = 0, areaY = 0, areaW = 0, areaH = 0;  // where it may go (the map's part of the screen)
  int bubbleW = 0, bubbleH = 0;                    // the bubble itself
  int sideW = 0, sideH = 0;                        // a portrait beside it, on its left (0 = none)
  int gap = 2;                                     // between the portrait and the bubble
  int margin = 2;                                  // kept clear of the area's edges
  int tail = 6;                                    // the tail's length when there is room
};

struct BubblePlace {
  int x = 0, y = 0;            // the bubble's top-left corner
  int sideX = 0, sideY = 0;    // the portrait's (when sideW > 0)
  bool below = false;          // under the speaker: the tail points up
  int baseX = 0, baseY = 0;    // the middle of the tail where it leaves the bubble's edge
  int tipX = 0, tipY = 0;      // where it points (the speaker's top or bottom edge)
  bool covers = false;         // the bubble or the portrait overlaps the speaker (no room anywhere)
  bool hasTail() const { return below ? tipY < baseY : tipY > baseY; }
};

BubblePlace placeBubble(const BubbleRequest& q, const BubbleAnchor& a);

}  // namespace ote
