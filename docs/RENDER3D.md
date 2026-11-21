# The 3D drawing layer

Contract between the engine and whatever draws 3D on a platform. It exists
so the 3D kinds of view (docs/VIEWS.md: 3D isometric, full 3D) run on every
target, including homebrew console ports.

## Rules

- The engine draws 3D **only** through `Renderer3D`
  (`engine/gfx3d/render3d.h`). Nothing outside a backend names a graphics
  API. Game data never contains shader code or API names.
- The feature set is that of fixed-function hardware, because the weakest
  targets have nothing else:
  - triangles with one texture, a vertex colour and a material colour;
  - lighting per vertex from one sun and an ambient colour
    (`colour x (ambient + sun x max(0, normal . towards-sun))`), or unlit;
  - depth test; back faces left out unless the material is two-sided;
  - alpha: opaque, cut-out (a threshold) or blended. Blended things are
    drawn at `end()`, far to near by the mesh's origin, without writing
    depth;
  - linear fog by distance from the camera;
  - texture sampling: hard pixels or smooth (with mipmaps if the texture
    was made with them), repeat or clamp;
  - the depth test can be switched off per material (things shown through
    walls).
- Moving characters are posed on the CPU and uploaded again
  (`updateMesh`). No GPU skinning.
- Limits a backend states in `Caps3D` (defaults: textures power-of-two up
  to 1024 a side, 65535 vertices per mesh; the software backend says 2048,
  the OpenGL one the driver's limit, at most 4096). Loaders shrink and split
  content to fit; a backend refuses (returns 0, logs) what is over.
- Conventions (`math3d.h`): right-handed, +Y up, cameras look down -Z,
  column-major matrices, radians, front faces counter-clockwise, texture
  (0,0) = top-left of the picture. Same as glTF, so models need no
  conversion.
- One 3D picture per `begin`/`end`, in a rectangle of the 2D screen given
  in logical pixels. 2D drawing done afterwards lies on top (menus, text).
  The picture itself may have more pixels than the rectangle has logical
  ones (`Frame3D::renderWidth/Height`): see "Resolution".

## Resolution

The 3D picture is drawn at the **display's real resolution**, so detailed art
is not shrunk to the game's 480 x 270 and scaled up again. Everything that
is about places stays in logical pixels (`MapProj`, picking, the letters and
bars drawn on top); only the picture's pixel count changes, the camera is
the same.

- **Asking the display.** `Renderer::displaySize()` (platform.h) is the real
  pixel size of the rectangle the logical screen is shown in (the
  letterboxed area, not the window; on the web the canvas's real pixels,
  devicePixelRatio included). It follows window resizes, so the picture size
  does too (a new framebuffer / texture when it changes).
- **Fitting.** `Show3D::fit()` (`engine/player/show3d.cpp`) turns that into
  `Frame3D::renderWidth/Height`: the logical rectangle times the display's
  pixels per logical pixel, never below the logical size, at most **4096 on
  the longer side**. `Map3D` and the `--test-3d` scene use it; the backends
  clamp again to what they can (OpenGL: its framebuffer limit).
- **Showing.** `Show3D::show()` draws the picture into the logical rectangle
  through a 2D texture. A picture bigger than its rectangle is drawn with
  linear scaling (SDL3: `SDL_SCALEMODE_LINEAR`; SDL2: the scale-quality hint
  while the texture is made), which on the display is about 1:1, so it
  arrives at full detail. A picture at the logical size is still scaled with
  hard pixels, as before.
- **Choosing**: the "3D resolution" option (Automatic / Display / Game size;
  key `res3d` = `"display"` / `"game"`, global or per game), or
  `ote-player --3d-res auto|display|game`, which wins. Automatic is the
  display's resolution where the backend says it is fast enough
  (`Caps3D::fullResolution`: the OpenGL one) and the game's size otherwise
  (the software backend, which is too slow for more; "Display" forces it).
- **Headless** runs have no display: it is the logical size, unless
  `--3d-scale N` (1..8) says the "display" is N times bigger. The headless
  window is then N times the logical size, so the 3D picture is N times the
  logical one (Automatic becomes Display there) and **the screenshot is
  written at that size too** (2D things in it are scaled up, the 3D picture
  is the real detail). That is how a high-resolution result is checked
  without a window:
  `ote-player --headless --game demo-srpg --map bridge --style full-3d --no-settings --no-saving --3d-scale 4 --screenshot out.bmp`
  (software at 1920 x 1080 takes ~190 ms a frame).
- **Edges.** At display resolution the OpenGL backend draws with 4x
  multisampling (`Frame3D::samples`; ES 3.0 / WebGL 2 multisampled
  renderbuffers resolved by a blit) when the context supports it, else
  silently without. Texture positions are interpolated at the centroid, or
  the edge samples of a tile read the transparent padding of its sheet.
- **Cost.** The read-back into memory is unchanged and grows with the
  picture: 8 MB a frame at 1920 x 1080 (RGBA), 64 MB at 4096 x 4096, then
  one upload into the 2D renderer's texture. Measured on the desktop
  (AMD Radeon, Mesa), full 3D at 1920 x 1080 with 4x multisampling: the
  frame's GPU work plus the read-back wait 2.4-3.1 ms (`glReadPixels` alone
  is a fraction of that). The headless screenshot run takes ~55 ms a frame
  because its 2D side is SDL's software renderer scaling the picture; the
  software 3D backend takes ~190 ms a frame at that size. A weak or
  shared-memory GPU, or the web (the copy out of WebGL), costs more.
  Drawing 2D and 3D in one context (no read-back) is the way to remove
  this cost; not done.

