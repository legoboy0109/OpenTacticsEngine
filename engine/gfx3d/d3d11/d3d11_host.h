// Where the platform gives the Direct3D 11 backend its device (docs/RENDER3D.md "Direct3D 11 backend"), like GlHost
// does for OpenGL. Windows only; nothing outside engine/gfx3d/d3d11 and engine/platform includes this.
//
// Two kinds of host:
// - Sharing a renderer's device (SDL's own Direct3D 11 2D renderer on the desktop; SDL2's on UWP / Xbox): enter()
//   lets the 2D renderer finish its queued drawing and saves the pipeline state, leave() puts it back
//   (D3D11StateBackup), and screen() says where on the back buffer the 3D picture goes. The picture is then drawn
//   there as one textured quad, under whatever the 2D renderer draws afterwards; framePixels() is null.
// - A device of its own (windowless runs, tests, or when the 2D renderer is not Direct3D 11): screen() says no, and
//   the backend reads the finished picture back into memory for framePixels(), like the OpenGL backend.
#pragma once
#ifdef _WIN32

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11RenderTargetView;

namespace ote {

struct Frame3D;

// A place on a render target, in its pixels (fractions allowed: a letterboxed screen scales by any factor).
struct D3D11Screen {
  ID3D11RenderTargetView* target = nullptr;  // the host's, valid until screenDone()
  int targetWidth = 0, targetHeight = 0;
  float x = 0, y = 0, width = 0, height = 0;
};

class D3D11Host {
 public:
  virtual ~D3D11Host() {}
  // The device and its immediate context; null once the platform has shut down (draw nothing then). The backend
  // takes its own reference only for as long as it uses them inside enter()/leave().
  virtual ID3D11Device* device() = 0;
  virtual ID3D11DeviceContext* context() = 0;
  // Before the backend uses the context. False when the device is gone (shut down, removed or reset, or a simulated
  // loss): draw nothing and report nothing; the platform's DeviceKeeper notices and rebuilds.
  virtual bool enter() = 0;
  // After; puts back whatever another user of the context expects.
  virtual void leave() = 0;
  // Inside enter()/leave(), when a frame is finished: where the picture of `frame` goes (true), or false: nowhere, the
  // backend reads it back into memory. The rectangle is the frame's x, y, width, height mapped to the target.
  virtual bool screen(const Frame3D& /*frame*/, D3D11Screen& /*out*/) { return false; }
  // After the picture was drawn on the screen() target: let go of it (a swap chain cannot be resized while a view of
  // its back buffer is held).
  virtual void screenDone() {}
  // Where the device comes from, for the log line ("SDL's renderer", "own device, picture read back").
  virtual const char* describe() const { return ""; }
};

}  // namespace ote

#endif  // _WIN32
