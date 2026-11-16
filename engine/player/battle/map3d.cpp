#include "battle/map3d.h"

#include <algorithm>
#include <cmath>

#include "battle/battle_draw.h"
#include "show3d.h"
#include "ui/text.h"

namespace ote {

namespace {
Tint3D tintOf(Color c) { return Tint3D(c.r, c.g, c.b, c.a); }
int elevOr(const MapProj& p, const Grid& g, int x, int y) {
  return x < 0 || y < 0 || x >= g.width() || y >= g.height() ? -1 : p.elevation(x, y);
}
}  // namespace

Map3D::Map3D(Renderer3D& r3d, Show3D& show, const std::string& gameDir, const Graphics& graphics)
    : r3d_(r3d), show_(show) {
  sheets_.init(gameDir, &graphics);
  models_.init(gameDir);
}

Map3D::~Map3D() {
  if (ground_) r3d_.destroyMesh(ground_);
  for (ModelActor& a : unitActors_) a.release(r3d_);
  for (ModelActor& a : structureActors_) a.release(r3d_);
  models_.release(r3d_);
  batch_.release(r3d_);
  sheets_.release(r3d_);
}

Map3D::Spot Map3D::spot(const DrawCtx& c, float gx, float gy, float elevation) const {
  const MapProj& g = *c.proj;
  return Spot{gx - g.gridW() * 0.5f, elevation * g.stepY(), gy - g.gridH() * 0.5f};
}

// The 3D texture and texture positions of a piece of a look's sheet; false = not usable in 3D.
bool Map3D::picture(const DrawCtx&, const Look& look, int x, int y, int w, int h, Material3D& m, float uv[4]) {
  m.texture = sheets_.texture(r3d_, look.sheet);
  if (!m.texture) return false;
  m.smooth = sheets_.smooth(look.sheet);
  sheets_.uv(look.sheet, x, y, w, h, uv[0], uv[1], uv[2], uv[3]);
  return true;
}

// A flat rectangle lying on the ground: squares (x, y) .. (x + w, y + d) at height Y, pulled in by `inset`.
void Map3D::groundQuad(MeshData& m, const DrawCtx& c, int x, int y, float w, float d, float Y, float inset, Tint3D tint,
                       const float* uv) {
  Spot a = spot(c, x + inset, y + inset, 0), b = spot(c, x + w - inset, y + d - inset, 0);
  addQuad(m, Vec3(a.X, Y, b.Z), Vec3(b.X, Y, b.Z), Vec3(b.X, Y, a.Z), Vec3(a.X, Y, a.Z), tint, uv ? uv[0] : 0,
          uv ? uv[1] : 0, uv ? uv[2] : 1, uv ? uv[3] : 1);
}

// The outline of a footprint lying on the ground, `thick` squares wide.
void Map3D::frame(MeshData& m, const DrawCtx& c, int x, int y, int w, int d, float Y, float thick, Tint3D tint) {
  Spot a = spot(c, (float)x, (float)y, 0), b = spot(c, (float)(x + w), (float)(y + d), 0);
  auto bar = [&](float x0, float z0, float x1, float z1) {
    addQuad(m, Vec3(x0, Y, z1), Vec3(x1, Y, z1), Vec3(x1, Y, z0), Vec3(x0, Y, z0), tint);
  };
  bar(a.X, a.Z, b.X, a.Z + thick);
  bar(a.X, b.Z - thick, b.X, b.Z);
  bar(a.X, a.Z + thick, a.X + thick, b.Z - thick);
  bar(b.X - thick, a.Z + thick, b.X, b.Z - thick);
}

// A picture standing upright on `feet`, facing the camera, of a size given in
// screen pixels; `towards` moves it a little towards the camera (layers).
void Map3D::upright(MeshData& m, Spot feet, float wPx, float hPx, float towards, Tint3D tint, const float* uv) {
  float hw = wPx / pixels_ * 0.5f, h = hPx / (pixels_ * cosP_);
  Vec3 p(feet.X + cx_ * towards, feet.Y, feet.Z + cz_ * towards), r(rx_ * hw, 0, rz_ * hw), up(0, h, 0);
  addQuad(m, p - r, p + r, p + r + up, p - r + up, tint, uv ? uv[0] : 0, uv ? uv[1] : 0, uv ? uv[2] : 1, uv ? uv[3] : 1);
}

// The blocks: every side face that shows, and the tops of squares without a
// usable picture. Fixed for the whole battle (terrain does not change).
void Map3D::buildGround(const DrawCtx& c) {
  built_ = true;
  const Battle& b = c.battle;
  const MapProj& p = *c.proj;
  const MapProj& g = p;
  MeshData m;
  for (int y = 0; y < g.gridH(); ++y)
    for (int x = 0; x < g.gridW(); ++x) {
      Cell cell = b.grid.at(x, y);
      int e = p.elevation(x, y);
      Color col = Color::rgb(terrainRgb(c, b.cells[cell].terrain));
      Tint3D side = tintOf(mix(col, Color(40, 30, 24), 0.35f));
      Spot a = spot(c, (float)x, (float)y, (float)e), z = spot(c, (float)(x + 1), (float)(y + 1), (float)e);
      float hi = a.Y;
      const Look* art = terrainArt(c, b.cells[cell].terrain);
      if (!art || !sheets_.texture(r3d_, art->sheet)) groundQuad(m, c, x, y, 1, 1, hi, 0, tintOf(col));
      // Sides, down to the neighbour's top (the rim of the map shows one level below the lowest ground).
      auto lowOf = [&](int nx, int ny) { return elevOr(p, b.grid, nx, ny) * g.stepY(); };
      float lo;
      if ((lo = lowOf(x, y + 1)) < hi) addQuad(m, Vec3(a.X, lo, z.Z), Vec3(z.X, lo, z.Z), Vec3(z.X, hi, z.Z), Vec3(a.X, hi, z.Z), side);
      if ((lo = lowOf(x, y - 1)) < hi) addQuad(m, Vec3(z.X, lo, a.Z), Vec3(a.X, lo, a.Z), Vec3(a.X, hi, a.Z), Vec3(z.X, hi, a.Z), side);
      if ((lo = lowOf(x + 1, y)) < hi) addQuad(m, Vec3(z.X, lo, z.Z), Vec3(z.X, lo, a.Z), Vec3(z.X, hi, a.Z), Vec3(z.X, hi, z.Z), side);
      if ((lo = lowOf(x - 1, y)) < hi) addQuad(m, Vec3(a.X, lo, a.Z), Vec3(a.X, lo, z.Z), Vec3(a.X, hi, z.Z), Vec3(a.X, hi, a.Z), side);
    }
  ground_ = r3d_.createMesh(m);
}

void Map3D::updateHidden(const DrawCtx& c) {
  const MapProj& g = *c.proj;
  // What covers what only changes when the camera does.
  float key[7] = {g.yaw(), 0, 0, 0, 0, 0, 0};
  if (g.free3d()) {
    const Free3DGeometry& f = g.cam3();
    key[1] = f.pitch; key[2] = f.distance; key[3] = f.focusX; key[4] = f.focusY; key[5] = f.focusZ; key[6] = 1;
  }
  bool same = hidden_.size() == (size_t)(g.gridW() * g.gridH());
  for (int i = 0; i < 7 && same; ++i) same = key[i] == hiddenKey_[i];
  if (same) return;
  for (int i = 0; i < 7; ++i) hiddenKey_[i] = key[i];
  hidden_.assign((size_t)(g.gridW() * g.gridH()), 0);
  bool flat = true;  // flat map: nothing can cover anything
  for (int e : g.elevations()) flat = flat && e == 0;
  if (flat) return;
  for (int y = 0; y < g.gridH(); ++y)
    for (int x = 0; x < g.gridW(); ++x) hidden_[(size_t)(y * g.gridW() + x)] = g.hidden3(x, y);
}

void Map3D::beginFrame(Renderer& r, const DrawCtx& c) {
  const MapProj& p = *c.proj;
  const Layout& L = c.layout;
  float s = std::sin(p.yaw()), co = std::cos(p.yaw());
  pixels_ = p.artPixels();
  cosP_ = p.uprightSquash();
  rx_ = co;
  rz_ = -s;
  cx_ = s;
  cz_ = co;

  Frame3D f;
  f.x = 0;
  f.y = 0;
  f.width = L.vw;
  f.height = L.vh;
  show_.fit(r, r3d_, f);  // the picture itself may be bigger than this (the display's pixels); the camera does not care
  Color bg = c.style && c.style->hasBackground ? Color::rgb(c.style->background) : Color(10, 10, 16);
  f.clear = Rgba3D(bg.r / 255.0f, bg.g / 255.0f, bg.b / 255.0f);
  if (p.free3d()) {
    // The camera of Free3DGeometry (tests/test_free3d.cpp holds the two together).
    const Free3DGeometry& g = p.cam3();
    float ex, ey, ez;
    g.eye(ex, ey, ez);
    f.view = Mat4::lookAt(Vec3(ex, ey, ez), Vec3(g.focusX, g.focusY, g.focusZ), Vec3(0, 1, 0));
    float reach = g.distance + std::sqrt((float)(g.gridW * g.gridW + g.gridH * g.gridH)) + (g.levels + 2) * g.stepY;
    f.proj = Mat4::perspective(g.fovY, (float)L.vw / (float)L.vh, Free3DGeometry::kNear, reach);
  } else {
    // The camera of Iso3DGeometry: far enough out that the whole map is in front of it.
    const Iso3DGeometry& g = p.geo3();
    float reach = g.radius() / g.pixels + 4.0f, cosP = g.cosPitch();
    Vec3 eye(s * cosP * reach, g.sinPitch * reach, co * cosP * reach);
    f.view = Mat4::lookAt(eye, Vec3(0, 0, 0), Vec3(0, 1, 0));
    // Scrolling: the map's middle (world pixel radius, radius) goes where the 2D camera puts it.
    float R = g.radius(), shiftX = R - c.cam.ix() - L.vw * 0.5f, shiftY = R - c.cam.iy() - L.vh * 0.5f;
    f.proj = Mat4::translate(Vec3(2.0f * shiftX / L.vw, -2.0f * shiftY / L.vh, 0)) *
             Mat4::ortho(L.vh / (2.0f * g.pixels), (float)L.vw / (float)L.vh, 0.5f, reach * 2.0f);
  }
  // Light fixed to the map (faces change shade as the view turns). Chosen so
  // that tops are lit at exactly full strength: pictures there look as drawn.
  Vec3 toSun = normalize(Vec3(-0.5f, 1.0f, 0.35f));
  f.sunDir = toSun * -1.0f;
  float k = 0.5f / toSun.y;
  f.ambient = Rgba3D(0.5f, 0.5f, 0.5f);
  f.sun = Rgba3D(k, k, k);
  r3d_.begin(f);
}

void Map3D::draw(Renderer& r, const DrawCtx& c, const Overlay& ov, Cell cursor, bool showCursor) {
  if (!built_) {
    buildGround(c);
    markStands(c);
  }
  updateHidden(c);
  beginFrame(r, c);
  if (ground_) r3d_.draw(ground_, Mat4(), Material3D());
  drawTops(c);
  drawStands(c, ov);
  drawStructures(c);
  drawHighlights(c, ov, cursor, showCursor);
  const Battle& b = c.battle;
  int t = c.proj->tile();
  for (int pass = 0; pass < 2; ++pass)  // solid first; then, faintly, those standing behind blocks
    for (const Unit& u : b.units) {
      if (u.id >= (int)c.view.units.size() || !c.view.units[u.id].visible) continue;
      const UnitView& v = c.view.units[u.id];
      if (pass == 0) { drawUnit(c, u, v, 1.0f, false); continue; }
      int ux = std::max(0, std::min((int)std::lround(v.px / t), b.grid.width() - 1));
      int uy = std::max(0, std::min((int)std::lround(v.py / t), b.grid.height() - 1));
      // Behind higher blocks: faint. Behind standing terrain (trees, a peak): a little stronger, a unit in a forest
      // is where the player is looking.
      if (hidden_[(size_t)(uy * b.grid.width() + ux)]) drawUnit(c, u, v, 0.45f, true);
      else if (standInFront(c, ux, uy)) drawUnit(c, u, v, 0.6f, true);
    }
  batch_.flush(r3d_);
  r3d_.end();
  show_.show(r, r3d_, 0, 0, c.layout.vw, c.layout.vh);
  drawOnTop(r, c);
}

}  // namespace ote
