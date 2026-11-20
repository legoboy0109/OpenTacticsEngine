// The original Xbox's graphics chip (NV2A) through nxdk's pbkit: bring-up, frames, memory, and the few things a draw needs
// (docs/RENDER3D.md "Original Xbox backend", docs/XBOX_OG.md "3D"). Everything above this file (the Renderer3D, the 2D
// layer's composition) speaks in these small types and never writes a push-buffer command itself.
//
// One vertex program and one pixel stage do all the work (shaders/hw3d_vp.vsh, hw3d_ps.rc); this layer only fills their
// constants and switches the fixed state around them.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace ote {
namespace hw3d {

struct Rect {
  int x = 0, y = 0, w = 0, h = 0;
};

// One texture as the chip reads it. Linear (any size, 2D canvas) or swizzled (power of two, repeat and mipmaps).
struct HwTexture {
  uint32_t offset = 0;  // physical address
  uint32_t format = 0;  // NV097_SET_TEXTURE_FORMAT word
  uint32_t pitch = 0;   // linear only: bytes per row
  uint32_t rect = 0;    // linear only: (width << 16) | height
  bool linear = false;
};

enum class Sampling { Nearest, Linear, Trilinear };
enum class Edge { Repeat, Clamp };
enum class Mix { Replace, Alpha, Premultiplied };

// The switches of one draw.
struct Pipe {
  Mix mix = Mix::Replace;
  bool depthTest = true;
  bool depthWrite = true;
  bool cull = true;
  bool frontCw = false;  // which winding (as seen on the screen) is a front face: ours is counter-clockwise
  bool cullFront = false;  // debug: cull the front faces instead of the back ones
  int alphaRef = -1;  // 0..255: pixels with alpha below it are left out; -1: all pass
  Sampling sampling = Sampling::Nearest;
  Edge edge = Edge::Repeat;
};

// The vertex program's constants c[0] .. c[17], in this order (hw3d_vp.vsh).
struct VsConstants {
  float clip[16];  // four rows of projection * view * model
  float scale[4];  // window scale
  float offset[4]; // window offset
  float rot[12];   // three rows of the model's rotation part
  float toSun[4];
  float ambient[4];
  float sun[4];
  float color[4];  // material colour
  float mv[12];    // three rows of view * model
  float fog[4];    // (1 / (end - start), -start / (end - start), 0, 0)
  float fogColor[4];
};
static_assert(sizeof(VsConstants) == 18 * 16, "constants are c[0]..c[17]");

// ---- bring-up ----------------------------------------------------------------------------------------------------------
// Starts pbkit once (frame buffers, depth buffer, push buffer), loads the two programs. False with a plain reason.
bool start(std::string& why);
bool running();
int width();   // the video mode's size
int height();
size_t pushBufferBytes();
size_t lastFramePushBytes();  // commands the last finished frame used (of pushBufferBytes())

// ---- frames ------------------------------------------------------------------------------------------------------------
// Opens this frame's push buffer: waits for the vertical blank and for the chip to finish the previous frame, then sets
// the fixed state. Safe to call again in the same frame.
void beginFrame();
bool inFrame();
// Waits for the chip to finish what was sent so far (before vertex or texture memory is changed or freed).
void waitIdle();
// Sends the frame to the screen at the next vertical blank (waits for the chip first).
void endFrame();
// Number of draw commands dropped this frame because the push buffer was full (0 normally).
int droppedDraws();

// ---- memory ------------------------------------------------------------------------------------------------------------
// Contiguous, write-combined memory the chip reads (textures, vertices). Null when the system has none left.
void* alloc(size_t bytes);
void release(void* memory);
uint32_t physical(const void* memory);  // the address the chip uses
void flushWrites();                      // before the chip may read what the CPU just wrote
// Memory for vertices made during a frame (triangles cut at the camera): valid until the frame is over, null when this
// frame's share (1 MB) is used up.
void* scratch(size_t bytes);

// ---- drawing -----------------------------------------------------------------------------------------------------------
void setScissor(const Rect& r);
void clearDepth(const Rect& r);
void setPipe(const Pipe& pipe, const HwTexture& tex);
void setConstants(const VsConstants& c);
// Constants that make the program pass window-pixel positions through untouched, unlit, with `color` as the colour (the 2D
// layer and flat clears are drawn with these).
void screenSpaceConstants(VsConstants& c, float r, float g, float b, float a);
// Vertices as Vertex3D lies in memory (36 bytes: position, normal, texture position, 4 colour bytes) at `physVertices`.
// `triangles` x 3 16-bit indices. False (and the draw is dropped) when the push buffer has no room.
bool drawIndexed(uint32_t physVertices, const uint16_t* indices, size_t triangleCount);
// Same, from `count` loose vertices (a triangle list), no indices.
bool drawLoose(uint32_t physVertices, size_t count);
// A rectangle given in window pixels with texture positions u0..u1, v0..v1 (the texture's own units).
bool drawQuad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, uint32_t colorArgb);

// The picture drawn so far this frame (for screenshots): 32-bit pixels B,G,R,A; null when not available.
const uint8_t* backBuffer(int& w, int& h, int& pitch);

}  // namespace hw3d
}  // namespace ote
