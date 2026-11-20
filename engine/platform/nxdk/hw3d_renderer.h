// Renderer3D on the original Xbox's NV2A (docs/RENDER3D.md "Original Xbox backend"). Draws straight into the screen's frame
// buffer under the 2D layer (nxdk_present.h); framePixels() is null.
#pragma once
#include <vector>

#include "gfx3d/render3d.h"
#include "platform/nxdk/hw3d_clip.h"

namespace ote {

class NxdkRenderer3D final : public Renderer3D {
 public:
  NxdkRenderer3D();
  ~NxdkRenderer3D() override;

  Caps3D caps() const override;
  MeshId createMesh(const MeshData& data) override;
  void updateMesh(MeshId mesh, const MeshData& data) override;
  void destroyMesh(MeshId mesh) override;
  Texture3DId createTexture(const uint8_t* rgba, int width, int height, bool repeat, bool mipmaps) override;
  void destroyTexture(Texture3DId texture) override;
  void begin(const Frame3D& frame) override;
  void draw(MeshId mesh, const Mat4& model, const Material3D& material) override;
  void end() override;

  // Memory the 3D layer holds (textures + meshes), for the log and the budgets.
  size_t textureBytes() const { return texBytes_; }
  size_t meshBytes() const { return meshBytes_; }

 private:
  // Vertices as Vertex3D lies in memory, in chip-readable memory. A mesh that is rewritten every frame (updateMesh) gets a second
  // buffer when the first may still be read by the chip, and the two take turns, so the CPU never waits for the chip
  // (hw3d::noteUse in hw3d_gpu.h says when the chip is done with one).
  struct Buf {
    void* mem = nullptr;
    uint32_t phys = 0;
    uint32_t lastUse = 0;  // hw3d::noteUse() number of the newest draw that reads it
  };
  struct Mesh {
    bool used = false;
    Buf buf[2];
    int cur = 0;          // the buffer the next draw reads
    int buffers = 1;      // how many are allocated
    size_t capacity = 0;  // vertices each buffer holds
    size_t count = 0;
    std::vector<uint16_t> indices;
    std::vector<Vertex3D> cpu;  // a copy for the CPU (clipping at the camera)
    float lo[3] = {0, 0, 0}, hi[3] = {0, 0, 0};
    // What the CPU cut at the camera last time (a mesh that straddles the clip volume, typically the ground): kept while the matrix,
    // the guard band and the vertices stay the same, which is every frame the camera does not move.
    struct Cut {
      bool valid = false;
      float clip[16];
      float guard = 0;
      std::vector<uint16_t> keep;
      std::vector<Vertex3D> loose;
    } cut;
  };
  struct Tex {
    bool used = false;
    void* mem = nullptr;
    uint32_t phys = 0;
    int w = 0, h = 0, levels = 1;
    bool repeat = true, mips = false;
    size_t bytes = 0;
  };
  struct Pending {
    MeshId mesh;
    Mat4 model;
    Material3D material;
    float depth;
  };

  bool fill(Mesh& m, const MeshData& data, bool allocate);
  void emit(Mesh& mesh, const Mat4& model, const Material3D& material);
  void clearRect();

  std::vector<Mesh> meshes_;
  std::vector<Tex> textures_;
  std::vector<Pending> blended_;
  Frame3D frame_;
  bool inFrame_ = false;
  int rx_ = 0, ry_ = 0, rw_ = 0, rh_ = 0;  // the 3D picture's rectangle on the screen
  size_t texBytes_ = 0, meshBytes_ = 0;
  size_t texBudget_ = 0, meshBudget_ = 0;
  void* whiteMem_ = nullptr;  // a 1 x 1 white texture for materials without one
  uint32_t whitePhys_ = 0;
  float guard_ = 2;           // how far past the screen's edge a vertex may be before the CPU cuts the triangle
  hw3d::ClipResult cutScratch_;  // (reused between draws)
  int drawCalls_ = 0;      // this frame
  int clipped_ = 0;
  int debugMode_ = 0;      // OTE_3D_DEBUG
  int frames_ = 0;         // 3D pictures drawn, for the log line every few seconds
};

}  // namespace ote
