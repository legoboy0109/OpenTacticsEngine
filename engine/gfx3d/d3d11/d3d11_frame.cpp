// The Direct3D 11 backend: one frame. The picture it is drawn into, the queued draws, and where the finished picture
// goes (the host's screen as one quad, or back into memory). See d3d11_renderer3d.h.
#ifdef _WIN32
#include <algorithm>
#include <cstring>

#include "core/base/log.h"
#include "gfx3d/d3d11/d3d11_renderer3d.h"
#include "gfx3d/d3d11/d3d11_use.h"

namespace ote {

namespace {

// cbuffer Draw in shaders/render3d.hlsl, register by register.
struct DrawConstants {
  float mvp[16], mv[16], model[16];
  float color[4], toSun[4], sun[4], ambient[4], params[4], fog[4], fogColor[4];
};
static_assert(sizeof(DrawConstants) == 19 * 16, "DrawConstants must match cbuffer Draw");

void put4(float* d, float a, float b, float c, float e) {
  d[0] = a;
  d[1] = b;
  d[2] = c;
  d[3] = e;
}

}  // namespace

// ---- the picture ---------------------------------------------------------------

bool D3D11Renderer3D::target(int w, int h, int samples) {
  if (samples > maxSamples_) samples = maxSamples_;
  if (samples < 2) samples = 0;
  if (tgt_.color && tgt_.w == w && tgt_.h == h && tgt_.samples == samples) return true;
  tgt_ = Target();
  D3D11_TEXTURE2D_DESC d = {};
  d.Width = (UINT)w;
  d.Height = (UINT)h;
  d.MipLevels = d.ArraySize = 1;
  d.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  d.SampleDesc.Count = 1;
  d.Usage = D3D11_USAGE_DEFAULT;
  d.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
  if (FAILED(dev_->CreateTexture2D(&d, nullptr, tgt_.color.GetAddressOf())) ||
      FAILED(dev_->CreateShaderResourceView(tgt_.color.Get(), nullptr, tgt_.srv.GetAddressOf()))) {
    tgt_ = Target();
    return false;
  }
  if (samples) {
    // Multisampled colour and depth; the finished frame is resolved into `color` in finish().
    D3D11_TEXTURE2D_DESC m = d;
    m.SampleDesc.Count = (UINT)samples;
    m.BindFlags = D3D11_BIND_RENDER_TARGET;
    D3D11_TEXTURE2D_DESC z = m;
    z.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    z.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (FAILED(dev_->CreateTexture2D(&m, nullptr, tgt_.msColor.GetAddressOf())) ||
        FAILED(dev_->CreateTexture2D(&z, nullptr, tgt_.depth.GetAddressOf()))) {
      OTE_INFO("3D (Direct3D 11): %dx multisampling not available here; drawing without", samples);
      maxSamples_ = 0;  // silently carry on without, and do not try again
      tgt_.msColor.Reset();
      tgt_.depth.Reset();
      samples = 0;
    }
  }
  if (!samples) {
    D3D11_TEXTURE2D_DESC z = d;
    z.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    z.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (FAILED(dev_->CreateTexture2D(&z, nullptr, tgt_.depth.GetAddressOf()))) {
      tgt_ = Target();
      return false;
    }
  }
  if (FAILED(dev_->CreateRenderTargetView(samples ? tgt_.msColor.Get() : tgt_.color.Get(), nullptr, tgt_.rtv.GetAddressOf())) ||
      FAILED(dev_->CreateDepthStencilView(tgt_.depth.Get(), nullptr, tgt_.dsv.GetAddressOf()))) {
    tgt_ = Target();
    return false;
  }
  tgt_.w = w;
  tgt_.h = h;
  tgt_.samples = samples;
  return true;
}

// ---- frame ---------------------------------------------------------------------

void D3D11Renderer3D::begin(const Frame3D& frame) {
  frame_ = frame;
  frame_.sunDir = normalize(frame.sunDir);
  w_ = frame.renderWidth > 0 ? frame.renderWidth : std::max(0, frame.width);
  h_ = frame.renderHeight > 0 ? frame.renderHeight : std::max(0, frame.height);
  const int longer = std::max(w_, h_);
  if (longer > maxPicture_) {  // over the 4096 cap: same picture, smaller
    w_ = std::max(1, (int)((long long)w_ * maxPicture_ / longer));
    h_ = std::max(1, (int)((long long)h_ * maxPicture_ / longer));
  }
  queue_.clear();
  blended_.clear();
  for (Mesh& m : meshes_) m.queued = false;
  for (Texture& t : textures_) t.queued = false;
  cleared_ = false;
  inFrame_ = true;
}

void D3D11Renderer3D::draw(MeshId mesh, const Mat4& model, const Material3D& material) {
  if (!inFrame_ || mesh < 1 || (size_t)mesh > meshes_.size() || !meshes_[(size_t)mesh - 1].used) return;
  Cmd c{mesh, model, material, 0};
  if (material.blend == Blend3D::Blend) {
    // See-through things wait until everything solid is in the picture.
    c.depth = -(frame_.view * model).point(Vec3(0, 0, 0)).z;
    blended_.push_back(c);
    return;
  }
  queue_.push_back(c);
  meshes_[(size_t)mesh - 1].queued = true;
  if (material.texture >= 1 && (size_t)material.texture <= textures_.size()) textures_[(size_t)material.texture - 1].queued = true;
}

// Inside enter()/leave().
void D3D11Renderer3D::flush(bool withBlended) {
  if (!inFrame_ || w_ == 0 || h_ == 0 || broken_) {
    queue_.clear();
    return;
  }
  if (!target(w_, h_, frame_.samples)) {
    if (!broken_) OTE_ERROR("3D (Direct3D 11): no picture of %d x %d; the 3D picture is left out", w_, h_);
    broken_ = true;
    queue_.clear();
    return;
  }
  ID3D11DeviceContext* ctx = host_->context();
  ID3D11ShaderResourceView* noSrv = nullptr;
  ctx->PSSetShaderResources(0, 1, &noSrv);  // the picture may still be bound as last frame's screen quad input
  ctx->OMSetRenderTargets(1, tgt_.rtv.GetAddressOf(), tgt_.dsv.Get());
  D3D11_VIEWPORT vp = {0, 0, (float)w_, (float)h_, 0.0f, 1.0f};
  ctx->RSSetViewports(1, &vp);
  if (!cleared_) {
    const float c[4] = {frame_.clear.r, frame_.clear.g, frame_.clear.b, frame_.clear.a};
    ctx->ClearRenderTargetView(tgt_.rtv.Get(), c);
    ctx->ClearDepthStencilView(tgt_.dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    cleared_ = true;
  }
  ctx->IASetInputLayout(layout_.Get());
  ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  ctx->VSSetShader(vs_.Get(), nullptr, 0);
  ctx->PSSetShader(ps_.Get(), nullptr, 0);
  ctx->GSSetShader(nullptr, nullptr, 0);
  ctx->VSSetConstantBuffers(0, 1, drawCb_.GetAddressOf());
  ctx->PSSetConstantBuffers(0, 1, drawCb_.GetAddressOf());

  for (const Cmd& c : queue_) run(c);
  queue_.clear();
  for (Mesh& m : meshes_) m.queued = false;
  for (Texture& t : textures_) t.queued = false;
  if (withBlended) {
    std::stable_sort(blended_.begin(), blended_.end(), [](const Cmd& a, const Cmd& b) { return a.depth > b.depth; });
    for (const Cmd& c : blended_) run(c);
    blended_.clear();
  }
}

void D3D11Renderer3D::run(const Cmd& c) {
  if ((size_t)c.mesh > meshes_.size() || !meshes_[(size_t)c.mesh - 1].used) return;
  const Mesh& m = meshes_[(size_t)c.mesh - 1];
  if (m.indexCount <= 0) return;
  const Material3D& mat = c.material;
  const Texture* tex = nullptr;
  if (mat.texture >= 1 && (size_t)mat.texture <= textures_.size() && textures_[(size_t)mat.texture - 1].used)
    tex = &textures_[(size_t)mat.texture - 1];
  ID3D11DeviceContext* ctx = host_->context();

  D3D11_MAPPED_SUBRESOURCE map;
  if (FAILED(ctx->Map(drawCb_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) return;  // device gone
  DrawConstants* k = (DrawConstants*)map.pData;
  const Mat4 mv = frame_.view * c.model, mvp = frame_.proj * mv;
  std::memcpy(k->mvp, mvp.m, sizeof(k->mvp));
  std::memcpy(k->mv, mv.m, sizeof(k->mv));
  std::memcpy(k->model, c.model.m, sizeof(k->model));
  put4(k->color, mat.color.r, mat.color.g, mat.color.b, mat.color.a);
  const Vec3 toSun = frame_.sunDir * -1.0f;
  put4(k->toSun, toSun.x, toSun.y, toSun.z, 0);
  put4(k->sun, frame_.sun.r, frame_.sun.g, frame_.sun.b, 0);
  put4(k->ambient, frame_.ambient.r, frame_.ambient.g, frame_.ambient.b, 0);
  const bool blend = mat.blend == Blend3D::Blend;
  const float mode = mat.blend == Blend3D::Cutout ? 1.0f : blend ? 2.0f : 0.0f;
  put4(k->params, mode, mat.cutoff, mat.lit ? 1.0f : 0.0f, 0);
  const bool ranged = frame_.fog && frame_.fogEnd > frame_.fogStart;
  put4(k->fog, frame_.fogStart, ranged ? 1.0f / (frame_.fogEnd - frame_.fogStart) : 0.0f, ranged ? 1.0f : 0.0f, 0);
  put4(k->fogColor, frame_.fogColor.r, frame_.fogColor.g, frame_.fogColor.b, frame_.fog ? 1.0f : 0.0f);
  ctx->Unmap(drawCb_.Get(), 0);

  ctx->OMSetBlendState(blend ? alpha_.Get() : opaque_.Get(), nullptr, 0xffffffff);
  ctx->OMSetDepthStencilState(depthStates_[(mat.depthTest ? 0 : 2) + (blend ? 1 : 0)].Get(), 0);
  ctx->RSSetState(raster_[mat.twoSided ? 1 : 0].Get());
  ID3D11ShaderResourceView* srv = tex ? tex->srv.Get() : white_.Get();
  ctx->PSSetShaderResources(0, 1, &srv);
  ctx->PSSetSamplers(0, 1,
                     samplers_[(tex && tex->repeat ? 2 : 0) + (mat.smooth ? 1 : 0) + (mat.smooth && tex && tex->mipmaps ? 4 : 0)]
                         .GetAddressOf());
  const UINT stride = sizeof(Vertex3D), offset = 0;
  ctx->IASetVertexBuffers(0, 1, m.vb.GetAddressOf(), &stride, &offset);
  ctx->IASetIndexBuffer(m.ib.Get(), DXGI_FORMAT_R16_UINT, 0);
  ctx->DrawIndexed((UINT)m.indexCount, 0, 0);
}

void D3D11Renderer3D::end() {
  if (!inFrame_) return;
  onScreen_ = false;
  {
    D3D11Use use(*host_);
    if (use.ok) {
      flush(true);
      finish();
    } else {
      pixels_.assign((size_t)w_ * h_ * 4, (uint8_t)0);  // device gone: an empty picture, as the GL backend gives
    }
  }
  queue_.clear();
  blended_.clear();
  inFrame_ = false;
}

// Inside enter()/leave(), after the last flush.
void D3D11Renderer3D::finish() {
  if (broken_ || w_ == 0 || h_ == 0 || !tgt_.color) {
    pixels_.assign((size_t)w_ * h_ * 4, (uint8_t)0);
    return;
  }
  ID3D11DeviceContext* ctx = host_->context();
  if (tgt_.samples) ctx->ResolveSubresource(tgt_.color.Get(), 0, tgt_.msColor.Get(), 0, DXGI_FORMAT_R8G8B8A8_UNORM);
  D3D11Screen s;
  if (host_->screen(frame_, s) && s.target && s.targetWidth > 0 && s.targetHeight > 0) {
    toScreen(s);
    host_->screenDone();
    onScreen_ = true;
    pixels_.clear();
  } else {
    readBack();
  }
}

// The picture as one quad on the host's target, sampled the way the 2D layer would draw it (Show3D::show): smooth
// when the picture has more pixels than its logical rectangle, hard pixels otherwise; blended by its alpha (what the
// clear colour's alpha 0 leaves showing of the 2D drawing under it).
void D3D11Renderer3D::toScreen(const D3D11Screen& s) {
  ID3D11DeviceContext* ctx = host_->context();
  D3D11_MAPPED_SUBRESOURCE map;
  if (FAILED(ctx->Map(quadCb_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) return;
  float* r = (float*)map.pData;
  const float tw = (float)s.targetWidth, th = (float)s.targetHeight;
  put4(r, 2.0f * s.x / tw - 1.0f, 1.0f - 2.0f * s.y / th, 2.0f * (s.x + s.width) / tw - 1.0f,
       1.0f - 2.0f * (s.y + s.height) / th);
  ctx->Unmap(quadCb_.Get(), 0);

  ctx->OMSetRenderTargets(1, &s.target, nullptr);
  D3D11_VIEWPORT vp = {0, 0, tw, th, 0.0f, 1.0f};
  ctx->RSSetViewports(1, &vp);
  ctx->RSSetState(raster_[1].Get());  // no culling, no scissor
  ctx->OMSetBlendState(alpha_.Get(), nullptr, 0xffffffff);
  ctx->OMSetDepthStencilState(depthStates_[3].Get(), 0);  // (no depth buffer bound anyway)
  ctx->IASetInputLayout(nullptr);
  ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  ctx->VSSetShader(quadVs_.Get(), nullptr, 0);
  ctx->PSSetShader(quadPs_.Get(), nullptr, 0);
  ctx->VSSetConstantBuffers(0, 1, quadCb_.GetAddressOf());
  ctx->PSSetShaderResources(0, 1, tgt_.srv.GetAddressOf());
  const bool smooth = w_ > frame_.width || h_ > frame_.height;
  ctx->PSSetSamplers(0, 1, samplers_[smooth ? 1 : 0].GetAddressOf());
  ctx->Draw(4, 0);
  ID3D11ShaderResourceView* noSrv = nullptr;
  ctx->PSSetShaderResources(0, 1, &noSrv);
}

// Into memory, top row first (a Direct3D picture's first row is its top).
void D3D11Renderer3D::readBack() {
  pixels_.assign((size_t)w_ * h_ * 4, (uint8_t)0);
  if (!tgt_.color || tgt_.w != w_ || tgt_.h != h_) return;
  ID3D11DeviceContext* ctx = host_->context();
  if (!tgt_.staging) {
    D3D11_TEXTURE2D_DESC d = {};
    tgt_.color->GetDesc(&d);
    d.Usage = D3D11_USAGE_STAGING;
    d.BindFlags = 0;
    d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    if (FAILED(dev_->CreateTexture2D(&d, nullptr, tgt_.staging.GetAddressOf()))) return;
  }
  ctx->CopyResource(tgt_.staging.Get(), tgt_.color.Get());
  D3D11_MAPPED_SUBRESOURCE map;
  if (FAILED(ctx->Map(tgt_.staging.Get(), 0, D3D11_MAP_READ, 0, &map))) return;  // device gone: stays empty
  for (int y = 0; y < h_; ++y)
    std::memcpy(&pixels_[(size_t)y * w_ * 4], (const uint8_t*)map.pData + (size_t)y * map.RowPitch, (size_t)w_ * 4);
  ctx->Unmap(tgt_.staging.Get(), 0);
}

const uint8_t* D3D11Renderer3D::framePixels(int& width, int& height) const {
  if (onScreen_ || pixels_.empty()) {
    width = height = 0;
    return nullptr;
  }
  width = w_;
  height = h_;
  return pixels_.data();
}

}  // namespace ote

#endif  // _WIN32
