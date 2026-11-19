// The Direct3D 11 backend: start, self-test and resources. The frame (picture, draws, screen) is d3d11_frame.cpp.
#ifdef _WIN32
#include "gfx3d/d3d11/d3d11_renderer3d.h"

#include <dxgi.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "core/base/log.h"
#include "gfx3d/d3d11/d3d11_device.h"
#include "gfx3d/d3d11/d3d11_use.h"
#include "gfx3d/d3d11/shaders/quad_ps.h"
#include "gfx3d/d3d11/shaders/quad_vs.h"
#include "gfx3d/d3d11/shaders/render3d_ps.h"
#include "gfx3d/d3d11/shaders/render3d_vs.h"

namespace ote {

namespace {

// The buffers take Vertex3D as it is (the input layout below).
static_assert(sizeof(Vertex3D) == 36, "Vertex3D must stay 8 floats + 4 bytes (the Direct3D 11 vertex layout)");

bool powerOfTwo(int v) { return v > 0 && (v & (v - 1)) == 0; }

const char* levelName(D3D_FEATURE_LEVEL l) {
  switch (l) {
    case D3D_FEATURE_LEVEL_9_1: return "9_1";
    case D3D_FEATURE_LEVEL_9_2: return "9_2";
    case D3D_FEATURE_LEVEL_9_3: return "9_3";
    case D3D_FEATURE_LEVEL_10_0: return "10_0";
    case D3D_FEATURE_LEVEL_10_1: return "10_1";
    case D3D_FEATURE_LEVEL_11_0: return "11_0";
    case D3D_FEATURE_LEVEL_11_1: return "11_1";
    default: return l > D3D_FEATURE_LEVEL_11_1 ? "12 or newer" : "?";
  }
}

// The graphics chip's name, as UTF-8 ("Microsoft Basic Render Driver" for WARP).
std::string adapterName(ID3D11Device* dev) {
  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi;
  Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
  DXGI_ADAPTER_DESC desc;
  if (FAILED(dev->QueryInterface(__uuidof(IDXGIDevice), (void**)dxgi.GetAddressOf())) ||
      FAILED(dxgi->GetAdapter(adapter.GetAddressOf())) || FAILED(adapter->GetDesc(&desc)))
    return "?";
  char utf8[256] = "";
  WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, utf8, (int)sizeof(utf8), nullptr, nullptr);
  return utf8;
}

}  // namespace

// ---- start and end -------------------------------------------------------------

std::unique_ptr<Renderer3D> D3D11Renderer3D::create(std::unique_ptr<D3D11Host> host, std::string& why) {
  if (!host) {
    why = "the platform gave no Direct3D 11 device";
    return nullptr;
  }
  std::unique_ptr<D3D11Renderer3D> r(new D3D11Renderer3D(std::move(host)));
  if (!r->init(why)) return nullptr;
  return std::unique_ptr<Renderer3D>(r.release());
}

