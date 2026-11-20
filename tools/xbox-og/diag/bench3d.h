// A fixed 3D test scene for timing the software rasterizer (engine/gfx3d/soft3d.cpp): a 24x16 block map (one mesh,
// 3840 triangles, a lit, textured, fogged top and sides) and 16 "units" (stacked boxes, 6 x 10 triangles each), seen
// from a tilted perspective camera like the isometric map view. Used by the host benchmark (bench3d_host.cpp) and by
// the Xbox test program, so both time the same work.
#pragma once

namespace ote {

// Draws `frames` frames at width x height (the soft renderer's "renderWidth x renderHeight"); returns the number of
// triangles the last frame put on the picture, through `triangles`. Time it from outside.
// `checksum` gets a few picture bytes added up so the compiler cannot drop the work.
void bench3dRun(int width, int height, int frames, int& triangles, unsigned& checksum);

}  // namespace ote
