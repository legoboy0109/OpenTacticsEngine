// Windows: gives the Direct3D 11 3D backend (engine/gfx3d/d3d11) its device (docs/RENDER3D.md "Direct3D 11 backend").
//
// When SDL draws the 2D layer with its own Direct3D 11 renderer (sdl_app.cpp asks for it on Windows), the backend uses
// SDL's device and draws the finished 3D picture straight onto SDL's back buffer, as one textured quad under the 2D
// things drawn afterwards: no read-back, no second device. Before touching the device it lets SDL carry out its queued
// drawing (SDL_FlushRenderer) and saves the pipeline state; after, it puts that state back (D3D11StateBackup), so SDL's
// cached state stays true. Otherwise (windowless runs: SDL's software renderer; or SDL picked another driver) the
// backend makes a device of its own and reads the picture back, like the OpenGL backend.
//
// Losing the device: a driver update or reset, a removed GPU, or SDL resetting its renderer
// (SDL_EVENT_RENDER_DEVICE_RESET: SDL then has a new device). The backend is wrapped in ResilientRenderer3D
// (gfx3d/resilient3d.h), which keeps a CPU copy of the meshes and textures; D3D11Keeper below tells it when the device
// is gone (removed, or no longer SDL's) and makes a new backend on SDL's new device (or a new own device), or the
// software backend takes over.
//
// Debug, from the environment: OTE_3D_API=opengl skips Direct3D 11; OTE_3D_D3D11_LEVEL=10_1 (or 10_0, 11_0) caps the
// feature level and OTE_3D_D3D11_DRIVER=warp picks Microsoft's software rasteriser, both for an own device only (SDL
// makes its own); OTE_3D_D3D11_OWN=1 uses an own device even when SDL's could be shared; OTE_3D_SIMULATE_LOSS=N and
// OTE_3D_SIMULATE_NO_REBUILD=1 as on Android (gfx3d/resilient3d.h).
#ifdef _WIN32
#include <SDL3/SDL.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <cstdlib>
#include <string>

#include "core/base/log.h"
#include "gfx3d/d3d11/d3d11_device.h"
#include "gfx3d/d3d11/d3d11_renderer3d.h"
#include "gfx3d/d3d11/d3d11_state.h"
#include "gfx3d/resilient3d.h"
#include "platform/sdl3/sdl_gl3d.h"

namespace ote {
namespace {

using Microsoft::WRL::ComPtr;

struct Shared {
  SDL_Renderer* ren = nullptr;  // the player's 2D renderer (attachHardware3D), null after shutdown
  std::shared_ptr<D3D11LossFlag> loss;  // of the backend made last
};
Shared g;

ID3D11Device* sdlDevice() {
  if (!g.ren || SDL_strcmp(SDL_GetRendererName(g.ren), "direct3d11") != 0) return nullptr;
  return (ID3D11Device*)SDL_GetPointerProperty(SDL_GetRendererProperties(g.ren), SDL_PROP_RENDERER_D3D11_DEVICE_POINTER,
                                               nullptr);
}

// SDL's device, shared with its 2D renderer.
class SdlD3D11Host : public D3D11Host {
 public:
  SdlD3D11Host(ID3D11Device* dev, std::shared_ptr<D3D11LossFlag> loss) : dev_(dev), loss_(loss) {
    dev_->GetImmediateContext(ctx_.GetAddressOf());
  }
  ID3D11Device* device() override { return g.ren ? dev_.Get() : nullptr; }
  ID3D11DeviceContext* context() override { return g.ren ? ctx_.Get() : nullptr; }
  bool usable() { return g.ren && !loss_->simulated && sdlDevice() == dev_.Get() && dev_->GetDeviceRemovedReason() == S_OK; }

  bool enter() override {
    if (depth_ > 0) {
      ++depth_;
      return true;
    }
    if (!usable()) return false;
    SDL_FlushRenderer(g.ren);  // the 2D drawing queued so far goes first (it lies under the 3D picture)
    saved_.save(ctx_.Get());
    depth_ = 1;
    return true;
  }
  void leave() override {
    if (depth_ == 0 || --depth_ > 0) return;
    saved_.restore(ctx_.Get());
  }