## Backends

| Backend | Where | State |
|---|---|---|
| Software (`soft3d.h`) | everywhere: SDL3, SDL2, web, headless tests | built |
| OpenGL (`gl/gl_renderer3d.h`) | PC (SDL3, SDL2), web, Android | built |
| Direct3D 11 (`d3d11/d3d11_renderer3d.h`) | Windows desktop (SDL3); UWP / Xbox (SDL2) | built; run on WARP only (no GPU, no console) |
| NV2A through pbkit (`platform/nxdk/hw3d_*`) | original Xbox (nxdk) | built; run in xemu only |
| Console SDKs | homebrew ports | with each port |

The software backend draws into an RGBA picture (`framePixels`); the player
shows it through one 2D texture (`engine/player/show3d.h`,
`Renderer::updateTexture`). It is the reference: tests check its pixels
(`tests/test_soft3d.cpp`), and a new backend is right when its pictures are
the software backend's. At the logical resolution (480 x 270) the test scene
takes about 4 ms a frame on a desktop; it is meant for small maps, tests and
platforms without anything better, not for big scenes on weak hardware.

### OpenGL backend

One backend for OpenGL ES 3.0, WebGL 2 and desktop OpenGL 3.3 core (the
shaders differ in their first line only). It keeps the contract above: the
shaders do exactly the software backend's arithmetic (lighting and fog per
vertex), and no shader or API name appears outside `engine/gfx3d/gl/`.

- **Its own context, its own picture.** The platform gives it a GL context
  nothing else uses (`GlHost`, `gl/gl_api.h`). It draws into a framebuffer
  there and reads the finished picture back into memory, so it hands the
  picture over through `framePixels` exactly like the software backend. The
  2D renderer is untouched and need not be GL at all, and the two backends
  can be swapped at will. The read-back is small at the logical resolution
  and grows with the picture (see "Resolution"). (Drawing 2D and 3D in one
  context would save it; that is worth doing if the display-resolution
  read-back ever proves too slow.)
