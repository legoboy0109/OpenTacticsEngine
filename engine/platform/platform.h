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
#include <memory>
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
// TurnLeft / TurnRight turn the camera in views that can turn; TiltUp /
// TiltDown and ZoomIn / ZoomOut move the free camera of the full 3D view
// (docs/VIEWS.md).
enum class Button {
  None, Up, Down, Left, Right, Confirm, Cancel, Menu, Next, Prev, TurnLeft, TurnRight, TiltUp, TiltDown, ZoomIn, ZoomOut,
  Skip  // "skip everything" in a dialogue: no key or pad button makes it, only the --input script (and tests)
};

struct InputEvent {
  // CameraDrag: the pointer's second button (right mouse button) is held and
  // the pointer moved; x, y are how far, not where.
  enum Type { ButtonDown, ButtonUp, PointerMove, PointerDown, PointerUp, Quit, CameraDrag };
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
  // The real size in device pixels of the area the logical screen is shown in
  // (the letterboxed rectangle, not the whole window; on the web the canvas's
  // real pixels, devicePixelRatio included). Follows window resizes. Headless
  // runs have no display: there it is the logical size times
  // PlatformConfig::headlessScale. Used to draw 3D at the display's resolution.
  virtual void displaySize(int& width, int& height) const = 0;
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
  // bytes, row by row, not premultiplied). Nearest-neighbour scaling, or with
  // `smooth` linear (for a picture drawn at about its own pixel size or
  // smaller, like the 3D picture, which must not be doubled or blurred up).
  // Returns 0 on failure (logged). Textures survive setLogicalSize; whatever is still
  // alive when the renderer goes away is freed with it.
  virtual TextureId createTexture(const uint8_t* rgba, int width, int height, bool smooth = false) = 0;
  virtual void destroyTexture(TextureId id) = 0;
  // Replaces all pixels of a texture with a picture of the same size (a
  // picture that changes every frame, like the software 3D layer's).
  virtual void updateTexture(TextureId id, const uint8_t* rgba) = 0;
  // Draws part of a texture (sx,sy,sw,sh in texture pixels) into a rectangle
  // in logical pixels, alpha blended. `mod` multiplies colour and alpha
  // (white = as is). Unknown ids draw nothing.
  virtual void drawTexture(TextureId id, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh,
                           Color mod = Color(255, 255, 255)) = 0;
  // Additive drawing for the textures drawn next (docs/BATTLE_SCENE.md "Blending"): on = the picture's colour, times its alpha and `mod`, is ADDED to
  // what is under it (it only brightens); off (the default) = alpha blended. Returns false when the backend cannot do it: the call then changes
  // nothing and pictures are drawn normally. Always switch it off again after the pictures that wanted it.
  virtual bool setAdditiveBlend(bool on) { return false; }
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
  // Called once by the backend after the main loop ends and before the window / SDL go away (never
  // on the web, where the page stays up): release anything that needs the device (the audio output).
  virtual void onStop() {}
};

struct PlatformConfig {
  std::string title = "OpenTacticsEngine";
  int logicalWidth = 480, logicalHeight = 270;
  int windowScale = 3;        // initial window = logical size * scale
  // Test/automation support:
  bool headless = false;      // no visible window (offscreen software rendering)
  int headlessScale = 1;      // headless: the "display" is the logical size times this (1..8), so screenshots
                              // and the 3D picture can be taken at a higher resolution
  double fixedDt = 0;         // > 0: update() gets exactly this dt every frame
  int maxFrames = 0;          // > 0: quit after this many frames
  std::string screenshotPath; // non-empty: save the final frame as BMP on exit
};

// Runs the main loop until app.wantsQuit() / window close / maxFrames.
// Implemented by the active backend. Returns a process exit code.
int runApp(App& app, const PlatformConfig& config);

// ---- Audio output (implemented by the backend; docs/SOUND.md) ----

// Where mixed sound goes: 16-bit interleaved samples at the rate it was opened with. The player
// mixes on its main loop and pushes what the device is missing, so there are no threads and no
// callbacks into the player; a port only has to play the buffers it is given.
class AudioOut {
 public:
  virtual ~AudioOut() {}
  // false = the null device: nothing is played (headless runs, --no-audio, no sound hardware).
  virtual bool available() const = 0;
  virtual int rate() const = 0;      // frames per second the device wants pushed
  virtual int channels() const = 0;  // 2 (stereo) for every real device
  // Frames pushed but not yet played. Never blocks.
  virtual int queuedFrames() const = 0;
  // Adds `frames` frames (frames * channels() samples) to the end of the queue. Never blocks.
  virtual void push(const int16_t* samples, int frames) = 0;
  virtual const char* name() const = 0;  // for the log, e.g. "SDL3 audio"
};

// Plays nothing; queuedFrames() is always 0. Portable (platform/common/null_audio.cpp).
std::unique_ptr<AudioOut> createNullAudioOut(int rate);

// Opens the default output device at `rate`, stereo (the backend converts if the hardware wants
// something else). Null with a plain reason in `why` when there is no usable device: a missing
// sound card is not an error, the caller then falls back to the null device.
std::unique_ptr<AudioOut> openAudioOut(int rate, std::string& why);

// True where the system only lets sound start after the user's first key press or tap (the web):
// the player then opens the device on its first input instead of at start-up.
bool audioWaitsForUserInput();

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
