// The Direct3D 11 backend of the 3D drawing layer (docs/RENDER3D.md "Direct3D 11 backend"). Windows desktop (SDL3)
// and, unchanged, UWP / Xbox (SDL2): it only needs a device from the platform's D3D11Host (d3d11_host.h).
//
// Feature level 10_0 is enough (Xbox One UWP apps get 10_1): Shader Model 4.0 shaders compiled ahead of time
// (shaders/render3d.hlsl), no compute, no tessellation, nothing of 11_0.
//
// It draws into a picture of its own (colour, with 4x multisampling when the frame asks for it, and depth) at the
// frame's renderWidth x renderHeight. Then either the host gives a place on its screen and the picture is drawn there as
// one textured quad, entirely on the GPU, under whatever the 2D renderer draws afterwards (framePixels() is null), or
// the picture is read back into memory for framePixels(), like the OpenGL backend (windowless runs and tests).
//
// Draw calls are collected and carried out in end(), inside one enter()/leave() of the host (which, when sharing SDL's
// device, lets SDL finish its queued 2D drawing first and puts SDL's pipeline state back after): between begin() and
// end() the 2D renderer may be used freely.
#pragma once
#ifdef _WIN32
#include <d3d11.h>
#include <wrl/client.h>

#include <memory>
#include <string>
#include <vector>

#include "gfx3d/d3d11/d3d11_host.h"
#include "gfx3d/render3d.h"

namespace ote {

class D3D11Renderer3D : public Renderer3D {
 public:
  // Null (with a plain reason) when the device cannot do what is needed (feature level under 10_0, a shader or a
  // self-test picture failing); the caller then tries the next backend. Takes the host over either way.
  static std::unique_ptr<Renderer3D> create(std::unique_ptr<D3D11Host> host, std::string& why);
  ~D3D11Renderer3D() override;

  Caps3D caps() const override { return caps_; }
  MeshId createMesh(const MeshData& data) override;
  void updateMesh(MeshId mesh, const MeshData& data) override;
  void destroyMesh(MeshId mesh) override;
  Texture3DId createTexture(const uint8_t* rgba, int width, int height, bool repeat, bool mipmaps = false) override;
  void destroyTexture(Texture3DId texture) override;
  void begin(const Frame3D& frame) override;
  void draw(MeshId mesh, const Mat4& model, const Material3D& material) override;
  void end() override;
  const uint8_t* framePixels(int& width, int& height) const override;

 private:
  template <class T>
  using Ptr = Microsoft::WRL::ComPtr<T>;
  struct Mesh {
    bool used = false;
    bool queued = false;  // a solid draw of this frame still waits in queue_
    Ptr<ID3D11Buffer> vb, ib;
    UINT vbBytes = 0, ibBytes = 0;  // what the buffers hold at most (updateMesh refills them in place when it fits)
    int indexCount = 0;
  };
  struct Texture {
    bool used = false, repeat = false, mipmaps = false, queued = false;
    Ptr<ID3D11ShaderResourceView> srv;
  };
  struct Cmd {
    MeshId mesh;
    Mat4 model;
    Material3D material;
    float depth;  // view-space distance of the mesh's origin (blended things)
  };
  // The picture: colour (+ multisampled colour) and depth at one size.
  struct Target {
    int w = 0, h = 0, samples = 0;
    Ptr<ID3D11Texture2D> color, msColor, depth, staging;
    Ptr<ID3D11RenderTargetView> rtv;  // msColor's when multisampled, else color's
    Ptr<ID3D11ShaderResourceView> srv;  // color's: what the screen quad samples
    Ptr<ID3D11DepthStencilView> dsv;
  };

  explicit D3D11Renderer3D(std::unique_ptr<D3D11Host> host) : host_(std::move(host)) {}
  bool init(std::string& why);
  bool selfTest(std::string& why);
  bool validMesh(const MeshData& data) const;
  bool upload(Mesh& m, const MeshData& data);
  bool target(int w, int h, int samples);  // makes the picture that size (samples: wanted, 0 = none)
  void flush(bool withBlended);             // carries out the queued draws (inside enter/leave)
  void run(const Cmd& c);
  void finish();                            // resolves the picture and shows it or reads it back (inside enter/leave)
  void toScreen(const D3D11Screen& s);
  void readBack();

  std::unique_ptr<D3D11Host> host_;
  Ptr<ID3D11Device> dev_;
  D3D_FEATURE_LEVEL level_ = D3D_FEATURE_LEVEL_10_0;
  Ptr<ID3D11VertexShader> vs_, quadVs_;
  Ptr<ID3D11PixelShader> ps_, quadPs_;
  Ptr<ID3D11InputLayout> layout_;
  Ptr<ID3D11Buffer> drawCb_, quadCb_;
  Ptr<ID3D11SamplerState> samplers_[8];       // [mipmapped smooth * 4 + repeat * 2 + smooth]
  Ptr<ID3D11RasterizerState> raster_[2];      // [two-sided]
  Ptr<ID3D11DepthStencilState> depthStates_[4];  // [no depth test * 2 + no depth writes]
  Ptr<ID3D11BlendState> opaque_, alpha_;
  Ptr<ID3D11ShaderResourceView> white_;
  Caps3D caps_;
  int maxPicture_ = 4096, maxSamples_ = 0;
  Target tgt_;
  std::vector<Mesh> meshes_;
  std::vector<Texture> textures_;
  std::vector<Cmd> queue_, blended_;
  std::vector<uint16_t> indexScratch_;
  Frame3D frame_;
  bool inFrame_ = false, cleared_ = false, broken_ = false, onScreen_ = false;
  int w_ = 0, h_ = 0;
  std::vector<uint8_t> pixels_;
};

}  // namespace ote

#endif  // _WIN32