  // SDL's back buffer, and the frame's logical rectangle mapped through SDL's logical presentation (letterbox).
  bool screen(const Frame3D& f, D3D11Screen& out) override {
    if (SDL_GetRenderTarget(g.ren)) {
      // The player never draws 3D into a texture target; should that change, the picture goes through memory.
      if (!targetWarned_) OTE_WARN("3D (Direct3D 11): SDL has a texture as render target; the 3D picture is read back");
      targetWarned_ = true;
      return false;
    }
    IDXGISwapChain* chain = (IDXGISwapChain*)SDL_GetPointerProperty(
        SDL_GetRendererProperties(g.ren), SDL_PROP_RENDERER_D3D11_SWAPCHAIN_POINTER, nullptr);
    ComPtr<ID3D11Texture2D> back;
    if (!chain || FAILED(chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)back.GetAddressOf()))) return false;
    D3D11_TEXTURE2D_DESC d;
    back->GetDesc(&d);
    if (FAILED(dev_->CreateRenderTargetView(back.Get(), nullptr, rtv_.ReleaseAndGetAddressOf()))) return false;
    int lw = 0, lh = 0;
    SDL_RendererLogicalPresentation mode = SDL_LOGICAL_PRESENTATION_DISABLED;
    SDL_GetRenderLogicalPresentation(g.ren, &lw, &lh, &mode);
    SDL_FRect rc = {0, 0, (float)d.Width, (float)d.Height};
    float sx = 1, sy = 1;
    if (mode != SDL_LOGICAL_PRESENTATION_DISABLED && lw > 0 && lh > 0 && SDL_GetRenderLogicalPresentationRect(g.ren, &rc)) {
      sx = rc.w / (float)lw;
      sy = rc.h / (float)lh;
    } else {
      rc.x = rc.y = 0;
    }
    out.target = rtv_.Get();
    out.targetWidth = (int)d.Width;
    out.targetHeight = (int)d.Height;
    out.x = rc.x + (float)f.x * sx;
    out.y = rc.y + (float)f.y * sy;
    out.width = (float)f.width * sx;
    out.height = (float)f.height * sy;
    return true;
  }
  void screenDone() override { rtv_.Reset(); }  // SDL must be able to resize its swap chain
  const char* describe() const override { return "drawn on SDL's back buffer"; }

 private:
  ComPtr<ID3D11Device> dev_;
  ComPtr<ID3D11DeviceContext> ctx_;
  ComPtr<ID3D11RenderTargetView> rtv_;
  std::shared_ptr<D3D11LossFlag> loss_;
  D3D11StateBackup saved_;
  int depth_ = 0;
  bool targetWarned_ = false;
};

bool envSet(const char* name) {
  const char* v = SDL_getenv(name);
  return v && *v && SDL_strcmp(v, "0") != 0;
}

// A backend on SDL's device when its renderer is Direct3D 11, else on a device of its own. Null with the reason.
// `used` / `sharedWithSdl`: the device it draws with.
std::unique_ptr<Renderer3D> makeBackend(std::string& why, ComPtr<ID3D11Device>& used, bool& sharedWithSdl) {
  g.loss = std::make_shared<D3D11LossFlag>();
  ID3D11Device* shared = envSet("OTE_3D_D3D11_OWN") ? nullptr : sdlDevice();
  sharedWithSdl = shared != nullptr;
  if (shared) {
    used = shared;
    return D3D11Renderer3D::create(std::unique_ptr<D3D11Host>(new SdlD3D11Host(shared, g.loss)), why);
  }
  D3D11DeviceChoice choice;
  if (const char* level = SDL_getenv("OTE_3D_D3D11_LEVEL")) choice.maxLevel = level;
  if (const char* driver = SDL_getenv("OTE_3D_D3D11_DRIVER")) choice.warp = SDL_strcmp(driver, "warp") == 0;
  used = makeD3D11Device(choice, why);
  if (!used) return nullptr;
  return D3D11Renderer3D::create(std::unique_ptr<D3D11Host>(new OwnD3D11Host(used, g.loss)), why);
}

// What ResilientRenderer3D needs: when the device is gone, and a new backend.
class D3D11Keeper : public DeviceKeeper {
 public:
  D3D11Keeper(ComPtr<ID3D11Device> dev, bool sharedWithSdl) : dev_(dev), shared_(sharedWithSdl) {}
  const char* name() const override { return "3D (Direct3D 11)"; }
  const char* lossText() const override {
    return "the Direct3D 11 device was lost (driver updated or reset, graphics chip removed, or SDL reset its renderer)";
  }
  bool lost() override {
    if (!g.ren || !dev_) return false;  // shut down: the backend quietly draws nothing, as the OpenGL one does
    if (g.loss && g.loss->simulated) return true;
    if (dev_->GetDeviceRemovedReason() != S_OK) return true;
    return shared_ && sdlDevice() != dev_.Get();  // SDL made itself a new device
  }
  std::unique_ptr<Renderer3D> rebuild(std::string& why) override {
    dev_.Reset();
    return makeBackend(why, dev_, shared_);
  }
  void simulateLoss() override {
    if (g.loss) g.loss->simulated = true;
  }
  void release() override { dev_.Reset(); }

 private:
  ComPtr<ID3D11Device> dev_;
  bool shared_;
};

}  // namespace

void attachD3D11Renderer(SDL_Renderer* ren) { g.ren = ren; }

std::unique_ptr<Renderer3D> createD3D11HardwareRenderer3D(std::string& why) {
  ComPtr<ID3D11Device> dev;
  bool shared = false;
  std::unique_ptr<Renderer3D> r = makeBackend(why, dev, shared);
  if (!r) return nullptr;
  const char* loss = SDL_getenv("OTE_3D_SIMULATE_LOSS");
  return std::unique_ptr<Renderer3D>(new ResilientRenderer3D(std::move(r),
                                                             std::unique_ptr<DeviceKeeper>(new D3D11Keeper(dev, shared)),
                                                             loss ? SDL_atoi(loss) : 0, envSet("OTE_3D_SIMULATE_NO_REBUILD")));
}

void shutdownD3D11Hardware3D() {
  if (g.loss) g.loss->shutDown = true;  // own devices: draw nothing from now on
  g.ren = nullptr;                      // SDL's: the same (SDL_DestroyRenderer frees the device's last SDL users)
}

}  // namespace ote

#endif  // _WIN32