- **Draws are queued** and carried out in `end()`, inside one
  `enter()`/`leave()` of the host, which puts back whatever context was
  current (SDL's own 2D renderer is often GL too). A solid draw whose mesh
  is updated or freed before `end()` is carried out first.
- **No GL headers or libraries** in the engine: `gl_api.h` lists the ~60
  functions used, with the same meaning in all three GL kinds, and the
  platform fills the table by name.
- **Where the context comes from** (`createHardwareRenderer3D`,
  `engine/platform/platform3d.h`): SDL3 and SDL2 make a hidden 16 x 16
  window with an OpenGL ES 3.0 context, or OpenGL 3.3 core when the driver
  has no ES (`OTE_3D_GL=core` in the environment forces that path for
  testing); the web makes a WebGL 2 context on a canvas that is never shown;
  Android makes an EGL pbuffer context with no window, and keeps a CPU copy of
  the meshes and textures so a lost context can be rebuilt (docs/ANDROID.md "3D").
  The platform frees the context when it shuts down; a backend that lives
  longer draws nothing.
- **Falling back.** At start the backend compiles its shaders and reads back
  a test picture; if anything fails the player logs why and uses the
  software backend. `Caps3D` are the software backend's, so the same content
  loads either way.
- **Choosing**: the player's "3D graphics" option (Automatic / OpenGL /
  Software; settings key `draw3d` = `"opengl"` / `"software"`, global or per
  game; `options/option_defs.cpp`; "3D resolution" next to it, see
  "Resolution"), or `ote-player --3d
  auto|software|hardware`, which wins over the option. Automatic is
  hardware, except in `--headless` runs, which keep the software backend so
  their screenshots do not depend on the machine's driver. "Hardware" (the
  option's "OpenGL") is Direct3D 11 first on Windows, then OpenGL (UWP: Direct3D 11 only)
  (`createHardwareRenderer3D`, `platform/sdl3/sdl_gl3d.cpp`). A changed choice
  takes effect at once: `PlayerApp::applyLook` drops the backend (no scene
  holds 3D things while looks are applied) and the next use makes the new
  one. The
  log says which one draws (`3D drawing: OpenGL (...)` / `3D drawing:
  Direct3D 11 (...)` / `3D drawing: software`).
- **Checked by** `tests/render3d_backends_test.py` (windowless, both
  backends, five scenes, at most 0.2% of pixels may differ: triangle edges
  are filled by slightly different rules) and the webtest suite `render3d`
  (WebGL 2 against software in Chromium and Firefox).
- Measured on a desktop (windowless, 480 x 270, whole frame including 2D):
  test scene 4.3 ms software, 0.8 ms OpenGL; demo-srpg battle with models
  2.4 ms and 1.2 ms.
- Not handled yet on the desktop and the web: a lost context (a browser
  taking the GL context away) leaves the 3D picture empty until the page is
  reloaded. Android rebuilds it (see "Losing the device").

### Direct3D 11 backend

Windows' own API, so the 3D picture costs no second device, no read-back and no
OpenGL driver (Windows PCs without a vendor driver, and Xbox, have none). One
backend for the desktop (SDL3) and UWP / Xbox (SDL2): `engine/gfx3d/d3d11/`,
compiled on Windows only (`#ifdef _WIN32` in those files and in
`engine/platform` only; CMake adds them and links `d3d11` and `dxgi` when
`WIN32`). It keeps the contract above and does the GL backend's arithmetic.

- **Feature level 10_0 is enough** (an Xbox One gives UWP apps 10_1, Series
  11_0): Shader Model 4.0, no compute, no tessellation, nothing of 11_0.
  16-bit indices as in the other backends (`Caps3D::maxMeshVertices` 65535).
  `Caps3D`: textures up to 4096 a side (every level from 10_0 has 8192),
  `fullResolution` true, except on Microsoft's software rasteriser (WARP,
  "Microsoft Basic Render Driver": a PC or VM without a graphics driver),
  which is correct but too slow for the display's resolution, so Automatic
  keeps the game's size there.
