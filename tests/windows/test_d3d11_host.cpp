// The Direct3D 11 backend with a host that shares its device (as SDL's renderer does): the picture drawn onto the
// host's target as one quad, blended over what is there, at the right place and scale; the host's pipeline state put
// back exactly; and a lost device rebuilt (or replaced by the software backend) through ResilientRenderer3D. Windows
// only. The same paths with SDL's real renderer: tests/render3d_d3d11_test.py.
#include <d3d11.h>
#include <wrl/client.h>

#include "d3d11_test_util.h"
#include "gfx3d/d3d11/d3d11_state.h"
#include "gfx3d/resilient3d.h"
#include "gfx3d/soft3d.h"
#include "test.h"

using namespace d3d11test;
using Microsoft::WRL::ComPtr;

namespace {

// A stand-in for SDL: a "screen" texture with a backdrop on it, state saved and restored around the backend's use.
class ScreenHost : public OwnD3D11Host {
 public:
  ScreenHost(ComPtr<ID3D11Device> dev, int w, int h, float x, float y, float scale)
      : OwnD3D11Host(dev, std::make_shared<D3D11LossFlag>()), dev_(dev), w_(w), h_(h), x_(x), y_(y), scale_(scale) {
    D3D11_TEXTURE2D_DESC d = {};
    d.Width = (UINT)w;
    d.Height = (UINT)h;
    d.MipLevels = d.ArraySize = 1;
    d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;  // what SDL's swap chain has
    d.SampleDesc.Count = 1;
    d.Usage = D3D11_USAGE_DEFAULT;
    d.BindFlags = D3D11_BIND_RENDER_TARGET;
    dev_->CreateTexture2D(&d, nullptr, screen_.GetAddressOf());
    dev_->CreateRenderTargetView(screen_.Get(), nullptr, rtv_.GetAddressOf());
  }
  bool enter() override {
    if (!OwnD3D11Host::enter()) return false;
    saved_.save(context());
    return true;
  }
  void leave() override { saved_.restore(context()); }
  bool screen(const Frame3D& f, D3D11Screen& out) override {
    out.target = rtv_.Get();
    out.targetWidth = w_;
    out.targetHeight = h_;
    out.x = x_ + f.x * scale_;
    out.y = y_ + f.y * scale_;
    out.width = f.width * scale_;
    out.height = f.height * scale_;
    return true;
  }
  void backdrop(float r, float g, float b) {
    const float c[4] = {r, g, b, 1};
    context()->ClearRenderTargetView(rtv_.Get(), c);
  }
  // The screen as RGBA, top row first.
  Picture read() {
    Picture p;
    D3D11_TEXTURE2D_DESC d;
    screen_->GetDesc(&d);
    d.Usage = D3D11_USAGE_STAGING;
    d.BindFlags = 0;
    d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    if (FAILED(dev_->CreateTexture2D(&d, nullptr, staging.GetAddressOf()))) return p;
    context()->CopyResource(staging.Get(), screen_.Get());
    D3D11_MAPPED_SUBRESOURCE map;
    if (FAILED(context()->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &map))) return p;
    p.w = w_;
    p.h = h_;
    p.px.resize((size_t)w_ * h_ * 4);
    for (int y = 0; y < h_; ++y)
      for (int x = 0; x < w_; ++x) {
        const uint8_t* s = (const uint8_t*)map.pData + (size_t)y * map.RowPitch + (size_t)x * 4;
        uint8_t* o = &p.px[((size_t)y * w_ + x) * 4];
        o[0] = s[2];
        o[1] = s[1];
        o[2] = s[0];
        o[3] = s[3];
      }
    context()->Unmap(staging.Get(), 0);
    return p;
  }

 private:
  ComPtr<ID3D11Device> dev_;
  ComPtr<ID3D11Texture2D> screen_;
  ComPtr<ID3D11RenderTargetView> rtv_;
  D3D11StateBackup saved_;
  int w_, h_;
  float x_, y_, scale_;
};

// A scene with see-through parts over a clear colour of alpha 0, so the backdrop shows through.
void scene(Renderer3D& r, Frame3D f) {
  MeshId glass = r.createMesh(quad(0.5f, 0, 255, 128, 0, 100));
  MeshId solid = r.createMesh(quad(0.25f, -0.5f, 0, 128, 255));
  Material3D see;
  see.blend = Blend3D::Blend;
  see.lit = false;
  f.clear = Rgba3D(0, 0, 0, 0);
  r.begin(f);
  r.draw(solid, Mat4::translate(Vec3(0.4f, 0.4f, 0)), Material3D());
  r.draw(glass, Mat4(), see);
  r.end();
}

// What the 2D layer does with a picture (SDL's "blend" mode): colour by its alpha over the backdrop.
uint8_t over(uint8_t pic, uint8_t alpha, uint8_t back) {
  float a = alpha / 255.0f;
  return (uint8_t)(pic * a + back * (1 - a) + 0.5f);
}

