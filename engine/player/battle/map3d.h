// The battle map in the 3D kinds of view (docs/VIEWS.md "3D isometric" and
// "Full 3D"): real blocks drawn through the 3D drawing layer, by a turning
// camera without perspective or by a free one with it, then letters, bars and
// numbers on top with the 2D renderer. Where things are comes from MapProj
// (Iso3DGeometry / Free3DGeometry), so pointer picking, the camera and this
// drawer always agree.
#pragma once
#include <string>
#include <vector>

#include "battle/batch3d.h"
#include "battle/models3d.h"
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
  void beginFrame(Renderer& r, const DrawCtx& c);
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
  // map3d_stand.cpp: what stands on the ground of a terrain (docs/GRAPHICS.md "Standing terrain")
  const StandLook* standOf(const DrawCtx& c, int terrain) const;
  void markStands(const DrawCtx& c);
  bool standAt(Cell cell) const;
  bool standInFront(const DrawCtx& c, int x, int y) const;
  float standAhead(const DrawCtx& c, Cell cell);
  void drawStands(const DrawCtx& c, const Overlay& ov);
  ModelActor* standActor(const ModelAsset* asset);
  // map3d_models.cpp
  const ModelLook* unitModel(const DrawCtx& c, const Unit& u, const ModelAsset*& asset);
  const ModelLook* structureModel(const DrawCtx& c, const StructureState& st, const ModelAsset*& asset);
  void animateUnit(const DrawCtx& c, const Unit& u, const UnitView& v, const ModelLook& look, const ModelAsset& asset);
  void drawUnitModel(const DrawCtx& c, const Unit& u, const UnitView& v, const ModelLook& look, const ModelAsset& asset,
                     Spot feet, float towards, float fade, bool through);
  void drawStructureModel(const DrawCtx& c, int index, const ModelLook& look, const ModelAsset& asset, float Y);
  float unitFacing(const DrawCtx& c, const Unit& u) const;

  Renderer3D& r3d_;
  Show3D& show_;
  Sheets3D sheets_;
  Batch3D batch_;
  Models3D models_;
  std::vector<ModelActor> unitActors_, structureActors_;  // by unit id / structure index
  std::vector<std::pair<const ModelAsset*, ModelActor>> standActors_;  // one per standing model file (they do not animate)
  std::vector<uint8_t> standAt_;  // per cell: the terrain has a standing part
  bool anyStand_ = false;
  MeshId ground_ = 0;
  bool built_ = false;
  std::vector<uint8_t> hidden_;  // per square: blocks nearer the camera cover its middle
  float hiddenKey_[7] = {-1000, 0, 0, 0, 0, 0, 0};  // the camera hidden_ was worked out for
  // Per frame: the camera's sideways and towards-the-camera directions on the ground, and pixel measures.
  float rx_ = 1, rz_ = 0, cx_ = 0, cz_ = 1, pixels_ = 34, cosP_ = 0.866f;
};

}  // namespace ote
