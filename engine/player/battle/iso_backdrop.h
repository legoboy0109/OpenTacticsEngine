// A map's prerendered backdrop in the 2D isometric view (docs/VIEWS.md "Backdrop"): the picture drawn in place of the generated
// squares, and its over picture drawn over the units, whole or as pieces that sort with the squares (iso_draw.cpp decides when).
#pragma once
#include "battle/battle_draw.h"
#include "core/data/map_backdrop.h"

namespace ote {

// The backdrop to draw now, or null: the map has one, the style in use is of the 2D isometric kind and listed (or none are), and
// the picture's sheet has a texture. (The over picture failing alone just leaves the over-layer out.)
const MapBackdrop* isoBackdrop(const DrawCtx& c);

// Where the pictures' top-left corner is on the screen (camera at camX, camY): the picture's `origin` pixel is put on the top
// corner of square (0, 0) at the map file's height 0.
void isoBackdropCorner(const DrawCtx& c, const MapBackdrop& bd, int camX, int camY, int& x, int& y);

// The whole picture, at one picture pixel per logical pixel of the view.
void drawIsoBackdrop(Renderer& r, const DrawCtx& c, const MapBackdrop& bd, int camX, int camY);
// The over picture: the whole of it (bd.pieces empty), or one piece. `mod` dims it (fog).
void drawIsoOverWhole(Renderer& r, const DrawCtx& c, const MapBackdrop& bd, int camX, int camY);
void drawIsoOverPiece(Renderer& r, const DrawCtx& c, const MapBackdrop& bd, const BackdropPiece& p, int camX, int camY, Color mod);

// True when a piece drawn after diagonal `key` (its square's x + y is larger) overlaps the screen rectangle: what is drawn there
// is (partly) hidden by it and is shown through.
bool isoOverCovers(const DrawCtx& c, const MapBackdrop& bd, int key, int camX, int camY, int x, int y, int w, int h);

}  // namespace ote