ComPtr<ID3D11Device> device(const char* level = "") {
  D3D11DeviceChoice c;
  c.maxLevel = level;
  std::string why;
  return makeD3D11Device(c, why);
}

}  // namespace

TEST(d3d11_puts_the_picture_on_the_hosts_screen) {
  ComPtr<ID3D11Device> dev = device();
  if (!dev) return;
  // The reference: the same backend reading the picture back.
  std::unique_ptr<Renderer3D> plain = makeD3D11("");
  scene(*plain, frame());
  Picture pic = grab(*plain);
  CHECK(pic.w == 64);

  // 1:1 at an offset (a letterbox bar to the left and above), and twice as big (a window twice the logical size:
  // the picture has the logical size, so it is scaled with hard pixels, as Show3D does).
  struct Place {
    int w, h;
    float x, y, scale;
  };
  const Place places[] = {{100, 80, 10, 6, 1}, {140, 140, 4, 2, 2}};
  for (const Place& pl : places) {
    ScreenHost* host = new ScreenHost(dev, pl.w, pl.h, pl.x, pl.y, pl.scale);
    std::string why;
    std::unique_ptr<Renderer3D> r = D3D11Renderer3D::create(std::unique_ptr<D3D11Host>(host), why);
    CHECK(r != nullptr);
    if (!r) return;
    const uint8_t back[3] = {26, 153, 77};
    host->backdrop(back[0] / 255.0f, back[1] / 255.0f, back[2] / 255.0f);
    Frame3D f = frame();
    f.x = 0;
    f.y = 0;
    scene(*r, f);
    int w = -1, h = -1;
    CHECK(r->framePixels(w, h) == nullptr);  // on the screen, nothing in memory
    Picture s = host->read();
    int worst = 0, wrong = 0;
    for (int y = 0; y < pl.h; ++y)
      for (int x = 0; x < pl.w; ++x) {
        int px = (int)((x - pl.x) / pl.scale), py = (int)((y - pl.y) / pl.scale);
        bool inside = x >= pl.x && y >= pl.y && px < 64 && py < 64;
        for (int c = 0; c < 3; ++c) {
          int want = inside ? over(pic.at(px, py)[c], pic.at(px, py)[3], back[c]) : back[c];
          int d = std::abs(want - (int)s.at(x, y)[c]);
          if (d > worst) worst = d;
          if (d > 2) ++wrong;
        }
      }
    std::printf("  d3d11 screen quad at scale %.0f: max %d, %d channel values more than 2 apart\n", pl.scale, worst, wrong);
    CHECK_EQ(wrong, 0);
  }
}