bool D3D11Renderer3D::init(std::string& why) {
  D3D11Use use(*host_);
  if (!use.ok) {
    why = "the Direct3D 11 device is not usable";
    return false;
  }
  dev_ = host_->device();
  level_ = dev_->GetFeatureLevel();
  if (level_ < D3D_FEATURE_LEVEL_10_0) {
    why = std::string("the Direct3D 11 device has feature level ") + levelName(level_) + ", 10_0 is needed";
    return false;
  }
  auto failed = [&](HRESULT hr, const char* what) {
    if (SUCCEEDED(hr)) return false;
    char buf[160];
    snprintf(buf, sizeof(buf), "%s failed (HRESULT 0x%08lx)", what, (unsigned long)hr);
    why = buf;
    return true;
  };

  // Shaders, from bytecode compiled ahead of time (shaders/render3d.hlsl).
  if (failed(dev_->CreateVertexShader(kRender3dVs, sizeof(kRender3dVs), nullptr, vs_.GetAddressOf()), "a vertex shader") ||
      failed(dev_->CreatePixelShader(kRender3dPs, sizeof(kRender3dPs), nullptr, ps_.GetAddressOf()), "a pixel shader") ||
      failed(dev_->CreateVertexShader(kQuadVs, sizeof(kQuadVs), nullptr, quadVs_.GetAddressOf()), "the screen vertex shader") ||
      failed(dev_->CreatePixelShader(kQuadPs, sizeof(kQuadPs), nullptr, quadPs_.GetAddressOf()), "the screen pixel shader"))
    return false;
  const D3D11_INPUT_ELEMENT_DESC elements[4] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0},
  };
  if (failed(dev_->CreateInputLayout(elements, 4, kRender3dVs, sizeof(kRender3dVs), layout_.GetAddressOf()), "the vertex layout"))
    return false;

  // Constant buffers, filled anew for every draw (Map with DISCARD).
  D3D11_BUFFER_DESC cb = {};
  cb.Usage = D3D11_USAGE_DYNAMIC;
  cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  cb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  cb.ByteWidth = 19 * 16;  // cbuffer Draw: three matrices and seven float4
  if (failed(dev_->CreateBuffer(&cb, nullptr, drawCb_.GetAddressOf()), "a constant buffer")) return false;
  cb.ByteWidth = 16;  // cbuffer Quad
  if (failed(dev_->CreateBuffer(&cb, nullptr, quadCb_.GetAddressOf()), "a constant buffer")) return false;

  // One sampler per way of reading a texture: [mipmapped * 4 + repeat * 2 + smooth]. Hard pixels never use the
  // smaller copies, so those exist for smooth ones only; without them only the full-size picture is read (MaxLOD 0),
  // as the GL backend's samplers without a mipmap filter do.
  for (int i = 0; i < 8; ++i) {
    D3D11_SAMPLER_DESC s = {};
    const bool smooth = (i & 1) != 0, mip = (i & 4) != 0;
    s.Filter = mip ? D3D11_FILTER_MIN_MAG_MIP_LINEAR : smooth ? D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT : D3D11_FILTER_MIN_MAG_MIP_POINT;
    s.AddressU = s.AddressV = s.AddressW = (i & 2) ? D3D11_TEXTURE_ADDRESS_WRAP : D3D11_TEXTURE_ADDRESS_CLAMP;
    s.MaxAnisotropy = 1;
    s.ComparisonFunc = D3D11_COMPARISON_NEVER;
    s.MinLOD = 0;
    s.MaxLOD = mip ? D3D11_FLOAT32_MAX : 0.0f;
    if (failed(dev_->CreateSamplerState(&s, samplers_[i].GetAddressOf()), "a sampler")) return false;
  }
  // Front faces counter-clockwise as seen on the picture (Direct3D's first row is the top, so no flip is involved).
  for (int i = 0; i < 2; ++i) {
    D3D11_RASTERIZER_DESC r = {};
    r.FillMode = D3D11_FILL_SOLID;
    r.CullMode = i ? D3D11_CULL_NONE : D3D11_CULL_BACK;
    r.FrontCounterClockwise = TRUE;
    r.DepthClipEnable = TRUE;
    r.MultisampleEnable = TRUE;  // only matters on a multisampled target (and there only for lines at 10_1 and up)
    if (failed(dev_->CreateRasterizerState(&r, raster_[i].GetAddressOf()), "a rasterizer state")) return false;
  }
  // [no depth test * 2 + no depth writes]. Without the depth test a thing still leaves its depth behind (as in the
  // software backend), so the test stays on and always passes.
  for (int i = 0; i < 4; ++i) {
    D3D11_DEPTH_STENCIL_DESC d = {};
    d.DepthEnable = TRUE;
    d.DepthWriteMask = (i & 1) ? D3D11_DEPTH_WRITE_MASK_ZERO : D3D11_DEPTH_WRITE_MASK_ALL;
    d.DepthFunc = (i & 2) ? D3D11_COMPARISON_ALWAYS : D3D11_COMPARISON_LESS;
    if (failed(dev_->CreateDepthStencilState(&d, depthStates_[i].GetAddressOf()), "a depth state")) return false;
  }
  // Blended things: colour by its alpha; the picture's alpha is what the 2D layer later blends it by (as GL's
  // glBlendFuncSeparate(SRC_ALPHA, ONE_MINUS_SRC_ALPHA, ONE, ONE_MINUS_SRC_ALPHA), and SDL's own "blend" mode, which
  // the screen quad uses too).
  D3D11_BLEND_DESC b = {};
  b.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
  if (failed(dev_->CreateBlendState(&b, opaque_.GetAddressOf()), "a blend state")) return false;
  b.RenderTarget[0].BlendEnable = TRUE;
  b.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
  b.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
  b.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
  b.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
  b.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
  b.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
  if (failed(dev_->CreateBlendState(&b, alpha_.GetAddressOf()), "a blend state")) return false;

  // "No texture" is a white pixel, so the shader always samples.
  const uint8_t white[4] = {255, 255, 255, 255};
  D3D11_TEXTURE2D_DESC td = {};
  td.Width = td.Height = 1;
  td.MipLevels = td.ArraySize = 1;
  td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  td.SampleDesc.Count = 1;
  td.Usage = D3D11_USAGE_IMMUTABLE;
  td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  D3D11_SUBRESOURCE_DATA init = {white, 4, 4};
  Ptr<ID3D11Texture2D> wt;
  if (failed(dev_->CreateTexture2D(&td, &init, wt.GetAddressOf()), "a texture") ||
      failed(dev_->CreateShaderResourceView(wt.Get(), nullptr, white_.GetAddressOf()), "a texture view"))
    return false;

  // What this device can take. Every feature level from 10_0 has textures of 8192 a side and 4x multisampling of
  // these formats; asked anyway, and kept within the engine's own 4096.
  caps_.maxTextureSize = 4096;
  // "Fast enough for the display's resolution" (the Automatic choice) unless the device is Microsoft's software
  // rasteriser (a PC without a graphics driver, a VM): correct, but too slow for that.
  const bool softwareDevice = isSoftwareD3D11Device(dev_.Get());
  caps_.fullResolution = !softwareDevice;
  maxPicture_ = 4096;
  maxSamples_ = 0;
  for (int n = 4; n >= 2 && !maxSamples_; n /= 2) {
    UINT qc = 0, qd = 0;
    if (SUCCEEDED(dev_->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, n, &qc)) &&
        SUCCEEDED(dev_->CheckMultisampleQualityLevels(DXGI_FORMAT_D24_UNORM_S8_UINT, n, &qd)) && qc > 0 && qd > 0)
      maxSamples_ = n;
  }

  if (!selfTest(why)) return false;
  OTE_INFO("3D drawing: Direct3D 11 (feature level %s; %s%s; %s; up to %dx multisampling)", levelName(level_),
           adapterName(dev_.Get()).c_str(), softwareDevice ? ", a software device: the game's resolution by default" : "",
           host_->describe(), maxSamples_);
  return true;
}