- **Where the device comes from** (`D3D11Host`, `d3d11/d3d11_host.h`, like
  `GlHost`): when SDL draws the 2D layer with its own Direct3D 11 renderer
  (asked for by name on Windows, `sdl_app.cpp`; the `SDL_RENDER_DRIVER` hint
  still wins), the backend uses SDL's device
  (`SDL_PROP_RENDERER_D3D11_DEVICE_POINTER`, `platform/sdl3/sdl_d3d11_3d.cpp`).
  Otherwise (windowless runs: SDL's software renderer; or SDL chose another
  driver) it makes a device of its own (`d3d11/d3d11_device.h`) and reads the
  picture back into memory (`framePixels`), exactly as the OpenGL backend does.
  If neither works the player tries OpenGL, then software, with a log line each.
- **SDL2 / UWP** (`platform/sdl2/sdl2_d3d11_3d.cpp`, same shape as the SDL3 glue). SDL2 asks for its `direct3d11`
  renderer by name on Windows (on UWP it is the only hardware one; batching stays on), and the backend uses its device
  (`SDL_RenderGetD3D11Device`; SDL 2.32 builds that function only for desktop Windows, so `tools/uwp/build.ps1` widens
  one `#if` in SDL's source and adds the declaration for WinRT, then checks the DLL exports it). SDL2 shows no swap
  chain, so the picture goes onto **the render target SDL has bound** after `SDL_RenderFlush`, mapped through **SDL's
  bound viewport** (the letterbox, in target pixels) and `SDL_RenderGetScale`. SDL2's Direct3D 11 renderer binds its
  target and viewport only when it draws, and a present unbinds the target (flip model), so before flushing the glue
  queues one point of alpha 0 with blending (no pixel changes) to make SDL bind both. Order: Direct3D 11, then software
  on UWP (no OpenGL there); Direct3D 11, OpenGL, software in an SDL2 desktop build (not built on Windows so far). The
  glue reads its debug switches with the C runtime's `getenv`: SDL2's WinRT `SDL_getenv` has an environment of its own.
- **Drawn onto SDL's back buffer, through a picture of its own.** The frame is
  drawn into the backend's own colour target (4x multisampled when
  `Frame3D::samples` asks and the device can) with a 24-bit depth buffer, at
  `renderWidth x renderHeight`, and then put on SDL's back buffer
  (`IDXGISwapChain::GetBuffer(0)`, a render target view made and released in
  the same frame, so SDL can still resize its swap chain) as **one textured
  quad** at the frame's rectangle mapped through SDL's logical presentation
  (`SDL_GetRenderLogicalPresentationRect`): entirely on the GPU, under the 2D
  things drawn afterwards; `framePixels` is null. Not drawn straight into the
  back buffer, because SDL's back buffer has no depth buffer and no
  multisampling, and its size is the window's, not the picture's; the quad
  also gives the picture exactly the treatment `Show3D::show` gives the other
  backends' pictures: blended by its alpha with SDL's "blend" factors (a clear
  colour of alpha 0 leaves the 2D backdrop showing), smooth scaling when the
  picture has more pixels than its rectangle, hard pixels otherwise.
