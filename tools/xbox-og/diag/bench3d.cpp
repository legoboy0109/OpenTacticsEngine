#include "bench3d.h"

#include <vector>

#include "gfx3d/shapes.h"
#include "gfx3d/soft3d.h"

namespace ote {

void bench3dRun(int width, int height, int frames, int& triangles, unsigned& checksum) {
  SoftRenderer3D r;

  // 64x64 checker texture (two greens), repeating.
  std::vector<uint8_t> px(64 * 64 * 4);
  for (int y = 0; y < 64; ++y)
    for (int x = 0; x < 64; ++x) {
      const bool a = ((x >> 3) + (y >> 3)) & 1;
      uint8_t* p = &px[(size_t)(y * 64 + x) * 4];
      p[0] = a ? 70 : 90;
      p[1] = a ? 150 : 170;
      p[2] = a ? 60 : 80;
      p[3] = 255;
    }
  const Texture3DId tex = r.createTexture(px.data(), 64, 64, true);

  MeshData ground;
  for (int y = 0; y < 16; ++y)
    for (int x = 0; x < 24; ++x) {
      const float h = 0.3f + 0.25f * (float)((x * 7 + y * 3) % 4);
      addBlock(ground, Vec3((float)x, 0, (float)y), Vec3((float)x + 1, h, (float)y + 1), Tint3D(255, 255, 255),
               Tint3D(170, 150, 120));
    }
  const MeshId groundId = r.createMesh(ground);

  MeshData unit;
  addBlock(unit, Vec3(0.25f, 0, 0.25f), Vec3(0.75f, 0.6f, 0.75f), Tint3D(220, 60, 60), Tint3D(200, 50, 50));
  addBlock(unit, Vec3(0.3f, 0.6f, 0.3f), Vec3(0.7f, 1.0f, 0.7f), Tint3D(240, 200, 160), Tint3D(230, 190, 150));
  const MeshId unitId = r.createMesh(unit);

  Material3D groundMat;
  groundMat.texture = tex;
  Material3D unitMat;
  unitMat.lit = true;

  Frame3D f;
  f.width = width;
  f.height = height;
  f.renderWidth = width;
  f.renderHeight = height;
  f.clear = Rgba3D(0.1f, 0.15f, 0.3f, 1);
  f.view = Mat4::lookAt(Vec3(12, 14, 26), Vec3(12, 0, 8), Vec3(0, 1, 0));
  f.proj = Mat4::perspective(0.7f, (float)width / (float)height, 0.5f, 80.f);
  f.fog = true;
  f.fogColor = Rgba3D(0.1f, 0.15f, 0.3f, 1);
  f.fogStart = 20;
  f.fogEnd = 45;

  checksum = 0;
  for (int i = 0; i < frames; ++i) {
    r.begin(f);
    r.draw(groundId, Mat4::translate(Vec3(0, 0, 0)), groundMat);
    for (int u = 0; u < 16; ++u)
      r.draw(unitId, Mat4::translate(Vec3((float)(3 + (u * 5) % 18), 0.5f, (float)(2 + (u * 3) % 12))), unitMat);
    r.end();
    int w = 0, h = 0;
    const uint8_t* p = r.framePixels(w, h);
    if (p) checksum += p[(size_t)(w * h / 2) * 4] + p[(size_t)(w * h / 3) * 4 + 1];
  }
  triangles = r.trianglesDrawn();
}

}  // namespace ote
