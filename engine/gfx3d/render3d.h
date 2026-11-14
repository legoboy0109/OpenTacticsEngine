// The 3D drawing layer: the ONLY interface the engine uses to draw 3D. Every
// platform implements it once (docs/RENDER3D.md): a software backend that
// works everywhere today (soft3d.h), OpenGL ES 3 / WebGL 2 next, console SDKs
// later. Nothing outside a backend may name a graphics API.
//
// Deliberately the feature set of fixed-function hardware (the weakest
// targets are homebrew consoles): triangles with one texture, a vertex
// colour and a material colour; lighting worked out per vertex from one sun
// and an ambient colour; depth test; alpha cut-out or blending; linear fog.
// No shaders anywhere, also not in game data. Moving characters are meshes
// the engine re-poses on the CPU and uploads again (updateMesh).
#pragma once
#include <cstdint>
#include <vector>

#include "gfx3d/math3d.h"

namespace ote {

typedef int MeshId;      // 0 = none
typedef int Texture3DId; // 0 = none (plain white)

struct Vertex3D {
  float x = 0, y = 0, z = 0;
  float nx = 0, ny = 1, nz = 0;        // normal (for lighting)
  float u = 0, v = 0;                  // texture position; (0,0) = top-left of the picture
  uint8_t r = 255, g = 255, b = 255, a = 255;
};

// Triangles: three indices each, front faces counter-clockwise.
struct MeshData {
  std::vector<Vertex3D> vertices;
  std::vector<uint32_t> indices;
};

struct Rgba3D {
  float r = 1, g = 1, b = 1, a = 1;
  Rgba3D() = default;
  Rgba3D(float r_, float g_, float b_, float a_ = 1) : r(r_), g(g_), b(b_), a(a_) {}
};

enum class Blend3D {
  Opaque,  // alpha ignored
  Cutout,  // pixels whose alpha is under `cutoff` are left out (leaves, fences, sprites)
  Blend,   // see-through; drawn after everything else, far to near, without writing depth
};

struct Material3D {
  Texture3DId texture = 0;
  Rgba3D color;            // multiplies the texture and the vertex colour
  Blend3D blend = Blend3D::Opaque;
  float cutoff = 0.5f;
  bool lit = true;         // false: colours as they are (sprites, effects)
  bool twoSided = false;   // also draw back faces
  bool smooth = false;     // blend neighbouring texels instead of hard pixels
};

struct Frame3D {
  int x = 0, y = 0, width = 0, height = 0;  // where on the 2D screen, in logical pixels
  Rgba3D clear = Rgba3D(0, 0, 0, 1);        // alpha 0: leave what the 2D layer drew showing
  Mat4 view, proj;
  Vec3 sunDir = Vec3(-0.4f, -1.0f, -0.3f);  // the way the light travels
  Rgba3D sun = Rgba3D(0.75f, 0.75f, 0.72f);
  Rgba3D ambient = Rgba3D(0.45f, 0.45f, 0.5f);
  bool fog = false;                         // linear, by distance from the camera
  Rgba3D fogColor;
  float fogStart = 0, fogEnd = 0;
};

// What a backend can take. Loaders shrink or split content to fit.
struct Caps3D {
  int maxTextureSize = 1024;     // each side; sides are powers of two
  int maxMeshVertices = 65535;   // per mesh (16-bit indices on old hardware)
};

class Renderer3D {
 public:
  virtual ~Renderer3D() {}
  virtual Caps3D caps() const = 0;

  // 0 on failure (logged): empty or over the limits.
  virtual MeshId createMesh(const MeshData& data) = 0;
  // Replaces a mesh's vertices and indices (CPU-posed characters). Not between
  // drawing it and end() of the same frame.
  virtual void updateMesh(MeshId mesh, const MeshData& data) = 0;
  virtual void destroyMesh(MeshId mesh) = 0;

  // 32-bit pixels (R,G,B,A bytes, row by row, top row first, not
  // premultiplied). `repeat`: tile outside 0..1, else clamp to the edge.
  virtual Texture3DId createTexture(const uint8_t* rgba, int width, int height, bool repeat) = 0;
  virtual void destroyTexture(Texture3DId texture) = 0;

  // One 3D picture per begin/end. It lies under whatever the 2D renderer
  // draws afterwards.
  virtual void begin(const Frame3D& frame) = 0;
  virtual void draw(MeshId mesh, const Mat4& model, const Material3D& material) = 0;
  virtual void end() = 0;

  // Backends that draw into memory give the finished picture here (R,G,B,A
  // bytes) for the host to show at the frame's rectangle; backends that draw
  // straight to the screen return null.
  virtual const uint8_t* framePixels(int& width, int& height) const {
    width = height = 0;
    return nullptr;
  }
};

}  // namespace ote