- **Sharing the context with SDL** (`D3D11StateBackup`,
  `d3d11/d3d11_state.h`). SDL caches some pipeline state (render target,
  viewport, scissor, rasterizer, blend, shaders and their resources and
  samplers) and sets other parts once when it makes its device (input
  layout, vertex shader, its vertex constant buffer). Around every use of the
  context the host first lets SDL carry out its queued 2D drawing
  (`SDL_FlushRenderer`, which in SDL3 also forgets SDL's cached state) and
  saves everything the backend touches (slot 0 of each stage, one render
  target, viewports, scissors, the three state objects, topology, buffers),
  and puts all of it back after. SDL2's `SDL_RenderFlush` does not forget the
  cache, so exact restoring is the rule that holds for both. Checked by
  drawing 2D after 3D in every test picture (the HUD over the battle, the
  test scene's text) after hundreds of frames, and by a unit test that sets
  state, draws a frame and reads every piece back.
- **Conventions.** The engine's projections give OpenGL's clip-space depth
  (-w..w); the vertex shader turns it into Direct3D's 0..w (`z = (z + w) / 2`:
  the same near and far planes, the same depth precision). Direct3D's first
  row is the picture's top row, so nothing is drawn upside down (the GL
  backend draws upside down so its read-back starts at the top) and front
  faces stay counter-clockwise (`FrontCounterClockwise`). Pixel centres are at
  .5 in both APIs (Direct3D 9's half-pixel offset is gone since 10). Textures
  and targets are `R8G8B8A8_UNORM`, no sRGB anywhere, as in the GL backend
  (`GL_RGBA8`); vertex colours are `R8G8B8A8_UNORM` too. Matrices go to the
  shaders as the engine has them (column-major, `mul(M, v)`).
- **Textures**: without mipmaps immutable; with mipmaps the GPU makes the
  smaller copies (`GenerateMips`, a box filter like `glGenerateMipmap`). One
  sampler per way of reading, as in the GL backend: hard pixels never read the
  smaller copies (`MaxLOD` 0), smooth ones trilinear. Texture positions are
  interpolated at the centroid (multisampling; see "Edges").
- **Shaders**: one small fixed set, `d3d11/shaders/render3d.hlsl` (the 3D
  drawing, and the screen quad), compiled ahead of time to Shader Model 4.0
  bytecode by `tools/windows/make_d3d11_shaders.ps1` (`fxc` from the Windows
  SDK, `/O3`, reflection and debug data stripped) into
  `d3d11/shaders/*.h`. The generated headers are committed: building needs no
  shader compiler, and nothing compiles shaders at run time (`d3dcompiler` is
  not on every device, Xbox included). Change the HLSL, run the script on
  Windows (`powershell -ExecutionPolicy Bypass -File
  tools\windows\make_d3d11_shaders.ps1`), commit both. No shader exists
  anywhere else.
- **Draws are queued** and carried out in `end()`, inside one host
  `enter()`/`leave()`, as in the GL backend (a solid draw whose mesh or
  texture is updated or freed before `end()` is carried out first). Meshes
  updated with `updateMesh` are refilled in place when they fit
  (`UpdateSubresource`), else made anew.
- **Debug switches** (environment, Windows): `OTE_3D_API=opengl` skips
  Direct3D 11; `OTE_3D_D3D11_OWN=1` uses a device of its own even when SDL's
  could be shared; `OTE_3D_D3D11_LEVEL=10_1` (or `10_0`, `11_0`) caps the
  feature level and `OTE_3D_D3D11_DRIVER=warp` picks the software rasteriser,
  both for an own device (SDL makes its own); `OTE_WINDOW_HIDDEN=1` (SDL3,
  for tests) makes the real window and renderer but never shows the window.
  On UWP the same switches go into `LocalState\args.txt` as `NAME=value` words
  (`OTE_3D_API` is ignored there: no OpenGL).
- **Checked by** `ote_tests` on Windows (`tests/windows/`: ten feature scenes
  against the software backend at the best feature level and at 10_1, the
  same pictures at both levels, mipmaps, multisampling, the screen quad at
  two scales, the state put back, device loss), `tests/render3d_backends_test.py`
  (the player's scenes, Direct3D 11 against software) and
  `tests/render3d_d3d11_test.py` (10_1, device loss and the software
  fallback, multisampling, and SDL's real Direct3D 11 renderer in a hidden
  window: 3D under 2D, at two window sizes; that part needs an interactive
  desktop and reports SKIP over ssh, where Windows refuses a swap chain).
  On other systems that ctest entry is reported as skipped. The UWP app has
  no automated test: it was checked by hand in the VM (docs/XBOX.md "3D").
- **Measured** only on WARP (a VM without a GPU): correct pictures; its
  timings say nothing about a graphics chip. Never run on an Xbox.
- **Suspend (UWP).** Windows wants `IDXGIDevice3::Trim` on every Direct3D
  device when a UWP app is suspended. SDL 2.32's WinRT layer trims its
  renderer's device itself (in its suspend task, right after
  `SDL_APP_DIDENTERBACKGROUND`), and that is the device the 3D drawing shares,
  so the glue only logs it (`suspending: the device is SDL's, which SDL trims
  now`). An own device (`OTE_3D_D3D11_OWN=1`) is trimmed by the glue on
  `SDL_APP_WILLENTERBACKGROUND`, after `ClearState` as the documentation asks
  (`suspending: own device trimmed`); the backend sets all its state again
  every frame.

### Original Xbox (NV2A) backend

`Renderer3D` on the first Xbox's graphics chip through nxdk's pbkit, for the port in docs/XBOX_OG.md. Files in
`engine/platform/nxdk/`: `hw3d_gpu.*` (pbkit start, frames, memory, the push-buffer commands; the only place that writes them),
`hw3d_renderer.*` (the `Renderer3D`), `hw3d_texture.*` (swizzling and mipmaps, plain C++), `hw3d_clip.*` (cutting triangles at
the camera, plain C++), `nxdk_present.*` (the 2D layer and how the chip composes it), `nxdk_hw3d.cpp` (the factory). It keeps the
contract and does the other backends' arithmetic; `framePixels()` is null.

- **One vertex program and one pixel stage, not the chip's fixed-function pipeline.** `tools/xbox-og/shaders/hw3d_vp.vsh` is
  NV_vertex_program 1.1 assembly (position with the viewport and divide by w done by hand, which the chip wants; lighting per vertex
  from one sun and the ambient colour; vertex colour x material colour; fog by distance; texture position) and `hw3d_ps.rc` a
  register combiner (texture x colour + fog share). nxdk's own `vp20compiler` / `fp20compiler` turn them into
  `hw3d_vp.inl` / `hw3d_ps.inl` (`tools/xbox-og/shaders/make_shaders.sh NXDK`); the generated files are committed, so a build needs no
  shader tool, nothing is compiled at run time, and no Microsoft file is involved. A program rather than the fixed-function
  pipeline because the arithmetic is then exactly the software backend's (and fixed-function lighting on this chip has a different
  model), and it is the path nxdk's samples show.
