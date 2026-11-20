// The sheet cache behind Sprites (battle/sprites.h; docs/GRAPHICS.md "Art memory"): textures made on first use and dropped
// least recently used first when over the budget. Free of SDL (only the Renderer interface), so the tests run it against a fake.
#include "battle/sprites.h"

#include <chrono>

#include "core/base/log.h"
#include "platform/common/image.h"
#include "platform/platform.h"

namespace ote {

void Sprites::load(Renderer& r, const std::string& gameDir, const Graphics& g) {
  r_ = &r;
  dir_ = gameDir;
  g_ = &g;
  slots_.assign(g.sheets.size(), Slot());
  budget_.reset(g.sheets.size(), (uint64_t)artBudgetMB() * 1024u * 1024u);
  // Sheets the battle scene draws mirrored (docs/BATTLE_SCENE.md): its animation sets and effects.
  auto mirrorIt = [&](int sheet) {
    if (sheet >= 0 && (size_t)sheet < slots_.size()) slots_[(size_t)sheet].mirrorOk = true;
  };
  for (const BattleSet& s : g.battle.sets)
    for (const BattleAnim& a : s.anims) {
      if (!s.mirror) continue;
      mirrorIt(a.sheet);
      for (const auto& ss : a.sideSheets) mirrorIt(ss.second);
    }
  for (const BattleEffect& e : g.battle.effects) mirrorIt(e.anim.sheet);
  // A fixed stage turns the left platform picture round for the right-hand fighter.
  if (g.battle.stage.on)
    for (const BattlePlatform& p : g.battle.platforms) mirrorIt(p.sheet);
  // Walking frames of one side drawn as the other (docs/GRAPHICS.md "Walking").
  for (const GraphicsSet& set : g.styles)
    for (const UnitLook& ul : set.units) {
      auto mark = [&](const Look& l) {
        for (int d = 0; d < kWalkDirs; ++d)
          if (l.walk.mirrored[d]) mirrorIt(l.walk.sheet);
      };
      mark(ul.base);
      for (const auto& s : ul.sides) mark(s.second);
    }
  for (const TilesetDef& ts : g.tilesets)  // the isometric view samples tiles itself (tile_art.cpp)
    if (ts.sheet >= 0 && (size_t)ts.sheet < slots_.size()) slots_[(size_t)ts.sheet].keepPixels = true;
  // Only the headers are read here: the file is there, is a PNG, and is at least one cell big. The pixels follow when drawn.
  for (size_t i = 0; i < g.sheets.size(); ++i) {
    const SheetDef& s = g.sheets[i];
    Slot& sl = slots_[i];
    std::string err;
    if (!readPngFileSize(gameDir + "/" + s.file, sl.w, sl.h, err)) {
      OTE_ERROR("art sheet \"%s\" (%s): %s; what uses it keeps the placeholder look", s.id.c_str(), s.file.c_str(), err.c_str());
      sl.bad = true;
      continue;
    }
    if (sl.w < s.cellW || sl.h < s.cellH) {
      OTE_ERROR("art sheet \"%s\" (%s): the image is %dx%d, smaller than one %dx%d cell; "
                "what uses it keeps the placeholder look",
                s.id.c_str(), s.file.c_str(), sl.w, sl.h, s.cellW, s.cellH);
      sl.bad = true;
      continue;
    }
    sl.cols = s.cols;
    sl.cellW = s.cellW;
  }
}

void Sprites::release(Renderer& r) {
  for (Slot& s : slots_) {
    if (s.tex) r.destroyTexture(s.tex);
    if (s.mir) r.destroyTexture(s.mir);
  }
  slots_.clear();
  budget_.reset(0, 0);
  r_ = nullptr;
  g_ = nullptr;
}

uint64_t Sprites::heldBytes(const Slot& s) const {
  uint64_t b = s.pix.rgba.size();
  if (s.tex) b += (uint64_t)s.w * (uint64_t)s.h * 4;
  if (s.mir) b += (uint64_t)s.cols * (uint64_t)s.cellW * (uint64_t)s.h * 4;
  return b;
}

void Sprites::drop(Slot& s) const {
  if (s.tex) r_->destroyTexture(s.tex);
  if (s.mir) r_->destroyTexture(s.mir);
  s.tex = s.mir = 0;
  s.mirrorTried = false;
  Image().rgba.swap(s.pix.rgba);  // really frees the pixels (clear() would keep the memory)
  s.pix = Image();
}

bool Sprites::make(size_t i, bool mirror) const {
  Slot& s = slots_[i];
  const SheetDef& sh = g_->sheets[i];
  const bool needTex = s.tex == 0;
  const bool needMir = mirror && s.mir == 0 && !s.mirrorTried;
  // The pixels come from the kept copy when there is one, else from the file.
  Image img;
  const Image* src = &s.pix;
  if (s.pix.rgba.empty()) {
    std::string err;
    if (!loadPngFile(dir_ + "/" + sh.file, img, err)) {
      OTE_ERROR("art sheet \"%s\" (%s): %s; what uses it keeps the placeholder look", sh.id.c_str(), sh.file.c_str(), err.c_str());
      if (needTex) s.bad = true;
      s.mirrorTried = true;
      return !needTex;
    }
    if (img.width < sh.cellW || img.height < sh.cellH) {  // (the file changed since the header was read)
      OTE_ERROR("art sheet \"%s\" (%s): the image is %dx%d, smaller than one %dx%d cell; "
                "what uses it keeps the placeholder look",
                sh.id.c_str(), sh.file.c_str(), img.width, img.height, sh.cellW, sh.cellH);
      if (needTex) s.bad = true;
      s.mirrorTried = true;
      return !needTex;
    }
    src = &img;
  }
  s.w = src->width;
  s.h = src->height;
  if (needTex) {
    s.tex = r_->createTexture(src->rgba.data(), src->width, src->height);  // logs its own failure
    if (!s.tex) {
      s.bad = true;
      return false;
    }
  }
  if (needMir) {
    // A copy of the whole cells with every row reversed, for the fighter that faces the other way.
    const int w = sh.cols * sh.cellW, h = src->height;
    std::vector<uint8_t> flipped((size_t)w * (size_t)h * 4);
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x)
        for (int k = 0; k < 4; ++k)
          flipped[((size_t)y * w + (size_t)(w - 1 - x)) * 4 + (size_t)k] = src->rgba[((size_t)y * src->width + (size_t)x) * 4 + (size_t)k];
    s.mir = r_->createTexture(flipped.data(), w, h);
    s.mirrorTried = true;
  }
  if (s.keepPixels && src == &img) s.pix = std::move(img);
  budget_.setBytes(i, heldBytes(s));
  trim(budget_.budget());
  return true;
}

