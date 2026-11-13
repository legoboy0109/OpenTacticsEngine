// Info panel (right side) and hint strip (bottom).
#include <cmath>
#include <cstdio>

#include "battle/battle_draw.h"
#include "ui/text.h"

namespace ote {
namespace {

constexpr int kPad = 4, kLine = 10;
const Color kLabel(150, 158, 190), kText(228, 230, 240), kRule(60, 66, 96);

std::string numStr(double v) {
  char buf[24];
  if (std::fabs(v - std::lround(v)) < 1e-9) snprintf(buf, sizeof buf, "%ld", std::lround(v));
  else snprintf(buf, sizeof buf, "%.1f", v);
  return buf;
}

// "Label        value": label left, value right-aligned, label shortened to fit.
void row(Renderer& r, int x, int y, int chars, const std::string& label, const std::string& value,
         Color lc = kLabel, Color vc = kText) {
  int labelChars = std::max(1, chars - (int)value.size() - 1);
  r.drawText(x, y, fitText(label, labelChars).c_str(), lc);
  r.drawText(x + (chars - (int)value.size()) * kGlyph, y, value.c_str(), vc);
}

void rule(Renderer& r, int x, int& y, int w) {
  r.fillRect(x, y + 2, w, 1, kRule);
  y += 6;
}

std::string rangeText(const WeaponDef& w) {
  return w.minRange == w.maxRange ? intStr(w.minRange) : intStr(w.minRange) + "-" + intStr(w.maxRange);
}

void drawCell(Renderer& r, const DrawCtx& c, Cell cell, int x, int& y, int chars) {
  const Battle& b = c.battle;
  const TerrainDef& td = b.terrainAt(cell);
  const CellState& cs = b.cells[cell];
  r.fillRect(x, y, 6, 7, Color::rgb(td.color));
  r.drawRect(x, y, 6, 7, Color(0, 0, 0, 160));
  r.drawText(x + 9, y, fitText(td.name, chars - 1).c_str(), kText);
  y += kLine;
  for (const NamedNumber& p : td.props) {
    row(r, x, y, chars, p.key, numStr(p.value));
    y += kLine;
  }
  if (cs.owner >= 0) {
    row(r, x, y, chars, "Owner", fitText(b.sides[cs.owner].name, chars - 6), kLabel,
        Color::rgb(b.sides[cs.owner].color));
    y += kLine;
  }
  if (td.capturePoints > 0) {
    row(r, x, y, chars, "Capture", intStr(cs.capture) + "/" + intStr(td.capturePoints));
    y += kLine;
  }
}

void drawUnitInfo(Renderer& r, const DrawCtx& c, const Unit& u, int x, int& y, int w, int chars) {
  const Battle& b = c.battle;
  const Database& db = *b.db;
  Color sc = Color::rgb(b.sides[u.side].color);
  r.fillRect(x, y + 1, 3, 7, sc);
  r.drawText(x + 6, y, fitText(u.name, chars - 1).c_str(), kText);
  y += kLine;
  if (b.typeOf(u).name != u.name) {  // unnamed units would just repeat the type
    r.drawText(x, y, fitText(b.typeOf(u).name, chars).c_str(), kLabel);
    y += kLine;
  }
  if (db.rules.progression) {
    row(r, x, y, chars, "Lv " + intStr(u.level), "XP " + intStr(u.xp) + "/" + intStr(db.rules.xpPerLevel));
    y += kLine;
  }
  int hp = u.id < (int)c.view.units.size() ? c.view.units[u.id].hp : u.hp;  // displayed, not final
  int mx = b.maxHp(u);
  row(r, x, y, chars, db.stats[db.rules.statHp].name, intStr(hp) + "/" + intStr(mx));
  y += kLine;
  r.fillRect(x, y, w, 3, Color(0, 0, 0, 200));
  r.fillRect(x, y, mx > 0 ? w * std::min(hp, mx) / mx : 0, 3, hpColor(hp, mx));
  y += 6;
  for (size_t s = 0; s < db.stats.size(); ++s) {
    if ((int)s == db.rules.statHp) continue;
    row(r, x, y, chars, db.stats[s].name, intStr(u.stats[s]));
    y += kLine;
  }
  for (int wi : b.typeOf(u).weapons) {
    row(r, x, y, chars, db.weapons[wi].name, "r" + rangeText(db.weapons[wi]), kText, kLabel);
    y += kLine;
  }
}

}  // namespace

void panelResourceAnchor(const Layout& L, int resourceCount, int& x, int& y) {
  x = L.vw + L.panelW / 2;
  y = 31 + kLine * resourceCount;  // just below the resource rows (see drawPanel)
}

void drawPanel(Renderer& r, const DrawCtx& c, Cell cursor, bool autoplay) {
  const Battle& b = c.battle;
  const Layout& L = c.layout;
  const Database& db = *b.db;
  int px = L.vw, x = px + kPad, w = L.panelW - 2 * kPad, chars = w / kGlyph;
  r.fillRect(px, 0, L.panelW, L.vh, Color(20, 23, 38));
  r.fillRect(px, 0, 1, L.vh, Color(90, 98, 140));

  // Header: whose turn it is.
  const SideState& side = b.sides[b.activeSide];
  Color sc = Color::rgb(side.color);
  r.fillRect(px + 1, 0, L.panelW - 1, 12, sc);
  r.drawText(x, 2, fitText(side.name, chars).c_str(), readableOn(sc));
  int y = 15;
  row(r, x, y, chars, "Turn " + intStr(b.round), (side.ai || autoplay) ? "AI" : "You");
  y += kLine;

  // Resources of the side the player is looking at (economy games only).
  if (!db.resources.empty() && db.rules.economy) {
    const SideState& vs = b.sides[viewSide(b, autoplay)];
    rule(r, x, y, w);
    for (size_t i = 0; i < db.resources.size(); ++i) {
      row(r, x, y, chars, db.resources[i].name, intStr(vs.resources[i]), kLabel, Color(255, 225, 120));
      y += kLine;
    }
  }

  if (b.grid.valid(cursor)) {
    rule(r, x, y, w);
    drawCell(r, c, cursor, x, y, chars);
    int uid = b.unitAt(cursor);
    if (uid >= 0 && b.units[uid].alive && uid < (int)c.view.units.size() && c.view.units[uid].visible) {
      rule(r, x, y, w);
      drawUnitInfo(r, c, b.units[uid], x, y, w, chars);
    }
  }
}

void drawHint(Renderer& r, const Layout& L, const std::string& hint) {
  r.fillRect(0, L.vh, L.W, L.hintH, Color(14, 16, 28));
  r.fillRect(0, L.vh, L.W, 1, Color(90, 98, 140));
  r.drawText(4, L.vh + 2, fitText(hint, L.W / kGlyph - 1).c_str(), Color(200, 205, 230));
}

}  // namespace ote
