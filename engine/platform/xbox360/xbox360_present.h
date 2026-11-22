// Xbox 360 picture output (docs/XBOX360.md). SDL-xenon has no video driver, so the SDL3 backend draws into SDL's dummy
// window surface with its software renderer, as it does headless; each frame that surface is uploaded as one texture
// and drawn as a full-screen quad through xsdk's GPU layer. Only built for the 360 (OTE_XBOX360).
#pragma once

namespace ote {

// Takes the GPU and the display (720p, 2 buffers, one flip per vblank). Returns the frame size, false when a step failed
// (the reason is in the log).
bool xbox360PresentOpen(int& width, int& height);

// Shows one finished frame: `pixels` is width x height 32-bit XRGB8888 values (as the CPU holds them), `pitchBytes` apart.
// Waits for a free front buffer, so it also paces the loop to the display. False when the display stopped answering.
bool xbox360PresentFrame(const void* pixels, int pitchBytes);

// For emulators without a display controller (Xenia): after xbox360PresentOpen failed at the display step, draws a test
// pattern through the same texture / shader / quad path into plain GPU memory and compares what comes back. Logs PASS or FAIL.
bool xbox360PresentSelfTest();

void xbox360PresentClose();

}  // namespace ote
