// The battle map in the "isometric-3d" kind of view (docs/VIEWS.md "3D
// isometric"): real blocks drawn through the 3D drawing layer by a camera
// without perspective, then letters, bars and numbers on top with the 2D
// renderer. Where things are comes from MapProj / Iso3DGeometry, so pointer
// picking, the scrolling camera and this drawer always agree.
#pragma once
#include <string>
#include <vector>

#include "battle/batch3d.h"
#include "battle/sheets3d.h"
#include "core/battle/battle.h"
#include "platform/platform.h"

namespace ote {

struct DrawCtx;
struct Overlay;
struct UnitView;
class Show3D;

class Map3D {
 public:
  Map3D(Renderer3D& r3d, Show3D& show, const std::string& gameDir, const Graphics& graphics);
  ~Map3D();  // frees what it made in the 3D layer (which outlives every scene)
  void draw(Renderer& r, const DrawCtx& c, const Overlay& ov, Cell cursor, bool showCursor);

 private:
  struct Spot { float X, Y, Z; };
  // map3d.cpp
  void buildGround(const DrawCtx& c);
  void beginFrame(const DrawCtx& c);
  void updateHidden(const DrawCtx& c);
  Spot spot(const DrawCtx& c, float gx, float gy, float elevation) const;
  bool picture(const DrawCtx& c, const Look& look, int x, int y, int w, int h, Material3D& m, float uv[4]);
  void groundQuad(MeshData& m, const DrawCtx& c, int x, int y, float w, float d, float Y, float inset, Tint3D tint,
                  const float* uv = nullptr);
  void frame(MeshData& m, const DrawCtx& c, int x, int y, int w, int d, float Y, float thick, Tint3D tint);
  void upright(MeshData& m, Spot feet, float wPx, float hPx, float towards, Tint3D tint, const float* uv = nullptr);
  // map3d_things.cpp
  void drawTops(const DrawCtx& c);
  void drawStructures(const DrawCtx& c);
  void drawHighlights(const DrawCtx& c, const Overlay& ov, Cell cursor, bool showCursor);
  void drawUnit(const DrawCtx& c, const Unit& u, const UnitView& v, float fade, bool through);
  void drawOnTop(Renderer& r, const DrawCtx& c);

  Renderer3D& r3d_;
  Show3D& show_;
  Sheets3D sheets_;
  Batch3D batch_;
  MeshId ground_ = 0;
  bool built_ = false;
  std::vector<uint8_t> hidden_;  // per square: blocks nearer the camera cover its middle
  float hiddenYaw_ = -1000;
  // Per frame: the camera's sideways and towards-the-camera directions on the ground, and pixel measures.
  float rx_ = 1, rz_ = 0, cx_ = 0, cz_ = 1, pixels_ = 34, cosP_ = 0.866f;
};

}  // namespace ote
