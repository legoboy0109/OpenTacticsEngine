// Saves and restores the parts of a Direct3D 11 immediate context's pipeline state the 3D backend changes, so a 2D
// renderer sharing the device (SDL's) finds everything as it left it. Windows only.
//
// SDL caches some of this (render target, viewport, rasterizer, blend, shaders, shader resources, samplers) and sets
// other parts once when it makes its device (input layout, vertex shader, its vertex constant buffer). SDL3's
// SDL_FlushRenderer also forgets the cached parts, SDL2's SDL_RenderFlush does not, so the only safe rule for every
// version is "put everything back exactly": that is this.
#pragma once
#ifdef _WIN32
#include <d3d11.h>

namespace ote {

class D3D11StateBackup {
 public:
  ~D3D11StateBackup() { clear(); }
  void save(ID3D11DeviceContext* c);
  void restore(ID3D11DeviceContext* c);  // also releases what save() held
  // What save() found, valid until restore(): the render target and the first viewport the other user of the context
  // (SDL) was drawing with. The SDL2 glue takes the place for the 3D picture from them (SDL2 shows no swap chain).
  ID3D11RenderTargetView* savedTarget() const { return saved_ ? rtv_ : nullptr; }
  bool savedViewport(D3D11_VIEWPORT& out) const {
    if (!saved_ || viewportCount_ == 0) return false;
    out = viewports_[0];
    return true;
  }

 private:
  void clear();
  bool saved_ = false;
  // Input assembler
  ID3D11InputLayout* layout_ = nullptr;
  D3D11_PRIMITIVE_TOPOLOGY topology_ = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
  ID3D11Buffer* vb_ = nullptr;
  UINT vbStride_ = 0, vbOffset_ = 0;
  ID3D11Buffer* ib_ = nullptr;
  DXGI_FORMAT ibFormat_ = DXGI_FORMAT_UNKNOWN;
  UINT ibOffset_ = 0;
  // Shaders (no class instances: neither SDL nor the backend uses them)
  ID3D11VertexShader* vs_ = nullptr;
  ID3D11PixelShader* ps_ = nullptr;
  ID3D11GeometryShader* gs_ = nullptr;
  ID3D11Buffer* vsCb_ = nullptr;
  ID3D11Buffer* psCb_ = nullptr;
  ID3D11ShaderResourceView* psSrv_ = nullptr;
  ID3D11SamplerState* psSampler_ = nullptr;
  // Rasterizer
  ID3D11RasterizerState* raster_ = nullptr;
  D3D11_VIEWPORT viewports_[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
  UINT viewportCount_ = 0;
  D3D11_RECT scissors_[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
  UINT scissorCount_ = 0;
  // Output merger
  ID3D11BlendState* blend_ = nullptr;
  FLOAT blendFactor_[4] = {0, 0, 0, 0};
  UINT sampleMask_ = 0xffffffff;
  ID3D11DepthStencilState* depth_ = nullptr;
  UINT stencilRef_ = 0;
  ID3D11RenderTargetView* rtv_ = nullptr;
  ID3D11DepthStencilView* dsv_ = nullptr;
};

}  // namespace ote

#endif  // _WIN32
