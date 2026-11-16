// Measures shared by the 3D isometric map's drawing files, in squares.
#pragma once

namespace ote {

const float kWall = 0.45f;             // height of a placeholder structure's box
const float kUnitModelHeight = 1.0f;   // a unit model without a "height" stands this tall
const float kStructureModelFit = 0.92f; // a structure model without a "height" fills this share of its footprint's shorter side

const float kStandBack = 0.3f;         // a standing picture of terrain stands this far behind its cell's middle (towards the back, from the camera)
const float kModelAheadMax = 0.62f;    // a unit on a cell with a standing terrain model is moved towards the camera, at most this far (a little past the cell's front edge)

}  // namespace ote
