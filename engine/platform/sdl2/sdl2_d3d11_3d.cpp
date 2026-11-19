// Windows on SDL2 (the UWP / Xbox app; an SDL2 desktop build would use it too): gives the Direct3D 11 3D backend
// (engine/gfx3d/d3d11) its device. The SDL3 desktop glue is platform/sdl3/sdl_d3d11_3d.cpp; this one does the same
// with what SDL2 offers (docs/RENDER3D.md "Direct3D 11 backend", docs/XBOX.md "3D").
//
// When SDL draws the 2D layer with its Direct3D 11 renderer (the only one SDL2 has on UWP; sdl2_app.cpp asks for it by
// name on Windows) the backend uses SDL's device (SDL_RenderGetD3D11Device: SDL 2.32 leaves it out of WinRT builds,
// tools/uwp/build.ps1 turns it on) and draws the finished 3D picture onto the render target SDL is drawing to, as one
// textured quad under the 2D things drawn afterwards: no read-back, no second device. SDL2 exposes no swap chain, so
// the target and its letterbox come from the context itself: after SDL_RenderFlush (which carries out SDL's queued
// 2D drawing) the bound render target is SDL's back buffer and the bound viewport is SDL's, and SDL_RenderGetScale is
// how many target pixels a logical pixel is. SDL2's Direct3D 11 renderer binds its target and viewport only when it
// draws (after a present the flip-model swap chain unbinds it), so enter() first queues one invisible point (alpha 0,
// blended: the pixel is unchanged) to make SDL bind both. The pipeline state is saved after that and put back exactly
// in leave() (D3D11StateBackup): SDL2's flush does not forget its cached state, so exact restoring is what keeps its
// 2D drawing right.
//
// Otherwise (OTE_3D_D3D11_OWN=1, or SDL's renderer is not Direct3D 11) the backend makes a device of its own and the
// picture is read back into memory and shown through an SDL texture, like the OpenGL backend's.
//
// Losing the device: a driver update or reset, a removed GPU, or SDL2 resetting its renderer (SDL_RENDER_DEVICE_RESET:
// SDL then has a new device). ResilientRenderer3D keeps a CPU copy of the meshes and textures; D3D11Keeper below tells
// it when the device is gone and makes a new backend (on SDL's new device, or a new own device), or the software
// backend takes over.
//
// Suspend (UWP): Windows wants IDXGIDevice3::Trim on every Direct3D device when the app is suspended. SDL 2.32's WinRT
// layer already trims its renderer's device (SDL_winrtapp_direct3d.cpp, OnSuspending: D3D11_Trim, in the suspend
// deferral task right after SDL_APP_DIDENTERBACKGROUND), and that is the device the 3D drawing shares, so nothing more is
// done for it (a second Trim would only repeat SDL's, from another thread). An own device is trimmed here, on
// SDL_APP_WILLENTERBACKGROUND (on the loop's thread, after the last frame: no frame runs until the app is resumed),
// after ClearState as the Trim documentation asks; the backend sets all its state again every frame.
//
// Debug, from the environment (UWP: LocalState\args.txt): as in the SDL3 glue, OTE_3D_D3D11_OWN=1,
// OTE_3D_D3D11_LEVEL=10_1 (or 10_0, 11_0) and OTE_3D_D3D11_DRIVER=warp (both for an own device: SDL makes its own),
// OTE_3D_SIMULATE_LOSS=N, OTE_3D_SIMULATE_NO_REBUILD=1.
#ifdef _WIN32
#include <SDL.h>
#include <SDL_system.h>
#include <d3d11.h>
#include <dxgi1_3.h>
#include <wrl/client.h>

#include <memory>
#include <string>

#include "core/base/log.h"
#include "gfx3d/d3d11/d3d11_device.h"
#include "gfx3d/d3d11/d3d11_renderer3d.h"
#include "gfx3d/d3d11/d3d11_state.h"
#include "gfx3d/resilient3d.h"
#include "platform/sdl2/sdl2_gl3d.h"