// Proof that drawing into a picture and reading it back works here: clear a small one to a known colour and look at it.
bool D3D11Renderer3D::selfTest(std::string& why) {
  if (!target(16, 16, 0)) {
    why = "a 16 x 16 picture could not be made";
    return false;
  }
  const float c[4] = {0.2f, 0.4f, 0.6f, 1.0f};
  host_->context()->ClearRenderTargetView(tgt_.rtv.Get(), c);
  w_ = h_ = 16;
  readBack();
  const uint8_t* px = pixels_.data() + (3 * 16 + 3) * 4;
  bool right = pixels_.size() == 16 * 16 * 4 && px[0] >= 50 && px[0] <= 52 && px[1] >= 101 && px[1] <= 103 &&
               px[2] >= 152 && px[2] <= 154 && px[3] == 255;
  if (!right) {
    why = "a test picture came back wrong";
    if (pixels_.size() == 16 * 16 * 4)
      OTE_WARN("3D (Direct3D 11): self-test read %d,%d,%d,%d", px[0], px[1], px[2], px[3]);
    return false;
  }
  w_ = h_ = 0;
  pixels_.clear();
  return true;
}

D3D11Renderer3D::~D3D11Renderer3D() {
  // The COM objects release themselves (the device stays alive while they hold it, even after the platform let go).
}

