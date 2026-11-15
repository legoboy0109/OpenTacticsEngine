// 3D models on the 3D isometric map: which model a thing uses, which
// animation it plays for what it is doing, the way it faces, and drawing it.
#include <algorithm>
#include <cmath>

#include "battle/battle_draw.h"
#include "battle/map3d.h"
#include "battle/map3d_sizes.h"
#include "ui/text.h"

namespace ote {

namespace {

const float kPi = 3.14159265f;
enum State { Idle, Move, Attack, Die };

// The shorter way round from angle a to angle b.
float turnTowards(float a, float b, float most) {
  float d = std::fmod(b - a, 2 * kPi);
  if (d > kPi) d -= 2 * kPi;
  if (d < -kPi) d += 2 * kPi;
  return a + std::max(-most, std::min(most, d));
}

Rgba3D rgba(Color c) { return Rgba3D(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f); }

}  // namespace

const ModelLook* Map3D::unitModel(const DrawCtx& c, const Unit& u, const ModelAsset*& asset) {
  const GraphicsSet* set = c.battle.db->graphics.set(c.styleIndex);
  if (!set || u.type < 0 || (size_t)u.type >= set->units.size()) return nullptr;
  const ModelLook& look = set->units[(size_t)u.type].model;
  if (!look.any()) return nullptr;
  asset = models_.get(r3d_, look.file);
  return asset ? &look : nullptr;
}

const ModelLook* Map3D::structureModel(const DrawCtx& c, const StructureState& st, const ModelAsset*& asset) {
  const GraphicsSet* set = c.battle.db->graphics.set(c.styleIndex);
  if (!set || st.type < 0 || (size_t)st.type >= set->structures.size()) return nullptr;
  const ModelLook& look = set->structures[(size_t)st.type].model;
  if (!look.any()) return nullptr;
  asset = models_.get(r3d_, look.file);
  return asset ? &look : nullptr;
}

// The way a unit faces when first seen: towards the nearest enemy (an angle
// about the up axis; 0 = towards grid +y, a quarter turn = towards grid +x).
float Map3D::unitFacing(const DrawCtx& c, const Unit& u) const {
  const Battle& b = c.battle;
  int best = -1, bestD = 1 << 30;
  for (const Unit& o : b.units) {
    if (!o.alive || !b.isEnemy(u, o)) continue;
    int d = b.grid.distance(u.cell, o.cell);
    if (d < bestD) { bestD = d; best = o.id; }
  }
  if (best < 0) return 0;
  return std::atan2((float)(b.grid.x(b.units[best].cell) - b.grid.x(u.cell)), (float)(b.grid.y(b.units[best].cell) - b.grid.y(u.cell)));
}

// Once per frame per unit: follows what BattleView is doing with the unit
// (sliding = moving, lunging = attacking, fading = dying) and picks the
// animation and the facing from it.
void Map3D::animateUnit(const DrawCtx& c, const Unit& u, const UnitView& v, const ModelLook& look, const ModelAsset& asset) {
  if (unitActors_.size() <= (size_t)u.id) unitActors_.resize((size_t)u.id + 1);
  ModelActor& a = unitActors_[(size_t)u.id];
  if (a.lastTime == c.time && a.asset == &asset) return;  // already done this frame
  float dt = a.lastTime < 0 ? 0 : std::max(0.0f, std::min(c.time - a.lastTime, 0.1f));
  a.lastTime = c.time;
  if (!a.placed) {
    a.placed = true;
    a.facing = unitFacing(c, u);
    a.lastX = v.px;
    a.lastY = v.py;
  }
  float dx = v.px - a.lastX, dy = v.py - a.lastY;
  a.lastX = v.px;
  a.lastY = v.py;
  bool moving = std::fabs(dx) + std::fabs(dy) > 0.01f, lunging = std::fabs(v.ox) + std::fabs(v.oy) > 0.01f;
  bool dying = v.alpha < 0.999f;
  const Model3D& m = asset.model;
  auto clipOf = [&](int anim) { return m.findClip(look.clips[anim]); };
  auto lengthOf = [&](int anim) { int k = clipOf(anim); return k < 0 ? 0.0f : m.clips[(size_t)k].duration; };

  int state = a.state;
  if (dying) state = Die;
  else if (lunging && a.state != Attack) { state = Attack; a.time = 0; }
  else if (a.state == Attack && a.time < lengthOf(AnimAttack)) state = Attack;  // let the swing finish
  else state = moving ? Move : Idle;
  if (state != a.state && state != Attack) a.time = 0;
  a.state = state;
  a.time += dt;

  float want = a.facing;
  if (moving) want = std::atan2(dx, dy);
  else if (lunging) want = std::atan2(v.ox, v.oy);
  a.facing = turnTowards(a.facing, want, dt * 14.0f);

  int clip;
  float time = a.time;
  bool loop = true;
  switch (state) {
    case Move: clip = clipOf(AnimMove); break;
    case Attack: clip = clipOf(AnimAttack); loop = false; break;
    case Die:  // the fall follows the fade, however long the view takes over it
      clip = clipOf(AnimDie);
      time = (1.0f - v.alpha) * lengthOf(AnimDie);
      loop = false;
      break;
    default: clip = clipOf(AnimIdle); break;
  }
  // Units of one kind should not breathe in step: shift each one's idle a little.
  if (state == Idle) time += (float)(u.id % 7) * 0.17f;
  poseActor(r3d_, asset, a, clip, time, loop);
}

void Map3D::drawUnitModel(const DrawCtx& c, const Unit& u, const UnitView& v, const ModelLook& look, const ModelAsset& asset,
                          Spot feet, float towards, float fade, bool through) {
  animateUnit(c, u, v, look, asset);
  const ModelActor& a = unitActors_[(size_t)u.id];
  float size = (look.height > 0 ? look.height : kUnitModelHeight) / asset.height * v.scale;
  Mat4 place = Mat4::translate(Vec3(feet.X + cx_ * towards, feet.Y, feet.Z + cz_ * towards)) *
               Mat4::rotate(Quat::axisAngle(Vec3(0, 1, 0), a.facing + look.turn * kPi / 180.0f)) * Mat4::scale(Vec3(size, size, size));
  ModelTint tint;
  tint.side = rgba(Color::rgb(c.battle.sides[(size_t)shownSide(c, u)].color));
  tint.sideMaterial = look.sideMaterial;
  tint.dim = u.done ? 0.55f : 1.0f;  // dimmed: already acted
  tint.alpha = v.alpha * fade;
  tint.depthTest = !through;
  drawModel(r3d_, asset, a, place, tint);
}

void Map3D::drawStructureModel(const DrawCtx& c, int index, const ModelLook& look, const ModelAsset& asset, float Y) {
  const Battle& b = c.battle;
  const StructureState& st = b.structures[(size_t)index];
  const StructureDef& def = b.defOf(st);
  if (structureActors_.size() <= (size_t)index) structureActors_.resize((size_t)index + 1);
  ModelActor& a = structureActors_[(size_t)index];
  poseActor(r3d_, asset, a, asset.model.findClip(look.clips[AnimIdle]), c.time, true);
  float size = look.height > 0 ? look.height / asset.height : kStructureModelFit * std::min(def.width, def.height) / asset.width;
  Spot mid = spot(c, st.x + def.width * 0.5f, st.y + def.height * 0.5f, 0);
  Mat4 place = Mat4::translate(Vec3(mid.X, Y, mid.Z)) * Mat4::rotate(Quat::axisAngle(Vec3(0, 1, 0), look.turn * kPi / 180.0f)) *
               Mat4::scale(Vec3(size, size, size));
  ModelTint tint;
  // Nobody's: a pale neutral colour where the owner's would be.
  const int owner = shownOwner(c, index);  // fog: the owner the viewer last saw
  tint.side = owner >= 0 ? rgba(Color::rgb(b.sides[(size_t)owner].color)) : Rgba3D(0.82f, 0.82f, 0.8f);
  tint.sideMaterial = look.sideMaterial;
  if (structureDimmed(c, index)) tint.dim = kFogDim;  // fog: out of the viewer's sight
  drawModel(r3d_, asset, a, place, tint);
}

}  // namespace ote
