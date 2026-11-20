// How the 2D layer reaches the Xbox's screen, and how a hardware 3D picture gets under it (docs/XBOX_OG.md "3D",
// docs/RENDER3D.md "Original Xbox backend").
//
// The 2D layer is SDL's CPU renderer drawing into one 32-bit ARGB surface (the "canvas", the size of the video mode). With no
// 3D picture in sight the canvas is copied to the frame buffer, as the first version of the port did. The first time a 3D
// backend is made, the NV2A takes the screen over (pbkit): from then on every frame is composed by the chip:
//   2D drawn before the 3D picture  ->  the picture (3D drawing)  ->  2D drawn after it, blended on top,
// where the 2D "before" and "after" are the canvas as it is when the 3D picture begins and what the 2D renderer draws into
// the cleared canvas afterwards (transparent where nothing is drawn; SDL's own alpha blending keeps it premultiplied).
#pragma once
#include <string>

struct SDL_Surface;

namespace ote {

// Where the game's logical canvas lies on the screen, in screen pixels (screen_fit.h, set whenever it changes).
struct NxdkFit {
  int x = 0, y = 0, w = 0, h = 0;
  float kx = 1, ky = 1;
};

// The 2D layer's surface (ARGB8888, whole screen). Set once, before the first frame.
void nxdkSetCanvas(SDL_Surface* canvas);
void nxdkSetFit(const NxdkFit& fit);
// True once the NV2A owns the screen (a hardware 3D backend was made): then nxdkPresent composes on the chip.
bool nxdkChipPresents();

// Called at the end of every frame, after everything is drawn: shows the frame. `screenshot` = a BMP path for this frame
// (whole screen, margins included) or empty. In chip mode this waits for the vertical blank itself.
void nxdkPresent(const std::string& screenshot);

namespace hw3d {

// A 3D picture begins: whatever the 2D layer has drawn so far goes to the screen (opaque if it is the frame's first layer,
// blended over the earlier ones otherwise) and the canvas is cleared to transparent for what is drawn on top.
void canvasLayerDown();
// The 3D picture's rectangle (logical canvas pixels) in screen pixels.
void canvasRect(int x, int y, int w, int h, int& sx, int& sy, int& sw, int& sh);

}  // namespace hw3d
}  // namespace ote