- **Composition with the 2D layer** (docs/XBOX_OG.md "3D"): the 2D layer is a surface drawn at the game's own size; when a 3D picture
  begins, what is drawn so far goes to the screen as one rectangle stretched by the chip over the fitted rectangle, the picture is
  drawn into the frame's rectangle mapped through the same fit (safe area, anamorphic) as the 2D canvas, and what the 2D renderer
  draws afterwards is blended over it, stretched the same way. `renderWidth/Height` and `samples` are
  ignored: the picture is drawn at the screen's pixels, without smoothing of edges.
- **Draws**: solid draws go to the push buffer at once, `Blend` ones wait for `end()` and are drawn far to near by the mesh's origin
  without writing depth (as the software backend). Cut-out is the chip's alpha test (`alpha >= ceil(cutoff x 255)`), blend is
  source alpha. Depth is 24 bit, "less"; a material without depth test still writes depth, as the software one. Culling: counter-
  clockwise faces are front.
- **Meshes** are kept as `Vertex3D` is laid out (36 bytes) in write-combined memory the chip reads in place; indices (16 bit) are
  written inline into the push buffer per draw. `updateMesh` rewrites the vertices in place (a memcpy) when the chip is done with them and otherwise
  into a second buffer the mesh gets the first time it needs one (the two take turns; `hw3d::noteUse` / `idleSeq` say when the chip is done
  with a buffer), so the CPU never waits for the chip; a mesh that grows gets half again as much room. A copy stays in ordinary memory for the
  CPU cutting.
