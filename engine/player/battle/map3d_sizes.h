// Measures shared by the 3D isometric map's drawing files, in squares.
#pragma once

namespace ote {

const float kWall = 0.45f;             // height of a placeholder structure's box
const float kUnitModelHeight = 1.0f;   // a unit model without a "height" stands this tall
const float kStructureModelFit = 0.92f; // a structure model without a "height" fills this share of its footprint's shorter side

// Standing PICTURES (flat, facing the camera) stand in FRONT of the middle of what they stand on, so their lower
// edge sits near the front of the tile: with the lower edge on the middle line the front half of the tile shows
// under the picture and it reads as floating above it (the owner's report, 2026-11-15).
const float kStandAhead = 0.18f;        // a terrain picture: this far in front of its cell's middle (towards the camera)
const float kStandUnitAhead = 0.42f;    // a unit on such a cell stands in front of that picture, still inside the cell
const float kStructureAhead = 0.3f;     // a structure picture: this far in front of its footprint's middle, per square of footprint
const float kModelAheadMax = 0.62f;    // a unit on a cell with a standing terrain model is moved towards the camera, at most this far (a little past the cell's front edge)

}  // namespace ote
