// A Direct3D 11 device of the 3D backend's own, for when no 2D renderer shares one: windowless runs and tests, or a
// 2D renderer that is not Direct3D 11. The picture is then read back into memory (framePixels), as with OpenGL.
// Windows only. Usable by any platform glue (SDL3 desktop, SDL2 UWP), it names no SDL.
#pragma once
#ifdef _WIN32
#include <d3d11.h>
#include <wrl/client.h>

#include <memory>
#include <string>

#include "gfx3d/d3d11/d3d11_host.h"

namespace ote {

struct D3D11DeviceChoice {
  // The highest feature level to ask for: "11_1" (default, the best the machine has), "11_0", "10_1" (what an Xbox One
  // gives UWP apps) or "10_0". Debug: OTE_3D_D3D11_LEVEL in the environment, read by the platform glue.
  std::string maxLevel;
  bool warp = false;  // Microsoft's software rasteriser instead of the graphics chip (debug: OTE_3D_D3D11_DRIVER=warp)
};

// Shared by a host and the platform's DeviceKeeper: a simulated loss (OTE_3D_SIMULATE_LOSS) makes enter() fail until
// a new host is made.
struct D3D11LossFlag {
  bool simulated = false;
  bool shutDown = false;  // the platform has shut down: draw nothing, never rebuild
};

class OwnD3D11Host : public D3D11Host {
 public:
  OwnD3D11Host(Microsoft::WRL::ComPtr<ID3D11Device> dev, std::shared_ptr<D3D11LossFlag> loss);
  ID3D11Device* device() override { return loss_->shutDown ? nullptr : dev_.Get(); }
  ID3D11DeviceContext* context() override { return loss_->shutDown ? nullptr : ctx_.Get(); }
  bool enter() override;
  void leave() override {}
  const char* describe() const override { return "own device, picture read back"; }

 private:
  Microsoft::WRL::ComPtr<ID3D11Device> dev_;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> ctx_;
  std::shared_ptr<D3D11LossFlag> loss_;
};

// A new device as chosen; null with a plain reason in `why`.
Microsoft::WRL::ComPtr<ID3D11Device> makeD3D11Device(const D3D11DeviceChoice& choice, std::string& why);

// True when the device runs on Microsoft's software rasteriser (WARP / "Microsoft Basic Render Driver": a PC without a
// graphics driver, or a VM). Correct but slow: the backend then does not claim to be fast enough for the display's
// resolution (Caps3D::fullResolution).
bool isSoftwareD3D11Device(ID3D11Device* dev);

}  // namespace ote

#endif  // _WIN32