void Sprites::trim(uint64_t limit) const {
  if (!r_) return;
  const std::vector<int> victims = budget_.pickVictims(limit);
  if (victims.empty()) return;
  uint64_t bytes = 0;
  for (int v : victims) {
    Slot& s = slots_[(size_t)v];
    bytes += heldBytes(s);
    drop(s);
  }
  uint64_t count = victims.size();
  static const auto t0 = std::chrono::steady_clock::now();
  const double now = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  if (budget_.noteEvicted(now, 5.0, count, bytes))
    OTE_INFO("art memory: dropped %llu sheet(s), %llu MB, least recently used first (%llu MB held of a budget of %llu MB)",
             (unsigned long long)count, (unsigned long long)(bytes >> 20), (unsigned long long)(budget_.total() >> 20),
             (unsigned long long)(budget_.budget() >> 20));
}

TextureId Sprites::texture(int sheet) const {
  if (sheet < 0 || (size_t)sheet >= slots_.size()) return 0;
  Slot& s = slots_[(size_t)sheet];
  if (s.bad) return 0;
  budget_.use((size_t)sheet);
  if (!s.tex && !make((size_t)sheet, false)) return 0;
  return s.tex;
}

TextureId Sprites::mirrored(int sheet) const {
  if (sheet < 0 || (size_t)sheet >= slots_.size()) return 0;
  Slot& s = slots_[(size_t)sheet];
  if (s.bad || !s.mirrorOk) return 0;
  budget_.use((size_t)sheet);
  if (!s.mir && !s.mirrorTried) make((size_t)sheet, true);
  return s.mir;
}

const Image* Sprites::pixels(int sheet) const {
  if (sheet < 0 || (size_t)sheet >= slots_.size() || !slots_[(size_t)sheet].keepPixels) return nullptr;
  if (!texture(sheet)) return nullptr;
  const Slot& s = slots_[(size_t)sheet];
  return s.pix.rgba.empty() ? nullptr : &s.pix;
}

void Sprites::request(int sheet, bool mirror) const {
  if (mirror) mirrored(sheet);
  texture(sheet);
}

}  // namespace ote
