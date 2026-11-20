// Host (Linux) timing of the fixed 3D test scene at the two Xbox resolutions. Links the host build of ote_core; see docs/XBOX_OG.md "Diagnostic program".
#include <chrono>
#include <cstdio>

#include "bench3d.h"

int main() {
  const int modes[2][2] = {{640, 480}, {1280, 720}};
  for (auto& m : modes) {
    int tris = 0;
    unsigned sum = 0;
    ote::bench3dRun(m[0], m[1], 3, tris, sum);  // warm up
    const int frames = 60;
    auto t0 = std::chrono::steady_clock::now();
    ote::bench3dRun(m[0], m[1], frames, tris, sum);
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / frames;
    std::printf("%dx%d: %.2f ms/frame, %d triangles drawn, checksum %u\n", m[0], m[1], ms, tris, sum);
  }
}
