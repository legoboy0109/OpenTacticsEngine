// Info panel (right side) and hint strip (bottom). Row pitch and widths all
// come from the active font (Layout::row, Renderer::textWidth).
#include <cmath>
#include <cstdio>

#include "battle/battle_draw.h"
#include "ui/text.h"

namespace ote {
namespace {

constexpr int kPad = 4;
const Color kLabel(150, 158, 190), kText(228, 230, 240), kRule(60, 66, 96);

std::string numStr(double v) {
  char buf[24];
  if (std::fabs(v - std::lround(v)) < 1e-9) snprintf(buf, sizeof buf, "%ld", std::lround(v));
  else snprintf(buf, sizeof buf, "%.1f", v);
  return buf;
}

// "Label        value": label left, value right-aligned. Both are truncated with
// ".." when they do not fit: the value gets at most 60% of the width, the
// label whatever is left.
void row(Renderer& r, int x, int y, int w, const std::string& label, const std::string& value,
         Color lc = kLabel, Color vc = kText) {
  std::string v = fitText(r, value, w * 3 / 5);
  int vw = textW(r, v);
  int gap = std::max(4, r.lineHeight() / 2);
  r.drawText(x, y, fitText(r, label, w - vw - gap).c_str(), lc);
  r.drawText(x + w - vw, y, v.c_str(), vc);
}

void rule(Renderer& r, int x, int& y, int w) {
  r.fillRect(x, y + 2, w, 1, kRule);
  y += 6;
}

std::string rangeText(const WeaponDef& w) {
  return w.minRange == w.maxRange ? intStr(w.minRange) : intStr(w.minRange) + "-" + intStr(w.maxRange);
}

void drawCell(Renderer& r, const DrawCtx& c, Cell cell, int x, int& y, int w) {
  const Battle& b = c.battle;
  const int rh = c.layout.row, lh = c.layout.lh;
  const TerrainDef& td = b.terrainAt(cell);
  const CellState& cs = b.cells[cell];
  int sw = std::max(6, lh * 3 / 4);
  r.fillRect(x, y, sw, lh - 1, Color::rgb(terrainRgb(c, cs.terrain)));
  if (const Look* art = terrainArt(c, cs.terrain)) drawTerrainArt(r, c, *art, cell, x, y, sw, lh - 1);
  r.drawRect(x, y, sw, lh - 1, Color(0, 0, 0, 160));
  r.drawText(x + sw + 3, y, fitText(r, td.name, w - sw - 3).c_str(), kText);
  y += rh;
  // The structure standing here (name with its own swatch), then the numbers of the
  // square as formulas see them (terrain merged with structure), then owner and capture.
  const int si = b.structureIndexAt(cell);
  if (si >= 0) {
    const StructureState& st = b.structures[(size_t)si];
    const StructureDef& sd = b.defOf(st);
    r.fillRect(x, y, sw, lh - 1, Color::rgb(structureRgb(c, st.type)));
    if (const Look* art = structureArt(c, st)) drawStructureArt(r, c, *art, x, y, sw, lh - 1);
    r.drawRect(x, y, sw, lh - 1, Color(0, 0, 0, 160));
    std::string name = sd.name;
    if (sd.width > 1 || sd.height > 1) name += " " + intStr(sd.width) + "x" + intStr(sd.height);
    r.drawText(x + sw + 3, y, fitText(r, name, w - sw - 3).c_str(), kText);
    y += rh;
  }
  Props scratch;
  for (const NamedNumber& p : b.numbersAt(cell, scratch)) {
    row(r, x, y, w, p.key, numStr(p.value));
    y += rh;
  }
  if (si >= 0) {
    const StructureState& st = b.structures[(size_t)si];
    if (st.owner >= 0) {
      row(r, x, y, w, "Owner", b.sides[st.owner].name, kLabel, Color::rgb(b.sides[st.owner].color));
      y += rh;
    }
    int pts = b.defOf(st).capturePoints;
    if (pts > 0) {
      row(r, x, y, w, "Capture", intStr(st.capture) + "/" + intStr(pts));
      y += rh;
    }
  }
}

void drawUnitInfo(Renderer& r, const DrawCtx& c, const Unit& u, int x, int& y, int w) {
  const Battle& b = c.battle;
  const Database& db = *b.db;
  const int rh = c.layout.row, lh = c.layout.lh;
  Color sc = Color::rgb(b.sides[u.side].color);
  r.fillRect(x, y + 1, 3, lh - 1, sc);
  r.drawText(x + 6, y, fitText(r, u.name, w - 6).c_str(), kText);
  y += rh;
  if (b.typeOf(u).name != u.name) {  // unnamed units would just repeat the type
    r.drawText(x, y, fitText(r, b.typeOf(u).name, w).c_str(), kLabel);
    y += rh;
  }
  if (db.rules.progression) {
    row(r, x, y, w, "Lv " + intStr(u.level), "XP " + intStr(u.xp) + "/" + intStr(db.rules.xpPerLevel));
    y += rh;
  }
  int hp = u.id < (int)c.view.units.size() ? c.view.units[u.id].hp : u.hp;  // displayed, not final
  int mx = b.maxHp(u);
  row(r, x, y, w, db.stats[db.rules.statHp].name, intStr(hp) + "/" + intStr(mx));
  y += rh;
  r.fillRect(x, y, w, 3, Color(0, 0, 0, 200));
  r.fillRect(x, y, mx > 0 ? w * std::min(hp, mx) / mx : 0, 3, hpColor(hp, mx));
  y += 6;
  // Rows that would run past the panel bottom (tall fonts) are dropped, not overdrawn.
  const int bottom = c.layout.vh - lh;
  for (size_t s = 0; s < db.stats.size(); ++s) {
    if ((int)s == db.rules.statHp || y > bottom) continue;
    row(r, x, y, w, db.stats[s].name, intStr(u.stats[s]));
    y += rh;
  }
  for (int wi : b.typeOf(u).weapons) {
    if (y > bottom) break;
    row(r, x, y, w, db.weapons[wi].name, "r" + rangeText(db.weapons[wi]), kText, kLabel);
    y += rh;
  }
}

}  // namespace

void panelResourceAnchor(const Layout& L, int resourceCount, int& x, int& y) {
  x = L.vw + L.panelW / 2;
  // Just below the resource rows (see drawPanel): turn row, rule, resources.
  y = L.panelTop() + L.row + 6 + L.row * resourceCount;
}

void drawPanel(Renderer& r, const DrawCtx& c, Cell cursor, bool autoplay) {
  const Battle& b = c.battle;
  const Layout& L = c.layout;
  const Database& db = *b.db;
  int px = L.vw, x = px + kPad, w = L.panelW - 2 * kPad;
  r.fillRect(px, 0, L.panelW, L.vh, Color(20, 23, 38));
  r.fillRect(px, 0, 1, L.vh, Color(90, 98, 140));

  // Header: whose turn it is.
  const SideState& side = b.sides[b.activeSide];
  Color sc = Color::rgb(side.color);
  r.fillRect(px + 1, 0, L.panelW - 1, L.lh + 4, sc);
  r.drawText(x, 2, fitText(r, side.name, w).c_str(), readableOn(sc));
  int y = L.panelTop();
  row(r, x, y, w, "Turn " + intStr(b.round), (side.ai || autoplay) ? "AI" : "You");
  y += L.row;

  // Resources of the side the player is looking at (economy games only).
  if (!db.resources.empty() && db.rules.economy) {
    const SideState& vs = b.sides[viewSide(b, autoplay)];
    rule(r, x, y, w);
    for (size_t i = 0; i < db.resources.size(); ++i) {
      row(r, x, y, w, db.resources[i].name, intStr(vs.resources[i]), kLabel, Color(255, 225, 120));
      y += L.row;
    }
  }

  if (b.grid.valid(cursor)) {
    rule(r, x, y, w);
    drawCell(r, c, cursor, x, y, w);
    int uid = b.unitAt(cursor);
    if (uid >= 0 && b.units[uid].alive && uid < (int)c.view.units.size() && c.view.units[uid].visible) {
      rule(r, x, y, w);
      drawUnitInfo(r, c, b.units[uid], x, y, w);
    }
  }
}

void drawHint(Renderer& r, const Layout& L, const std::string& hint) {
  r.fillRect(0, L.vh, L.W, L.hintH, Color(14, 16, 28));
  r.fillRect(0, L.vh, L.W, 1, Color(90, 98, 140));
  r.drawText(4, L.vh + 2, fitText(r, hint, L.W - 8).c_str(), Color(200, 205, 230));
}

}  // namespace ote