namespace ote {
namespace {

using Microsoft::WRL::ComPtr;

struct Shared {
  SDL_Renderer* ren = nullptr;  // the player's 2D renderer (attachHardware3D), null after shutdown
  bool sdlIsD3D11 = false;      // its driver is "direct3d11"
  std::shared_ptr<D3D11LossFlag> loss;  // of the backend made last
  ComPtr<ID3D11Device> own;     // the backend's own device, when it has one (trimmed on suspend)
  bool sharedWithSdl = false;   // the backend made last draws with SDL's device
};
Shared g;

// SDL's device as it is now (SDL_RenderGetD3D11Device adds a reference; the ComPtr takes it over).
ComPtr<ID3D11Device> sdlDevice() {
  ComPtr<ID3D11Device> d;
  if (g.ren && g.sdlIsD3D11) d.Attach(SDL_RenderGetD3D11Device(g.ren));
  return d;
}

// SDL's device, shared with its 2D renderer.
class Sdl2D3D11Host : public D3D11Host {
 public:
  Sdl2D3D11Host(ComPtr<ID3D11Device> dev, std::shared_ptr<D3D11LossFlag> loss) : dev_(dev), loss_(loss) {
    dev_->GetImmediateContext(ctx_.GetAddressOf());
  }
  ID3D11Device* device() override { return g.ren ? dev_.Get() : nullptr; }
  ID3D11DeviceContext* context() override { return g.ren ? ctx_.Get() : nullptr; }
  bool usable() {
    return g.ren && !loss_->simulated && sdlDevice().Get() == dev_.Get() && dev_->GetDeviceRemovedReason() == S_OK;
  }

  bool enter() override {
    if (depth_ > 0) {
      ++depth_;
      return true;
    }
    if (!usable()) return false;
    if (!SDL_GetRenderTarget(g.ren)) bindSdlTarget();
    SDL_RenderFlush(g.ren);  // the 2D drawing queued so far goes first (it lies under the 3D picture)
    saved_.save(ctx_.Get());
    depth_ = 1;
    return true;
  }
  void leave() override {
    if (depth_ == 0 || --depth_ > 0) return;
    saved_.restore(ctx_.Get());
  }

  // The target SDL draws the 2D layer to (its back buffer), and the frame's logical rectangle mapped through SDL's
  // viewport (the letterbox) and scale.
  bool screen(const Frame3D& f, D3D11Screen& out) override {
    if (SDL_GetRenderTarget(g.ren)) {
      // The player never draws 3D into a texture target; should that change, the picture goes through memory.
      if (!warned_) OTE_WARN("3D (Direct3D 11): SDL has a texture as render target; the 3D picture is read back");
      warned_ = true;
      return false;
    }
    ID3D11RenderTargetView* rtv = saved_.savedTarget();
    D3D11_VIEWPORT vp;
    if (!rtv || !saved_.savedViewport(vp)) {
      if (!warned_) OTE_WARN("3D (Direct3D 11): SDL has no render target bound; the 3D picture is read back");
      warned_ = true;
      return false;
    }
    ComPtr<ID3D11Resource> res;
    ComPtr<ID3D11Texture2D> tex;
    rtv->GetResource(res.GetAddressOf());
    if (!res || FAILED(res.As(&tex))) return false;
    D3D11_TEXTURE2D_DESC d;
    tex->GetDesc(&d);
    float sx = 1, sy = 1;
    SDL_RenderGetScale(g.ren, &sx, &sy);
    out.target = rtv;  // held by saved_ until leave(), so nothing to let go of in screenDone()
    out.targetWidth = (int)d.Width;
    out.targetHeight = (int)d.Height;
    out.x = vp.TopLeftX + (float)f.x * sx;
    out.y = vp.TopLeftY + (float)f.y * sy;
    out.width = (float)f.width * sx;
    out.height = (float)f.height * sy;
    return true;
  }
  const char* describe() const override { return "drawn on SDL's render target"; }

 private:
  // SDL2's Direct3D 11 renderer sets its render target and viewport only when it draws something, and a present
  // unbinds the target: one point of alpha 0, blended, makes it bind both without changing a pixel.
  void bindSdlTarget() {
    Uint8 r, gr, b, a;
    SDL_BlendMode mode;
    SDL_GetRenderDrawColor(g.ren, &r, &gr, &b, &a);
    SDL_GetRenderDrawBlendMode(g.ren, &mode);
    SDL_SetRenderDrawBlendMode(g.ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g.ren, 0, 0, 0, 0);
    SDL_RenderDrawPoint(g.ren, 0, 0);
    SDL_SetRenderDrawColor(g.ren, r, gr, b, a);
    SDL_SetRenderDrawBlendMode(g.ren, mode);
  }