- **Textures**: A8R8G8B8, swizzled, power of two up to 1024 a side, nearest or linear, trilinear with the box-filtered mipmaps made
  when asked; repeat or clamp-to-edge. Memory budgets and the half-size rule: docs/XBOX_OG.md "3D". Without a texture a 1x1 white one.
- **Clipping**: a mesh whose bounding box is inside the clip volume (in front of the near plane, within a guard band around the screen
  of up to 8 half-screens, about 1800 pixels) is drawn as is; one wholly outside is skipped; one crossing it has its triangles cut
  against the volume by the CPU (`clipMesh`: whole triangles keep their indices, cut ones become loose triangles in memory made for
  the frame). The cut is kept with the mesh and reused while the matrix and the guard band stay the same (a still camera: the ground
  is cut once, not every frame) and forgotten when the mesh changes.
- **Commands** are written in one block per draw (state, constants, indices) and the chip is told once (`kick()`, hw3d_gpu.cpp); a frame
  ends with `pb_finished()` (queue it for the flip at the next blank); a frame never starts in the buffer that is on the screen
  (`beginFrame` waits for the blank that moves the display on: docs/XBOX_OG.md "Pacing").
- **`Caps3D`**: `maxTextureSize` 1024, `maxMeshVertices` 65535, `fullResolution` true. Over-size content is shrunk by the loaders
  (glTF images, art sheets: `sheets3d.cpp` shrinks a sheet over the size instead of refusing it, for every backend).
- **Checked by** the host unit tests (`tests/test_nxdk_hw3d.cpp`: swizzle addresses, mipmaps, alpha-weighted halving, the cutting) and
  by the scene set in xemu against the software backend (docs/XBOX_OG.md "3D": 0.3 to 2.5% of pixels differ after blurring away
  edge noise, for the test scene and every 3D style of the three demos; "Detailed 3D" differs in 2.0% and does not really fit the memory).
  Never run on a real console.


### Losing the device

A GPU context or device can go away under a running game: Android may take
an EGL context, Direct3D reports `DXGI_ERROR_DEVICE_REMOVED` / `RESET` after a
driver update or reset or a removed GPU, and SDL's Direct3D 11 renderer then
makes itself a new device (`SDL_EVENT_RENDER_DEVICE_RESET`).
`ResilientRenderer3D` (`gfx3d/resilient3d.h`, no graphics API) wraps the
hardware backend on Android (EGL) and on Windows and UWP (Direct3D 11; SDL2 says
`SDL_RENDER_DEVICE_RESET`): it keeps a CPU
copy of every mesh and texture the game made, and when the platform's
`DeviceKeeper` says the device is gone it makes a new backend (Windows: on
SDL's new device, or a new own device), uploads everything again (the game's
ids stay valid) and repeats the frame. If no new backend can be made the
copies go to the software backend for the rest of the session. Log lines:
`3D (Direct3D 11): the Direct3D 11 device was lost (...); rebuilding it`,
`... device rebuilt, N meshes and M textures uploaded again`, or `... cannot
rebuild the device (...); using the software 3D renderer for the rest of the
session`. Direct3D 11 counts as lost when the device reports a removal
reason, or when SDL's renderer has another device than the backend's. Debug:
`OTE_3D_SIMULATE_LOSS=N` loses the device after N frames,
`OTE_3D_SIMULATE_NO_REBUILD=1` refuses the rebuild (Android, Windows desktop and UWP; a
simulated loss on Windows marks the device lost, it cannot really remove
one). The cost is the copies (the meshes and textures once more in memory).
Not covered: SDL's own 2D textures after a real reset (docs/PORTING.md "Life
cycle").