TEST(d3d11_puts_the_hosts_pipeline_state_back) {
  ComPtr<ID3D11Device> dev = device();
  if (!dev) return;
  ScreenHost* host = new ScreenHost(dev, 80, 80, 0, 0, 1);
  std::string why;
  std::unique_ptr<Renderer3D> r = D3D11Renderer3D::create(std::unique_ptr<D3D11Host>(host), why);
  if (!r) return;
  ComPtr<ID3D11DeviceContext> ctx;
  dev->GetImmediateContext(ctx.GetAddressOf());
  // Some state of "SDL's": a target, a viewport, a scissor rectangle, a blend and a rasterizer state.
  D3D11_TEXTURE2D_DESC d = {};
  d.Width = d.Height = 8;
  d.MipLevels = d.ArraySize = 1;
  d.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  d.SampleDesc.Count = 1;
  d.BindFlags = D3D11_BIND_RENDER_TARGET;
  ComPtr<ID3D11Texture2D> t;
  ComPtr<ID3D11RenderTargetView> rtv;
  dev->CreateTexture2D(&d, nullptr, t.GetAddressOf());
  dev->CreateRenderTargetView(t.Get(), nullptr, rtv.GetAddressOf());
  D3D11_BLEND_DESC bd = {};
  bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED;
  ComPtr<ID3D11BlendState> blend;
  dev->CreateBlendState(&bd, blend.GetAddressOf());
  D3D11_RASTERIZER_DESC rd = {};
  rd.FillMode = D3D11_FILL_SOLID;
  rd.CullMode = D3D11_CULL_NONE;
  rd.ScissorEnable = TRUE;
  ComPtr<ID3D11RasterizerState> raster;
  dev->CreateRasterizerState(&rd, raster.GetAddressOf());
  ctx->OMSetRenderTargets(1, rtv.GetAddressOf(), nullptr);
  D3D11_VIEWPORT vp = {1, 2, 33, 44, 0, 1};
  ctx->RSSetViewports(1, &vp);
  D3D11_RECT sc = {3, 4, 5, 6};
  ctx->RSSetScissorRects(1, &sc);
  const float factor[4] = {0.25f, 0.5f, 0.75f, 1};
  ctx->OMSetBlendState(blend.Get(), factor, 0x0f0f0f0f);
  ctx->RSSetState(raster.Get());
  ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

  scene(*r, frame());  // makes meshes (no context use), draws, puts the picture on the screen

  ComPtr<ID3D11RenderTargetView> rtv2;
  ComPtr<ID3D11DepthStencilView> dsv2;
  ctx->OMGetRenderTargets(1, rtv2.GetAddressOf(), dsv2.GetAddressOf());
  UINT n = 1;
  D3D11_VIEWPORT vp2 = {};
  ctx->RSGetViewports(&n, &vp2);
  UINT ns = 1;
  D3D11_RECT sc2 = {};
  ctx->RSGetScissorRects(&ns, &sc2);
  ComPtr<ID3D11BlendState> blend2;
  float factor2[4];
  UINT mask2 = 0;
  ctx->OMGetBlendState(blend2.GetAddressOf(), factor2, &mask2);
  ComPtr<ID3D11RasterizerState> raster2;
  ctx->RSGetState(raster2.GetAddressOf());
  D3D11_PRIMITIVE_TOPOLOGY topo;
  ctx->IAGetPrimitiveTopology(&topo);
  ComPtr<ID3D11VertexShader> vs2;
  ctx->VSGetShader(vs2.GetAddressOf(), nullptr, nullptr);
  ComPtr<ID3D11ShaderResourceView> srv2;
  ctx->PSGetShaderResources(0, 1, srv2.GetAddressOf());
  CHECK(rtv2.Get() == rtv.Get() && dsv2.Get() == nullptr);
  CHECK(n == 1 && vp2.TopLeftX == 1 && vp2.TopLeftY == 2 && vp2.Width == 33 && vp2.Height == 44);
  CHECK(ns == 1 && sc2.left == 3 && sc2.top == 4 && sc2.right == 5 && sc2.bottom == 6);
  CHECK(blend2.Get() == blend.Get() && factor2[1] == 0.5f && mask2 == 0x0f0f0f0fu);
  CHECK(raster2.Get() == raster.Get());
  CHECK(topo == D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
  CHECK(vs2.Get() == nullptr && srv2.Get() == nullptr);
}

namespace {

// The keeper the SDL3 glue has, on own devices: a simulated loss, or a device that is really gone.
class OwnKeeper : public DeviceKeeper {
 public:
  explicit OwnKeeper(std::shared_ptr<D3D11LossFlag> loss) : loss_(loss) {}
  const char* name() const override { return "3D (Direct3D 11 test)"; }
  const char* lossText() const override { return "the test device was lost"; }
  bool lost() override { return loss_->simulated; }
  std::unique_ptr<Renderer3D> rebuild(std::string& why) override {
    ++rebuilds;
    loss_ = std::make_shared<D3D11LossFlag>();
    ComPtr<ID3D11Device> dev = device();
    if (!dev) return nullptr;
    return D3D11Renderer3D::create(std::unique_ptr<D3D11Host>(new OwnD3D11Host(dev, loss_)), why);
  }
  void simulateLoss() override { loss_->simulated = true; }
  void release() override {}
  int rebuilds = 0;

 private:
  std::shared_ptr<D3D11LossFlag> loss_;
};

}  // namespace

TEST(d3d11_device_loss_rebuilds_or_falls_back_to_software) {
  for (bool refuse : {false, true}) {
    std::shared_ptr<D3D11LossFlag> loss = std::make_shared<D3D11LossFlag>();
    std::unique_ptr<Renderer3D> inner = makeD3D11("", loss);
    if (!inner) return;
    OwnKeeper* keeper = new OwnKeeper(loss);
    ResilientRenderer3D r(std::move(inner), std::unique_ptr<DeviceKeeper>(keeper), 2, refuse);
    std::vector<uint8_t> t(16 * 16 * 4, 200);
    Texture3DId tex = r.createTexture(t.data(), 16, 16, true, true);
    MeshId m = r.createMesh(quad(0.5f, 0, 255, 100, 50));
    Material3D mat;
    mat.texture = tex;
    mat.smooth = true;
    Picture first, second, third;
    for (int i = 0; i < 3; ++i) {
      r.begin(frame());
      r.draw(m, Mat4::rotate(Quat::axisAngle(Vec3(0, 0, 1), 0.2f)), mat);
      r.end();
      (i == 0 ? first : i == 1 ? second : third) = grab(r);
    }
    Diff a = compare(first, second), b = compare(first, third);
    std::printf("  d3d11 device loss (%s): frame of the loss max %d, after max %d, %d pixels > 8\n",
                refuse ? "no rebuild: software" : "rebuilt", a.max, b.max, b.over);
    CHECK_EQ(keeper->rebuilds, refuse ? 0 : 1);
    CHECK(r.usingSoftware() == refuse);
    // Rebuilt: the very same picture. Software: the software backend's, a few edge pixels apart.
    CHECK(a.sameSize && b.sameSize);
    CHECK(refuse ? (a.over <= 8 && b.over <= 8) : (a.max == 0 && b.max == 0));
  }
}