// ---- resources -----------------------------------------------------------------

bool D3D11Renderer3D::validMesh(const MeshData& d) const {
  if (d.vertices.empty() || d.indices.empty() || d.indices.size() % 3 != 0) {
    OTE_ERROR("3D mesh refused: it needs vertices and whole triangles (%d vertices, %d indices)",
              (int)d.vertices.size(), (int)d.indices.size());
    return false;
  }
  if ((int)d.vertices.size() > caps().maxMeshVertices) {
    OTE_ERROR("3D mesh refused: %d vertices, the limit is %d per mesh", (int)d.vertices.size(), caps().maxMeshVertices);
    return false;
  }
  for (uint32_t i : d.indices)
    if (i >= d.vertices.size()) {
      OTE_ERROR("3D mesh refused: an index points past its %d vertices", (int)d.vertices.size());
      return false;
    }
  return true;
}

// Indices fit 16 bits (the vertex limit). Buffers that are big enough are refilled in place (the context is then
// needed: inside enter/leave); otherwise new ones are made.
bool D3D11Renderer3D::upload(Mesh& m, const MeshData& data) {
  indexScratch_.resize(data.indices.size());
  for (size_t i = 0; i < data.indices.size(); ++i) indexScratch_[i] = (uint16_t)data.indices[i];
  const UINT vbBytes = (UINT)(data.vertices.size() * sizeof(Vertex3D));
  const UINT ibBytes = (UINT)(indexScratch_.size() * sizeof(uint16_t));
  ID3D11DeviceContext* ctx = host_->context();
  if (m.vb && vbBytes <= m.vbBytes && ctx) {
    D3D11_BOX box = {0, 0, 0, vbBytes, 1, 1};
    ctx->UpdateSubresource(m.vb.Get(), 0, &box, data.vertices.data(), 0, 0);
  } else {
    D3D11_BUFFER_DESC d = {};
    d.ByteWidth = vbBytes;
    d.Usage = D3D11_USAGE_DEFAULT;
    d.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA s = {data.vertices.data(), 0, 0};
    m.vb.Reset();
    if (FAILED(dev_->CreateBuffer(&d, &s, m.vb.GetAddressOf()))) return false;
    m.vbBytes = vbBytes;
  }
  if (m.ib && ibBytes <= m.ibBytes && ctx) {
    D3D11_BOX box = {0, 0, 0, ibBytes, 1, 1};
    ctx->UpdateSubresource(m.ib.Get(), 0, &box, indexScratch_.data(), 0, 0);
  } else {
    D3D11_BUFFER_DESC d = {};
    d.ByteWidth = ibBytes;
    d.Usage = D3D11_USAGE_DEFAULT;
    d.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA s = {indexScratch_.data(), 0, 0};
    m.ib.Reset();
    if (FAILED(dev_->CreateBuffer(&d, &s, m.ib.GetAddressOf()))) return false;
    m.ibBytes = ibBytes;
  }
  m.indexCount = (int)data.indices.size();
  return true;
}

MeshId D3D11Renderer3D::createMesh(const MeshData& data) {
  if (!validMesh(data)) return 0;
  D3D11Use use(*host_);
  if (!use.ok) return 0;
  size_t slot = 0;
  while (slot < meshes_.size() && meshes_[slot].used) ++slot;
  if (slot == meshes_.size()) meshes_.push_back(Mesh());
  Mesh& m = meshes_[slot];
  m = Mesh();
  if (!upload(m, data)) {
    OTE_ERROR("3D (Direct3D 11): a mesh of %d vertices could not be made", (int)data.vertices.size());
    m = Mesh();
    return 0;
  }
  m.used = true;
  return (MeshId)(slot + 1);
}

