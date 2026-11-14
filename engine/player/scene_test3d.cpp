#include "scene_test3d.h"

#include <cmath>
#include <cstdio>

#include "core/base/log.h"
#include "gfx3d/shapes.h"
#include "ui/chrome.h"

namespace ote {

namespace {

const int kSize = 8;
// Heights of the squares, in half blocks; -1 = water.
const int kHeights[kSize][kSize] = {
    {2, 2, 1, 1, 0, 0, 0, 0}, {2, 3, 2, 1, 0, 0, 0, 0}, {1, 2, 1, 0, 0, -1, -1, 0}, {1, 1, 0, 0, -1, -1, -1, 0},
    {0, 0, 0, -1, -1, -1, 0, 0}, {0, 0, -1, -1, 0, 0, 1, 1}, {0, 0, 0, 0, 0, 1, 2, 2}, {0, 0, 0, 0, 1, 1, 2, 4},
};
const float kStep = 0.5f;  // height of half a block, in squares
const float kPi = 3.14159265f;

float topOf(int x, int z) { return kHeights[z][x] < 0 ? -0.15f : kHeights[z][x] * kStep; }

// 16 x 16 grass-like tile: two greens in a soft checker with a darker rim.
void groundPicture(std::vector<uint8_t>& px) {
  px.resize(16 * 16 * 4);
  for (int y = 0; y < 16; ++y)
    for (int x = 0; x < 16; ++x) {
      bool rim = x == 0 || y == 0 || x == 15 || y == 15;
      bool alt = ((x / 4) + (y / 4)) % 2 == 0;
      int v = rim ? 170 : alt ? 255 : 228;
      uint8_t* p = &px[(size_t)(y * 16 + x) * 4];
      p[0] = p[1] = p[2] = (uint8_t)v;
      p[3] = 255;
    }
}

// 16 x 32 standing figure: round head, body, two legs; the rest see-through.
void figurePicture(std::vector<uint8_t>& px) {
  px.assign(16 * 32 * 4, 0);
  auto put = [&](int x, int y, int r, int g, int b) {
    uint8_t* p = &px[(size_t)(y * 16 + x) * 4];
    p[0] = (uint8_t)r;
    p[1] = (uint8_t)g;
    p[2] = (uint8_t)b;
    p[3] = 255;
  };
  for (int y = 0; y < 32; ++y)
    for (int x = 0; x < 16; ++x) {
      float dx = x - 7.5f, dy = y - 6.5f;
      if (dx * dx + dy * dy <= 5.2f * 5.2f) put(x, y, 240, 205, 170);                // head
      else if (y >= 12 && y < 23 && x >= 3 && x < 13) put(x, y, 255, 255, 255);      // body (takes the side colour)
      else if (y >= 23 && y < 31 && ((x >= 4 && x < 7) || (x >= 9 && x < 12))) put(x, y, 70, 70, 90);  // legs
    }
}

}  // namespace

Test3DScene::Test3DScene(SceneHost& host) : host_(host), r3d_(host.renderer3d()) { build(); }

Test3DScene::~Test3DScene() {
  for (MeshId m : {ground_, water_, building_, figure_}) r3d_.destroyMesh(m);
  r3d_.destroyTexture(groundTex_);
  r3d_.destroyTexture(figureTex_);
  model_.release(r3d_);
}

void Test3DScene::build() {
  std::vector<uint8_t> px;
  groundPicture(px);
  groundTex_ = r3d_.createTexture(px.data(), 16, 16, true);
  figurePicture(px);
  figureTex_ = r3d_.createTexture(px.data(), 16, 32, false);

  // The map is centred on the origin; a square is one unit.
  MeshData ground, water;
  const float half = kSize * 0.5f;
  for (int z = 0; z < kSize; ++z)
    for (int x = 0; x < kSize; ++x) {
      Vec3 lo(x - half, -1.0f, z - half), hi(x + 1 - half, topOf(x, z), z + 1 - half);
      int h = kHeights[z][x];
      if (h < 0) {
        addBlock(ground, lo, hi, Tint3D(150, 130, 95), Tint3D(120, 100, 75));  // the bed
        addQuad(water, Vec3(lo.x, 0, hi.z), Vec3(hi.x, 0, hi.z), Vec3(hi.x, 0, lo.z), Vec3(lo.x, 0, lo.z),
                Tint3D(70, 140, 235, 150));
      } else {
        int g = 150 + h * 22;
        addBlock(ground, lo, hi, Tint3D(90, (uint8_t)(g > 255 ? 255 : g), 80), Tint3D(140, 105, 70));
      }
    }
  ground_ = r3d_.createMesh(ground);
  water_ = r3d_.createMesh(water);

  // A building: walls and a pitched roof, standing on square (6, 1).
  MeshData b;
  float bx = 6 - half, bz = 1 - half, by = topOf(6, 1);
  addBlock(b, Vec3(bx + 0.1f, by, bz + 0.1f), Vec3(bx + 0.9f, by + 0.7f, bz + 0.9f), Tint3D(215, 205, 185), Tint3D(215, 205, 185));
  Vec3 e0(bx, by + 0.7f, bz), e1(bx + 1, by + 0.7f, bz), e2(bx + 1, by + 0.7f, bz + 1), e3(bx, by + 0.7f, bz + 1);
  Vec3 r0(bx + 0.5f, by + 1.2f, bz), r1(bx + 0.5f, by + 1.2f, bz + 1);
  Tint3D roof(190, 70, 60);
  addQuad(b, e3, r1, r0, e0, roof);  // west slope
  addQuad(b, e1, r0, r1, e2, roof);  // east slope
  addQuad(b, e0, r0, r0, e1, Tint3D(160, 60, 50));  // gables (one corner doubled: a triangle)
  addQuad(b, e2, r1, r1, e3, Tint3D(160, 60, 50));
  building_ = r3d_.createMesh(b);

  // A standing picture, one square wide and two high, its feet at the origin.
  MeshData f;
  addQuad(f, Vec3(-0.4f, 0, 0), Vec3(0.4f, 0, 0), Vec3(0.4f, 1.6f, 0), Vec3(-0.4f, 1.6f, 0));
  figure_ = r3d_.createMesh(f);

  std::string err;
  std::string path = host_.args().assetsDir + "/models/figure.glb";
  if (!model_.load(r3d_, path, err)) OTE_WARN("3D test scene: no model (%s): %s", path.c_str(), err.c_str());
}

void Test3DScene::onInput(const InputEvent& e) {
  if (e.type == InputEvent::PointerDown) { ++turn_; return; }
  if (e.type != InputEvent::ButtonDown) return;
  switch (e.button) {
    case Button::Left: case Button::Prev: --turn_; break;
    case Button::Right: case Button::Next: ++turn_; break;
    case Button::Up: zoom_ = zoom_ < 2.0f ? zoom_ * 1.25f : zoom_; break;
    case Button::Down: zoom_ = zoom_ > 0.5f ? zoom_ / 1.25f : zoom_; break;
    case Button::Confirm: perspective_ = !perspective_; break;
    case Button::Cancel: case Button::Menu: host_.quit(); break;
    default: break;
  }
}

void Test3DScene::update(double dt) {
  time_ += dt;
  float d = (float)turn_ - angle_, step = (float)dt * 4.0f;  // a quarter turn in a quarter second
  angle_ = std::fabs(d) <= step ? (float)turn_ : angle_ + (d > 0 ? step : -step);
}

void Test3DScene::draw(Renderer& r) {
  Frame3D f;
  f.x = 0;
  f.y = 0;
  f.width = r.width();
  f.height = r.height();
  f.clear = Rgba3D(0.45f, 0.62f, 0.85f);
  // The camera looks at the middle of the map from a corner, 30 degrees
  // above the ground when the picture is flat (the classic isometric angle)
  // and a little higher with perspective.
  float yaw = (angle_ + 0.5f) * kPi * 0.5f, pitch = perspective_ ? 0.72f : 0.6155f, dist = perspective_ ? 11.0f / zoom_ : 30.0f;
  Vec3 eye(std::sin(yaw) * std::cos(pitch) * dist, std::sin(pitch) * dist, std::cos(yaw) * std::cos(pitch) * dist);
  f.view = Mat4::lookAt(eye, Vec3(0, 0, 0), Vec3(0, 1, 0));
  float aspect = (float)f.width / (float)f.height;
  if (perspective_) {
    f.proj = Mat4::perspective(0.8f, aspect, 0.5f, 100.0f);
    f.fog = true;
    f.fogColor = f.clear;
    f.fogStart = dist + 2;
    f.fogEnd = dist + 14;
  } else {
    float halfH = 4.2f / zoom_;
    f.proj = Mat4::ortho(halfH, aspect, 0.5f, 100.0f);
  }

  r3d_.begin(f);
  Material3D ground;
  ground.texture = groundTex_;
  r3d_.draw(ground_, Mat4(), ground);
  r3d_.draw(building_, Mat4(), Material3D());
  Material3D water;
  water.blend = Blend3D::Blend;
  water.lit = false;
  r3d_.draw(water_, Mat4(), water);

  // Standing pictures always face the camera (turned about the up axis only).
  Material3D fig;
  fig.texture = figureTex_;
  fig.blend = Blend3D::Cutout;
  fig.lit = false;
  fig.twoSided = true;
  const float half = kSize * 0.5f;
  const struct { int x, z; Rgba3D side; } units[] = {
      {1, 1, Rgba3D(0.3f, 0.5f, 1.0f)}, {2, 4, Rgba3D(0.3f, 0.5f, 1.0f)}, {0, 6, Rgba3D(0.3f, 0.5f, 1.0f)},
      {7, 7, Rgba3D(1.0f, 0.35f, 0.3f)}, {5, 6, Rgba3D(1.0f, 0.35f, 0.3f)}, {7, 3, Rgba3D(1.0f, 0.35f, 0.3f)}};
  for (const auto& u : units) {
    fig.color = u.side;
    float bob = (&u == &units[0]) ? 0.08f * (float)std::fabs(std::sin(time_ * 4.0)) : 0;
    Mat4 m = Mat4::translate(Vec3(u.x + 0.5f - half, topOf(u.x, u.z) + bob, u.z + 0.5f - half)) *
             Mat4::rotate(Quat::axisAngle(Vec3(0, 1, 0), yaw));
    r3d_.draw(figure_, m, fig);
  }
  // The loaded model stands on square (4, 5), turning slowly, scaled to a square's width.
  if (model_.loaded())
    model_.draw(r3d_, Mat4::translate(Vec3(4.5f - half, topOf(4, 5), 5.5f - half)) *
                          Mat4::rotate(Quat::axisAngle(Vec3(0, 1, 0), (float)time_)) * Mat4::scale(Vec3(0.9f, 0.9f, 0.9f)));
  r3d_.end();
  host_.show3d().show(r, r3d_, f.x, f.y, f.width, f.height);

  int lh = r.lineHeight();
  r.fillRect(0, 0, r.width(), lh + 4, Color(0, 0, 0, 140));
  r.drawText(4, 2, perspective_ ? "3D test scene - perspective" : "3D test scene - flat (isometric)", chrome::kTitle);
  drawFooter(r, "Left/Right turn   Up/Down zoom   Confirm perspective on/off   Cancel quit");
}

}  // namespace ote
