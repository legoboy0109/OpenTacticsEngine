#include "ui/dialogue_portrait.h"

#include <algorithm>
#include <cstdint>

namespace ote {

PortraitArt portraitArt(const Database& db, const Sprites* sprites, int styleIdx, const std::string& id) {
  PortraitArt art;
  const GraphicsSet* set = db.graphics.set(styleIdx);
  if (id.empty() || !set || !sprites) return art;
  const Look* look = set->portrait(id);
  if (!look || !sprites->has(*look) || look->cells.empty()) return art;
  const SheetDef& sh = db.graphics.sheets[(size_t)look->sheet];
  art.tex = sprites->texture(look->sheet);
  art.sx = look->cells[0].col * sh.cellW;
  art.sy = look->cells[0].row * sh.cellH;
  art.sw = sh.cellW;
  art.sh = sh.cellH;
  art.faces = look->faces;
  for (const SpriteCell& c : look->talk) art.talk.push_back(std::make_pair(c.col * sh.cellW, c.row * sh.cellH));
  for (const SpriteCell& c : look->blink) art.blink.push_back(std::make_pair(c.col * sh.cellW, c.row * sh.cellH));
  return art;  // (the sheet's "smooth" flag is not honoured: the 2D renderer samples every texture the same way)
}

namespace {
uint32_t mix(uint32_t v) {
  v ^= v >> 16;
  v *= 0x7feb352dU;
  v ^= v >> 15;
  v *= 0x846ca68bU;
  v ^= v >> 16;
  return v;
}
}  // namespace

PortraitArt portraitFrame(const PortraitArt& art, bool talking, double clock, const std::string& key) {
  if (art.talk.empty() && art.blink.empty()) return art;
  PortraitArt out = art;
  if (talking && !art.talk.empty()) {
    const size_t i = (size_t)(clock * 1000.0 / kTalkFrameMs) % art.talk.size();
    out.sx = art.talk[i].first;
    out.sy = art.talk[i].second;
    return out;
  }
  if (!art.blink.empty()) {
    uint32_t h = 2166136261u;
    for (char ch : key) h = (h ^ (uint8_t)ch) * 16777619u;
    // Cycle k lasts 2.8 s + 0..2.6 s (worked out from the key and k); its last 130 ms are the blink.
    const double ms = clock * 1000.0;
    double start = 0;
    for (uint32_t k = 0; k < 100000; ++k) {
      const double len = kBlinkMinMs + (double)(mix(h + k * 40503u) % kBlinkSpreadMs);
      if (ms < start + len) {
        const double into = ms - (start + len - kBlinkMs);
        if (into >= 0) {
          const size_t i = std::min(art.blink.size() - 1, (size_t)(into * art.blink.size() / kBlinkMs));
          out.sx = art.blink[i].first;
          out.sy = art.blink[i].second;
        }
        break;
      }
      start += len;
    }
  }
  return out;
}

}  // namespace ote