void D3D11Renderer3D::updateMesh(MeshId mesh, const MeshData& data) {
  if (mesh < 1 || (size_t)mesh > meshes_.size() || !meshes_[(size_t)mesh - 1].used) return;
  if (!validMesh(data)) return;
  D3D11Use use(*host_);
  if (!use.ok) return;
  Mesh& m = meshes_[(size_t)mesh - 1];
  // A solid draw of the old shape is still waiting: carry it out first.
  if (m.queued) flush(false);
  if (!upload(m, data)) m.indexCount = 0;  // (device gone: the keeper rebuilds from the game's copy)
}

void D3D11Renderer3D::destroyMesh(MeshId mesh) {
  if (mesh < 1 || (size_t)mesh > meshes_.size() || !meshes_[(size_t)mesh - 1].used) return;
  Mesh& m = meshes_[(size_t)mesh - 1];
  if (m.queued) {
    D3D11Use use(*host_);
    if (use.ok) flush(false);
  }
  m = Mesh();
}

Texture3DId D3D11Renderer3D::createTexture(const uint8_t* rgba, int width, int height, bool repeat, bool mipmaps) {
  int limit = caps().maxTextureSize;
  if (!rgba || !powerOfTwo(width) || !powerOfTwo(height) || width > limit || height > limit) {
    OTE_ERROR("3D texture refused: %d x %d; each side must be a power of two up to %d", width, height, limit);
    return 0;
  }
  D3D11Use use(*host_);
  if (!use.ok) return 0;
  D3D11_TEXTURE2D_DESC td = {};
  td.Width = (UINT)width;
  td.Height = (UINT)height;
  td.ArraySize = 1;
  td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  td.SampleDesc.Count = 1;
  Ptr<ID3D11Texture2D> tex;
  Ptr<ID3D11ShaderResourceView> srv;
  HRESULT hr;
  if (!mipmaps) {
    td.MipLevels = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA s = {rgba, (UINT)width * 4, 0};
    hr = dev_->CreateTexture2D(&td, &s, tex.GetAddressOf());
    if (SUCCEEDED(hr)) hr = dev_->CreateShaderResourceView(tex.Get(), nullptr, srv.GetAddressOf());
  } else {
    // The smaller copies are made by the GPU (a box filter, like glGenerateMipmap): the texture must be a render target.
    td.MipLevels = 0;  // all of them
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    td.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
    hr = dev_->CreateTexture2D(&td, nullptr, tex.GetAddressOf());
    if (SUCCEEDED(hr)) hr = dev_->CreateShaderResourceView(tex.Get(), nullptr, srv.GetAddressOf());
    if (SUCCEEDED(hr)) {
      host_->context()->UpdateSubresource(tex.Get(), 0, nullptr, rgba, (UINT)width * 4, 0);
      host_->context()->GenerateMips(srv.Get());
    }
  }
  if (FAILED(hr)) {
    OTE_ERROR("3D (Direct3D 11): a %d x %d texture could not be made (HRESULT 0x%08lx)", width, height, (unsigned long)hr);
    return 0;
  }
  size_t slot = 0;
  while (slot < textures_.size() && textures_[slot].used) ++slot;
  if (slot == textures_.size()) textures_.push_back(Texture());
  Texture& t = textures_[slot];
  t = Texture();
  t.used = true;
  t.repeat = repeat;
  t.mipmaps = mipmaps;
  t.srv = srv;
  return (Texture3DId)(slot + 1);
}

void D3D11Renderer3D::destroyTexture(Texture3DId texture) {
  if (texture < 1 || (size_t)texture > textures_.size() || !textures_[(size_t)texture - 1].used) return;
  Texture& t = textures_[(size_t)texture - 1];
  if (t.queued) {
    D3D11Use use(*host_);
    if (use.ok) flush(false);
  }
  t = Texture();
}

}  // namespace ote

#endif  // _WIN32
