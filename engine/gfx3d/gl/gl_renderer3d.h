// The hardware backend of the 3D drawing layer: OpenGL ES 3.0, WebGL 2 or
// desktop OpenGL 3.3 core, whichever the platform's GlHost gives it
// (docs/RENDER3D.md "OpenGL backend").
//
// It draws into a picture of its own (a framebuffer object in a GL context
// nothing else uses) and hands the finished picture back through
// framePixels(), exactly like the software backend. So the 2D renderer needs
// no changes and need not be GL at all, and the two backends are swapped
// freely. The price is one read-back of the picture per frame, small at the
// game's logical resolution.
//
// Draw calls are collected and carried out in end(), inside one
// enter()/leave() of the host: between begin() and end() the 2D renderer may
// be used freely.
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "gfx3d/gl/gl_api.h"
#include "gfx3d/render3d.h"

namespace ote {

class GlRenderer3D : public Renderer3D {
 public:
  // Null (with a plain reason) when the context cannot do what is needed;
  // the caller then keeps the software backend. Takes the host over either way.
  static std::unique_ptr<Renderer3D> create(std::unique_ptr<GlHost> host, std::string& why);
  ~GlRenderer3D() override;

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
    return pixels_.empty() ? nullptr : pixels_.data();
  }

 private:
  struct Mesh {
    bool used = false;
    bool dynamic = false;  // updated at least once: its buffers are marked as changing
    bool queued = false;   // a solid draw of this frame still waits in queue_
    GLuint_ vao = 0, vbo = 0, ibo = 0;
    int indexCount = 0;
  };
  struct Texture {
    bool used = false, repeat = false, queued = false;
    GLuint_ id = 0;
  };
  struct Cmd {
    MeshId mesh;
    Mat4 model;
    Material3D material;
    float depth;  // view-space distance of the mesh's origin (blended things)
  };
  struct Uniforms {
    GLint_ mvp = -1, mv = -1, normalM = -1, color = -1, toSun = -1, sun = -1, ambient = -1, lit = -1, fog = -1,
           fogColor = -1, alpha = -1, tex = -1;
  };

  explicit GlRenderer3D(std::unique_ptr<GlHost> host) : host_(std::move(host)) {}
  bool init(std::string& why);
  bool validMesh(const MeshData& data) const;
  void upload(Mesh& m, const MeshData& data);
  bool target(int w, int h);           // makes the framebuffer that size
  void flush(bool withBlended);        // carries out the queued draws (context must be current)
  void run(const Cmd& c);
  void freeAll();

  std::unique_ptr<GlHost> host_;
  GlApi gl_;
  GLuint_ program_ = 0, white_ = 0, fbo_ = 0, colorTex_ = 0, depthRb_ = 0;
  GLuint_ samplers_[4] = {0, 0, 0, 0};  // [repeat * 2 + smooth]
  Uniforms u_;
  int fbW_ = 0, fbH_ = 0;
  std::vector<Mesh> meshes_;
  std::vector<Texture> textures_;
  std::vector<Cmd> queue_, blended_;
  std::vector<uint16_t> indexScratch_;
  Frame3D frame_;
  bool inFrame_ = false, cleared_ = false, broken_ = false;
  int w_ = 0, h_ = 0;
  std::vector<uint8_t> pixels_;
};

}  // namespace ote
