// enter()/leave() of a D3D11Host for one block (like the GL backend's Use). Windows only.
#pragma once
#ifdef _WIN32
#include "gfx3d/d3d11/d3d11_host.h"

namespace ote {

struct D3D11Use {
  D3D11Host& host;
  bool ok;
  explicit D3D11Use(D3D11Host& h) : host(h), ok(h.enter()) {}
  ~D3D11Use() {
    if (ok) host.leave();
  }
  D3D11Use(const D3D11Use&) = delete;
  D3D11Use& operator=(const D3D11Use&) = delete;
};

}  // namespace ote

#endif  // _WIN32