  ComPtr<ID3D11Device> dev_;
  ComPtr<ID3D11DeviceContext> ctx_;
  std::shared_ptr<D3D11LossFlag> loss_;
  D3D11StateBackup saved_;
  int depth_ = 0;
  bool warned_ = false;
};

bool envSet(const char* name) {
  const char* v = SDL_getenv(name);
  return v && *v && SDL_strcmp(v, "0") != 0;
}

// A backend on SDL's device when its renderer is Direct3D 11, else on a device of its own. Null with the reason.
std::unique_ptr<Renderer3D> makeBackend(std::string& why, ComPtr<ID3D11Device>& used, bool& sharedWithSdl) {
  g.loss = std::make_shared<D3D11LossFlag>();
  g.own.Reset();
  ComPtr<ID3D11Device> shared = envSet("OTE_3D_D3D11_OWN") ? nullptr : sdlDevice();
  sharedWithSdl = shared != nullptr;
  g.sharedWithSdl = sharedWithSdl;
  if (shared) {
    used = shared;
    return D3D11Renderer3D::create(std::unique_ptr<D3D11Host>(new Sdl2D3D11Host(shared, g.loss)), why);
  }
  D3D11DeviceChoice choice;
  if (const char* level = SDL_getenv("OTE_3D_D3D11_LEVEL")) choice.maxLevel = level;
  if (const char* driver = SDL_getenv("OTE_3D_D3D11_DRIVER")) choice.warp = SDL_strcmp(driver, "warp") == 0;
  used = makeD3D11Device(choice, why);
  if (!used) return nullptr;
  std::unique_ptr<Renderer3D> r = D3D11Renderer3D::create(std::unique_ptr<D3D11Host>(new OwnD3D11Host(used, g.loss)), why);
  if (r) g.own = used;
  return r;
}

// What ResilientRenderer3D needs: when the device is gone, and a new backend. (The same as the SDL3 glue's.)
class D3D11Keeper : public DeviceKeeper {
 public:
  D3D11Keeper(ComPtr<ID3D11Device> dev, bool sharedWithSdl) : dev_(dev), shared_(sharedWithSdl) {}
  const char* name() const override { return "3D (Direct3D 11)"; }
  const char* lossText() const override {
    return "the Direct3D 11 device was lost (driver updated or reset, graphics chip removed, or SDL reset its renderer)";
  }
  bool lost() override {
    if (!g.ren || !dev_) return false;  // shut down: the backend quietly draws nothing
    if (g.loss && g.loss->simulated) return true;
    if (dev_->GetDeviceRemovedReason() != S_OK) return true;
    return shared_ && sdlDevice().Get() != dev_.Get();  // SDL made itself a new device
  }
  std::unique_ptr<Renderer3D> rebuild(std::string& why) override {
    dev_.Reset();
    return makeBackend(why, dev_, shared_);
  }
  void simulateLoss() override {
    if (g.loss) g.loss->simulated = true;
  }
  void release() override {
    dev_.Reset();
    g.own.Reset();
    g.sharedWithSdl = false;
  }

 private:
  ComPtr<ID3D11Device> dev_;
  bool shared_;
};

// Suspend (UWP). Called by SDL for every event, on the thread that sends it.
int SDLCALL suspendWatch(void*, SDL_Event* e) {
  if (e->type == SDL_APP_WILLENTERBACKGROUND && g.own) {
    ComPtr<ID3D11DeviceContext> ctx;
    ComPtr<IDXGIDevice3> dxgi;
    g.own->GetImmediateContext(ctx.GetAddressOf());
    if (SUCCEEDED(g.own.As(&dxgi))) {
      ctx->ClearState();
      dxgi->Trim();
      OTE_INFO("3D (Direct3D 11): suspending: own device trimmed (IDXGIDevice3::Trim)");
    }
  } else if (e->type == SDL_APP_DIDENTERBACKGROUND && g.sharedWithSdl) {
    // Sent from SDL's suspend task right before it trims its renderer's device, the one the 3D drawing uses.
    OTE_INFO("3D (Direct3D 11): suspending: the device is SDL's, which SDL trims now");
  }
  return 0;
}

}  // namespace

void attachD3D11Renderer(SDL_Renderer* ren) {
  g.ren = ren;
  SDL_RendererInfo info;
  g.sdlIsD3D11 = ren && SDL_GetRendererInfo(ren, &info) == 0 && SDL_strcmp(info.name, "direct3d11") == 0;
  if (ren) SDL_AddEventWatch(suspendWatch, nullptr);
}

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
  if (g.ren) SDL_DelEventWatch(suspendWatch, nullptr);
  if (g.loss) g.loss->shutDown = true;  // own devices: draw nothing from now on
  g.ren = nullptr;                      // SDL's: the same (SDL_DestroyRenderer frees the device's last SDL users)
  g.own.Reset();
  g.sharedWithSdl = false;
}

}  // namespace ote

#endif  // _WIN32
