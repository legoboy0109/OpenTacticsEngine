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
  Skip,  // "skip everything" in a dialogue: no key or pad button makes it, only the --input script (and tests)
  Ranges  // battle: show / hide the reach of every enemy you can see (key T, pad Back / View; player/battle/enemy_ranges.h). Last: the
          // buttons before it keep their numbers.
};

// Which kind of input device made an event. The player names the controls of the device last used in its hint lines
// (player/ui/input_hints.h). A backend sets it on every event it translates; None = "no information, do not switch" (the
// system back request, the mouse wheel, quit). Scripted input (--input) is Keyboard unless the script says otherwise.
// Touch also stands for a mouse press: both are "tap" in the hints.
enum class InputDevice { None, Keyboard, Controller, Touch, Mouse };

struct InputEvent {
  // CameraDrag: the pointer's second button (right mouse button) is held and
  // the pointer moved; x, y are how far, not where.
  enum Type { ButtonDown, ButtonUp, PointerMove, PointerDown, PointerUp, Quit, CameraDrag };
  Type type = ButtonDown;
  Button button = Button::None;
  int x = 0, y = 0;  // pointer position in LOGICAL pixels
  InputDevice device = InputDevice::None;
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

  // How much of a TV screen the picture may use, in percent (the rest is a black margin for an old TV's overscan); 0 = the
  // platform's default. Only the original Xbox backend acts on it (docs/XBOX_OG.md "Picture"); everywhere else it is ignored.
  virtual void setSafeArea(int /*percent*/) {}

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
  virtual bool setAdditiveBlend(bool /*on*/) { return false; }
};

// Life-cycle notices from the operating system (phones, consoles; docs/PORTING.md "Life cycle"). Desktop backends
// send none today. They arrive through App::onLifecycle, possibly right before the OS freezes the process, so the
// handler must finish what it has to write before it returns.
enum class Lifecycle {
  WillEnterBackground,  // the app is about to be hidden / suspended: write everything now
  DidEnterForeground,   // the app is visible and running again
  LowMemory,            // the OS asks to give memory back
  Terminating           // the OS is about to end the process (may follow WillEnterBackground with no further warning)
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
  virtual void onStart(Renderer& /*renderer*/) {}
  // A life-cycle notice (see Lifecycle). Backends call it from their event loop; where the OS may freeze the process
  // right after the notice (SDL's WILL_ENTER_BACKGROUND / TERMINATING) they call it from an event watch, at once,
  // with the main loop locked out, instead of queueing it.
  virtual void onLifecycle(Lifecycle /*event*/) {}
  virtual bool wantsQuit() const = 0;
  // Called once by the backend after the main loop ends and before the window / SDL go away (never
  // on the web, where the page stays up): release anything that needs the device (the audio output).
  virtual void onStop() {}
};

struct PlatformConfig {
  std::string title = "OpenTacticsEngine";
  int logicalWidth = 480, logicalHeight = 270;
  // Initial window = logical size * scale; 0 = the largest whole scale that fits the display's usable area
  // (platform/common/window_fit.h; desktop only, at least 1). Headless windows ignore it (headlessScale).
  int windowScale = 0;
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

// ---- Where things live (docs/PORTING.md "Roots") ----
// The player reads shipped content from one place and writes to another; on Android the first is app storage the
// assets were unpacked into, on UWP the read-only install folder and the writable LocalState folder. Everything the
// player reads or writes is under one of these, so a port changes the answers here and nothing else.

// Folder holding the shipped, read-only content: it contains games/ and assets/. Empty = the working directory
// (the desktop default, so `ote-player` run from the repository finds games/ and assets/). No trailing slash.
// Overridden by setContentRootOverride (the player's --content-root) or the OTE_CONTENT_ROOT environment variable.
std::string contentRoot();
// Optional second folder of games (same layout as games/: <id>/game.json) that the user added; empty = none.
// Overridden by setExtraContentRootOverride (--extra-games) or OTE_EXTRA_GAMES.
std::string extraContentRoot();
// Folder the app may write to: settings.json and saves/ live here. Empty if there is none. Today this is userDataDir().
std::string userRoot();
void setContentRootOverride(const std::string& dir);
void setExtraContentRootOverride(const std::string& dir);

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

// Moves `from` over `to`, replacing `to` if it exists (the save writer writes "x.tmp" and then replaces "x"). Paths are
// UTF-8; on Windows std::rename would read them in the ANSI code page and refuses to replace, so it is a platform call.
bool replaceFile(const std::string& from, const std::string& to);

// Folder the program file lives in, without a trailing slash; empty if unknown (web, Android) or if the platform has no
// such notion. contentRoot() uses it so an unpacked download runs from anywhere (docs/WINDOWS.md).
std::string executableDir();

// Windows only (a no-op everywhere else): the player is a GUI-subsystem program so a double-click opens no console
// window; when it was started from a terminal this attaches to that terminal so log lines and --help output show up
// there. Does nothing if stdout/stderr are already redirected to a file or pipe. Call first thing in main().
void attachParentConsole();

// How much memory the process uses, for the log ("working set 90 MB, app memory 88 MB of limit 1024 MB"). Only the UWP
// build answers (its apps have a hard budget, about 1 GB on Xbox); empty everywhere else, because a log line that differs
// from run to run would break the tests that compare whole logs. The player logs it at start and after every scene change.
std::string memoryStatus();

// How many megabytes of art (sheet textures, their mirrored copies and kept pixel copies) the player may hold at once;
// beyond it the sheets used longest ago are dropped and loaded again when drawn (docs/GRAPHICS.md "Art memory"). Per
// platform: web 384, UWP 256, Android 192, everything else 768. The environment variable OTE_ART_BUDGET_MB (a whole
// number, 1 or more) replaces it, for tests and for trying a small device on a big one.
unsigned artBudgetMB();

// Extra command-line words for a program that cannot be given a command line (the UWP app: LocalState/args.txt, a
// testing aid; docs/XBOX.md). Empty everywhere else and when there is no such file. A word NAME=value is put into the
// environment instead of being returned.
std::vector<std::string> platformLaunchArgs();

// A name (a game id) that is to become a file or folder name under userRoot(). Unchanged everywhere except the original
// Xbox, whose hard disk accepts only a small set of names (core/base/fatx_names.h, docs/XBOX_OG.md): there it is encoded.
std::string platformFileName(const std::string& name);

// What Cancel (back / B) does on the launcher's first screen. The answer is the platform's convention, so the player has no
// platform #ifdef: Android's back key leaves the app (Exit); Xbox / UWP asks first (ConfirmExit); everywhere else a stray
// Esc must not close the player (Nothing). The environment variable OTE_LAUNCHER_BACK=nothing|exit|confirm overrides it
// (tests; on UWP it can go into LocalState/args.txt as a NAME=value word).
enum class LauncherBack { Nothing, Exit, ConfirmExit };
LauncherBack launcherBack();

// The device a person starts with before any input: the controller on UWP / Xbox / the original Xbox, touch on Android,
// the keyboard everywhere else (web included). OTE_INPUT_DEVICE=keyboard|controller|touch overrides it (tests).
InputDevice naturalInputDevice();

}  // namespace ote
