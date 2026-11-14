// Platform abstraction: the ONLY interface the player uses to reach the
// device (window, drawing, input, main loop). Each port implements this
// once: engine/platform/sdl3/ and sdl2/ today; a console SDK later. Nothing
// outside engine/platform/<backend>/ may include backend headers.
//
// Kept deliberately tiny and "lowest common denominator" (solid rects,
// text via the shared engine/platform/common/text_engine, textured sprites) so it fits weak hardware; fancier
// backends can add optional capabilities without changing these calls.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace ote {

struct Color {
  uint8_t r = 0, g = 0, b = 0, a = 255;
  Color() = default;
  Color(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255) : r(r_), g(g_), b(b_), a(a_) {}
  // From 0xRRGGBB.
  static Color rgb(uint32_t v, uint8_t alpha = 255) {
    return Color((uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v, alpha);
  }
};

// Logical buttons. Backends map keyboard, gamepad and touch onto these, so
// game code never sees device-specific keys.
enum class Button { None, Up, Down, Left, Right, Confirm, Cancel, Menu, Next, Prev };

struct InputEvent {
  enum Type { ButtonDown, ButtonUp, PointerMove, PointerDown, PointerUp, Quit };
  Type type = ButtonDown;
  Button button = Button::None;
  int x = 0, y = 0;  // pointer position in LOGICAL pixels
};

// Which font to draw text with. An empty file means the built-in 8x8 pixel
// font; it is never smooth and its size rounds to a multiple of 8 (8, 16, ...).
struct FontRequest {
  std::string file;   // path to a .ttf/.otf file
  int size = 8;       // text height (line box) in logical pixels at scale 1
  bool smooth = true; // true: anti-aliased, rasterized at the display's real
                      // resolution; false: hard pixels at logical resolution
};

// Handle of an image the platform holds for drawing (0 = none).
typedef int TextureId;

// Immediate-mode 2D drawing in logical pixels (the backend scales the
// logical screen to the real display, keeping aspect ratio).
class Renderer {
 public:
  virtual ~Renderer() {}
  virtual int width() const = 0;
  virtual int height() const = 0;
  virtual void clear(Color color) = 0;
  virtual void fillRect(int x, int y, int w, int h, Color color) = 0;  // alpha blended
  virtual void drawRect(int x, int y, int w, int h, Color color) = 0;  // 1px outline

  // Changes the logical screen size (e.g. when a game with a different
  // "view" size starts). Drawing coordinates follow immediately.
  virtual void setLogicalSize(int width, int height) = 0;

  // Selects the font for all following text calls. On failure returns false,
  // logs why, and falls back to the built-in pixel font.
  virtual bool setFont(const FontRequest& font) = 0;
  // Text is UTF-8. (x, y) is the top-left of the line box. `scale` is an
  // integer multiplier on the font size.
  virtual void drawText(int x, int y, const char* text, Color color, int scale = 1) = 0;
  // Width of the text in logical pixels / height of one line (use these for
  // ALL layout; never assume a fixed glyph size).
  virtual int textWidth(const char* text, int scale = 1) const = 0;
  virtual int lineHeight(int scale = 1) const = 0;

  // Images (sprite sheets). Creates a texture from 32-bit pixels (R,G,B,A
  // bytes, row by row, not premultiplied). Nearest-neighbour scaling. Returns
  // 0 on failure (logged). Textures survive setLogicalSize; whatever is still
  // alive when the renderer goes away is freed with it.
  virtual TextureId createTexture(const uint8_t* rgba, int width, int height) = 0;
  virtual void destroyTexture(TextureId id) = 0;
  // Draws part of a texture (sx,sy,sw,sh in texture pixels) into a rectangle
  // in logical pixels, alpha blended. `mod` multiplies colour and alpha
  // (white = as is). Unknown ids draw nothing.
  virtual void drawTexture(TextureId id, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh,
                           Color mod = Color(255, 255, 255)) = 0;
};

// What the player implements.
class App {
 public:
  virtual ~App() {}
  virtual void onInput(const InputEvent& event) = 0;
  virtual void update(double dtSeconds) = 0;
  virtual void draw(Renderer& renderer) = 0;
  // Called once by the backend after the window/renderer exist and before
  // the first frame (set the font / logical size here, load content).
  virtual void onStart(Renderer& renderer) {}
  virtual bool wantsQuit() const = 0;
};

struct PlatformConfig {
  std::string title = "OpenTacticsEngine";
  int logicalWidth = 480, logicalHeight = 270;
  int windowScale = 3;        // initial window = logical size * scale
  // Test/automation support:
  bool headless = false;      // no visible window (offscreen software rendering)
  double fixedDt = 0;         // > 0: update() gets exactly this dt every frame
  int maxFrames = 0;          // > 0: quit after this many frames
  std::string screenshotPath; // non-empty: save the final frame as BMP on exit
};

// Runs the main loop until app.wantsQuit() / window close / maxFrames.
// Implemented by the active backend. Returns a process exit code.
int runApp(App& app, const PlatformConfig& config);

// ---- Files (implemented by the backend) ----

// Writable folder for the user's settings and saves, created if missing,
// without a trailing slash. Empty string if the platform has none.
std::string userDataDir();

// Call after writing files inside userDataDir(): lets the platform persist
// them (the web build copies its in-memory folder to browser storage; every
// other platform writes through already, so this does nothing there).
void commitUserData();

// Names (not paths) of the sub-folders directly inside `path`, in no
// particular order. Returns false if the folder cannot be read or the
// platform cannot list folders (callers must then rely on index files).
bool listSubdirs(const std::string& path, std::vector<std::string>& names);

// Creates the folder (one level; callers create parents first). True if the
// folder exists afterwards, including when it already did.
bool makeDir(const std::string& path);

// Deletes a file. True if the file is gone afterwards (also when it never
// existed).
bool removeFile(const std::string& path);

}  // namespace ote
