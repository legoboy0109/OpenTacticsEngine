// The software backend of the 3D drawing layer: draws on the CPU into a
// picture in memory. It needs nothing from the platform, so it runs wherever
// the engine runs (also in windowless test runs), and it is the reference
// the other backends are compared with. Slow next to a graphics chip: meant
// for small pictures (the game's logical resolution).
#pragma once
#include <vector>

#include "gfx3d/render3d.h"

namespace ote {

class SoftRenderer3D : public Renderer3D {
 public:
  // The picture is the frame's size in logical pixels times `scale`.
  explicit SoftRenderer3D(int scale = 1) : scale_(scale < 1 ? 1 : scale) {}

  Caps3D caps() const override { return Caps3D(); }
  MeshId createMesh(const MeshData& data) override;
  void updateMesh(MeshId mesh, const MeshData& data) override;
  void destroyMesh(MeshId mesh) override;
  Texture3DId createTexture(const uint8_t* rgba, int width, int height, bool repeat) override;
  void destroyTexture(Texture3DId texture) override;
  void begin(const Frame3D& frame) override;
  void draw(MeshId mesh, const Mat4& model, const Material3D& material) override;
  void end() override;
  const uint8_t* framePixels(int& width, int& height) const override {
    width = w_;
    height = h_;
    return color_.empty() ? nullptr : color_.data();
  }

  // For tests and statistics: triangles that reached the picture this frame.
  int trianglesDrawn() const { return triangles_; }

 private:
  struct Texture {
    int w = 0, h = 0;
    bool repeat = false, used = false;
    std::vector<uint8_t> rgba;
  };
  struct Mesh {
    bool used = false;
    MeshData data;
  };
  struct Pending {
    MeshId mesh;
    Mat4 model;
    Material3D material;
    float depth;  // view-space distance of the mesh's origin
  };
  struct V;  // a vertex on its way to the screen (soft3d.cpp)

  bool validMesh(const MeshData& data) const;
  void drawNow(const Mesh& mesh, const Mat4& model, const Material3D& material);
  void triangle(const V& a, const V& b, const V& c, const Material3D& material, const Texture* tex);

  int scale_ = 1;
  int w_ = 0, h_ = 0;
  std::vector<uint8_t> color_;  // w_ * h_ * 4
  std::vector<float> depth_;    // w_ * h_, -1 (near) .. 1 (far)
  std::vector<Mesh> meshes_;
  std::vector<Texture> textures_;
  std::vector<Pending> blended_;
  Frame3D frame_;
  bool inFrame_ = false;
  int triangles_ = 0;
};

}  // namespace ote
