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
| OpenGL (`gl/gl_renderer3d.h`) | PC (SDL3, SDL2) and web; phones when there is a phone port | built |
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
  testing); the web makes a WebGL 2 context on a canvas that is never shown.
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
  their screenshots do not depend on the machine's driver. A changed choice
  takes effect at once: `PlayerApp::applyLook` drops the backend (no scene
  holds 3D things while looks are applied) and the next use makes the new
  one. A further graphics API would be one more value here. The
  log says which one draws (`3D drawing: OpenGL (...)` / `3D drawing:
  software`).
- **Checked by** `tests/render3d_backends_test.py` (windowless, both
  backends, five scenes, at most 0.2% of pixels may differ: triangle edges
  are filled by slightly different rules) and the webtest suite `render3d`
  (WebGL 2 against software in Chromium and Firefox).
- Measured on a desktop (windowless, 480 x 270, whole frame including 2D):
  test scene 4.3 ms software, 0.8 ms OpenGL; demo-srpg battle with models
  2.4 ms and 1.2 ms.
- Not handled yet: a lost context (a browser or phone taking the GL context
  away) leaves the 3D picture empty until the page is reloaded.

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
