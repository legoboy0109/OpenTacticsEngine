// How big the 3D picture is (docs/RENDER3D.md "Resolution"): the option's
// layering and Show3D::fit, with a stand-in for the display.
#include "gfx3d/soft3d.h"
#include "options/effective.h"
#include "show3d.h"
#include "test.h"

using namespace ote;

namespace {

// A 2D renderer that only knows its sizes: logical 480 x 270 shown in a
// letterboxed rectangle of `dw` x `dh` real pixels.
class FakeScreen : public Renderer {
 public:
  int dw = 480, dh = 270;
  int width() const override { return 480; }
  int height() const override { return 270; }
  void displaySize(int& w, int& h) const override { w = dw; h = dh; }
  void clear(Color) override {}
  void fillRect(int, int, int, int, Color) override {}
  void drawRect(int, int, int, int, Color) override {}
  void setLogicalSize(int, int) override {}
  bool setFont(const FontRequest&) override { return true; }
  void drawText(int, int, const char*, Color, int) override {}
  int textWidth(const char*, int) const override { return 0; }
  int lineHeight(int) const override { return 8; }
  TextureId createTexture(const uint8_t*, int, int, bool) override { return 0; }
  void destroyTexture(TextureId) override {}
  void updateTexture(TextureId, const uint8_t*) override {}
  void drawTexture(TextureId, int, int, int, int, int, int, int, int, Color) override {}
};

// A 3D layer that claims to be fast enough for the display's resolution (the OpenGL one).
class FastLayer : public SoftRenderer3D {
 public:
  Caps3D caps() const override {
    Caps3D c = SoftRenderer3D::caps();
    c.fullResolution = true;
    return c;
  }
};

Frame3D whole() {
  Frame3D f;
  f.width = 480;
  f.height = 270;
  return f;
}

}  // namespace

TEST(res3d_option_layers_args_over_game_over_global) {
  LookLayer none, args, game, global;
  GameLook gl;
  CHECK(resolveLook("assets", none, none, none, gl).res3d.empty());  // unset = automatic
  global.res3d = "game";
  CHECK_STR(resolveLook("assets", none, none, global, gl).res3d, "game");
  game.res3d = "display";
  CHECK_STR(resolveLook("assets", none, game, global, gl).res3d, "display");
  args.res3d = "game";
  CHECK_STR(resolveLook("assets", args, game, global, gl).res3d, "game");
  args.res3d = "bogus";  // anything else is "no opinion"
  CHECK_STR(resolveLook("assets", args, game, global, gl).res3d, "display");
}

TEST(show3d_fit_sizes_the_picture) {
  FakeScreen screen;
  SoftRenderer3D soft;  // slow: automatic keeps the game's size
  FastLayer fast;
  Show3D show;
  Frame3D f = whole();

  screen.dw = 1920;
  screen.dh = 1080;
  show.fit(screen, soft, f);
  CHECK_EQ(f.renderWidth, 0);
  show.fit(screen, fast, f);  // automatic + fast backend = the display's pixels, edges smoothed
  CHECK_EQ(f.renderWidth, 1920);
  CHECK_EQ(f.renderHeight, 1080);
  CHECK_EQ(f.samples, 4);

  show.setResolution("game");
  show.fit(screen, fast, f);
  CHECK_EQ(f.renderWidth, 0);
  CHECK_EQ(f.samples, 0);
  show.setResolution("display");
  show.fit(screen, soft, f);  // asked for explicitly: even the slow backend
  CHECK_EQ(f.renderWidth, 1920);

  // Letterbox: the picture follows the picture rectangle, not the window.
  screen.dw = 1440;
  screen.dh = 810;
  show.fit(screen, soft, f);
  CHECK_EQ(f.renderWidth, 1440);
  CHECK_EQ(f.renderHeight, 810);
  // A part of the screen gets the same pixels per logical pixel.
  Frame3D part = whole();
  part.width = 240;
  show.fit(screen, soft, part);
  CHECK_EQ(part.renderWidth, 720);

  // A window smaller than the game keeps the game's size; a huge display is capped at 4096 on the longer side.
  screen.dw = 240;
  screen.dh = 135;
  show.fit(screen, soft, f);
  CHECK_EQ(f.renderWidth, 0);
  screen.dw = 7680;
  screen.dh = 4320;
  show.fit(screen, soft, f);
  CHECK_EQ(f.renderWidth, 4096);
  CHECK_EQ(f.renderHeight, 2304);
}