The player makes the backend in `PlayerApp::renderer3d()`; it outlives every
scene, and a scene frees its own meshes and textures when it ends.

## Models

`engine/gfx3d/model.h`: glTF 2.0 (`.glb`, or `.gltf` with its files) loaded
into plain data (`Model3D`: nodes, parts, materials, images) by `loadModel`.
Only what the feature set above can show is read: base colour and its
texture (PNG or JPEG), alpha mode, double-sidedness, unlit, repeat/clamp,
hard or smooth pixels (the sampler's `magFilter` NEAREST = hard pixels;
anything else, or no sampler, = smooth, and the texture gets mipmaps);
other PBR values are ignored. Content is fitted to
the backend's `Caps3D`: pictures shrunk to power-of-two sides, big meshes
split. Compressed files (Draco, meshopt), sparse accessors and files that
point outside their own folder are refused with a plain sentence; lines,
points and morph targets are skipped. Numbers are read byte by byte, so
big-endian consoles load the same files. Damaged files never crash the
loader (`tests/test_model.cpp`, `tests/test_model_anim_damage.cpp`).

Skins and animation are loaded too, and posed on the CPU (the backend stays
a plain mesh drawer):

- `Model3D::skins`: the joint nodes and inverse bind matrices of every skin
  a node uses. A skinned node's parts carry four joints and four weights per
  vertex (`ModelPart::joints` / `weights`; ubyte or ushort joints, float or
  normalised integer weights, normalised to add up to 1; all-zero weights
  mean joint 0). Their vertices are in model space, as glTF has them, so the
  node's own transform is not applied to them (bounds included). A primitive
  without JOINTS_0 / WEIGHTS_0 is drawn rigid with a warning; a fifth and
  later influence (JOINTS_1 / WEIGHTS_1) is ignored with a warning. A joint
  that is not in the default scene, or a joint index past the skin's joints,
  refuses the file.
- `Model3D::clips`: one `ModelClip` per glTF animation (unnamed ones are
  "Animation 1", "Animation 2", ...), with translation, rotation and scale
  tracks. STEP holds, LINEAR blends, CUBICSPLINE is read as LINEAR from the
  key values only (the tangents are dropped). Morph target (`weights`)
  channels and channels of nodes outside the scene are skipped; so is a
  sampler whose key times do not rise, whose counts do not match, that has no
  keys or whose values are not numbers (with a warning). A clip left without
  tracks is dropped.
- `poseModel(model, clip, time, loop, pose)` fills the model-space matrix of
  every node (no allocation beyond resizing `pose`); clip -1 or a missing
  clip gives the rest pose. Time before the first key holds the first value,
  after the last the last; `loop` wraps by the clip's duration.
- `skinPart(model, pose, node, part, out)` writes a skinned part's posed
  vertices (positions and normals, model space) for `Renderer3D::updateMesh`;
  a rigid part, or any bad index, gives the rest mesh (an empty mesh for a
  bad part index).

The loader does not decode pictures itself (the core and gfx3d have no image
code): the caller passes a decoder. The player's is in
`engine/player/model_view.*`, which also uploads a model to the 3D layer and
draws it (`ModelView`).

The test models (`tests/fixtures/models/`, `assets/models/figure.glb`) are
original and written by `tools/art/make_test_model.py`; `bend.glb` (a column
with three joints and three clips) comes from `tools/art/make_bend_model.py`.

## Helpers

`engine/gfx3d/shapes.h`: `addQuad`, `addBlock` (a map block: top and four
sides) for building map meshes.

## Seeing it

`ote-player --test-3d` shows a built-in scene: a block map with water
(blended), standing pictures that face the camera (cut-out), a lit building
and the loaded test model.
Left/Right turn in quarter steps, Up/Down zoom, Confirm switches between the
flat (isometric) and the perspective camera, Cancel quits. On the web:
`index.html?args=--test-3d`. Webtest suite `render3d` runs it in Chromium
and Firefox.
