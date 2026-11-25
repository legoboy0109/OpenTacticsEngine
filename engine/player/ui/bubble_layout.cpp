#include "ui/bubble_layout.h"

#include <algorithm>

namespace ote {

namespace {
int clampTo(int v, int lo, int hi) { return hi < lo ? lo : std::max(lo, std::min(hi, v)); }
bool overlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
  return aw > 0 && ah > 0 && bw > 0 && bh > 0 && ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}
}  // namespace

BubblePlace placeBubble(const BubbleRequest& q, const BubbleAnchor& a) {
  BubblePlace p;
  const int side = q.sideW > 0 ? q.sideW + q.gap : 0;
  const int blockW = side + q.bubbleW, blockH = std::max(q.bubbleH, q.sideW > 0 ? q.sideH : 0);
  const int top = q.areaY + q.margin, bottom = q.areaY + q.areaH - q.margin;
  const int aboveTop = a.y - q.tail - blockH, belowTop = a.y + a.h + q.tail;
  int blockY;
  if (aboveTop >= top) {
    blockY = aboveTop;
  } else if (belowTop + blockH <= bottom) {
    blockY = belowTop;
    p.below = true;
  } else {  // no room for the whole block on either side: the side with more room, kept inside the area
    p.below = (q.areaY + q.areaH) - (a.y + a.h) > a.y - q.areaY;
    blockY = clampTo(p.below ? belowTop : aboveTop, top, bottom - blockH);
  }
  // Across: the bubble's middle over the speaker's, the whole block kept inside the area.
  const int cx = a.x + a.w / 2;
  const int blockX = clampTo(cx - q.bubbleW / 2 - side, q.areaX + q.margin, q.areaX + q.areaW - q.margin - blockW);
  p.x = blockX + side;
  p.sideX = blockX;
  // Above, the bubble and the portrait stand on the block's bottom (next to the tail); below, they hang from its top.
  p.y = p.below ? blockY : blockY + blockH - q.bubbleH;
  p.sideY = p.below ? blockY : blockY + blockH - q.sideH;
  p.baseY = p.below ? p.y : p.y + q.bubbleH;
  p.baseX = clampTo(cx, p.x + 6, p.x + q.bubbleW - 7);
  p.tipX = clampTo(cx, q.areaX, q.areaX + q.areaW - 1);
  p.tipY = p.below ? a.y + a.h : a.y;
  p.covers = overlap(p.x, p.y, q.bubbleW, q.bubbleH, a.x, a.y, a.w, a.h) ||
             (q.sideW > 0 && overlap(p.sideX, p.sideY, q.sideW, q.sideH, a.x, a.y, a.w, a.h));
  return p;
}

}  // namespace ote
