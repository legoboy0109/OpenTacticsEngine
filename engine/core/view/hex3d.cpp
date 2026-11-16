#include "core/view/hex3d.h"

namespace ote {

void HexSpace::middle(int x, int y, float& X, float& Z) const {
  int mx, my;
  geo.middle(x, y, mx, my);
  toSpace((float)mx, (float)my, X, Z);
}

void HexSpace::corner(int x, int y, int i, float& X, float& Z) const {
  float c[6][2];
  geo.corners(x, y, c);
  toSpace(c[i][0], c[i][1], X, Z);
}

int HexSpace::cellAt(float X, float Z) const {
  int x, y;
  geo.nearestXY((X + width() * 0.5f) * (float)geo.T, (Z + height() * 0.5f) * (float)geo.T, x, y);
  return x < 0 || y < 0 || x >= geo.cols || y >= geo.rows ? -1 : y * geo.cols + x;
}

}  // namespace ote
