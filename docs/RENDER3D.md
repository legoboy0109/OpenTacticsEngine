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
  - texture sampling: hard pixels or smooth, repeat or clamp.
- Moving characters are posed on the CPU and uploaded again
  (`updateMesh`). No GPU skinning.
- Limits a backend states in `Caps3D` (defaults: textures power-of-two up
  to 1024 a side, 65535 vertices per mesh). Loaders shrink and split content
  to fit; a backend refuses (returns 0, logs) what is over.
- Conventions (`math3d.h`): right-handed, +Y up, cameras look down -Z,
  column-major matrices, radians, front faces counter-clockwise, texture
  (0,0) = top-left of the picture. Same as glTF, so models need no
  conversion.
- One 3D picture per `begin`/`end`, in a rectangle of the 2D screen given
  in logical pixels. 2D drawing done afterwards lies on top (menus, text).

## Backends

| Backend | Where | State |
|---|---|---|
| Software (`soft3d.h`) | everywhere: SDL3, SDL2, web, headless tests | built |
| OpenGL ES 3 / WebGL 2 | PC, web, phones | next; needs the 2D drawing on the same GL context |
| Console SDKs | homebrew ports | with each port |

The software backend draws into an RGBA picture (`framePixels`); the player
shows it through one 2D texture (`engine/player/show3d.h`,
`Renderer::updateTexture`). It is the reference: tests check its pixels
(`tests/test_soft3d.cpp`), and a new backend is right when the test scene
looks the same. At the logical resolution (480 x 270) the test scene takes
about 4 ms a frame on a desktop in an unoptimised build; it is meant for
small maps, tests and platforms without anything better, not for big scenes
on weak hardware.

The player makes the backend in `PlayerApp::renderer3d()`; it outlives every
scene, and a scene frees its own meshes and textures when it ends.

## Models

`engine/gfx3d/model.h`: glTF 2.0 (`.glb`, or `.gltf` with its files) loaded
into plain data (`Model3D`: nodes, parts, materials, images) by `loadModel`.
Only what the feature set above can show is read: base colour and its
texture (PNG or JPEG), alpha mode, double-sidedness, unlit, repeat/clamp,
hard or smooth pixels; other PBR values are ignored. Content is fitted to
the backend's `Caps3D`: pictures shrunk to power-of-two sides, big meshes
split. Compressed files (Draco, meshopt), sparse accessors and files that
point outside their own folder are refused with a plain sentence; lines,
points and morph targets are skipped. Numbers are read byte by byte, so
big-endian consoles load the same files. Damaged files never crash the
loader (`tests/test_model.cpp`). Skins and animation come as a later step.

The loader does not decode pictures itself (the core and gfx3d have no image
code): the caller passes a decoder. The player's is in
`engine/player/model_view.*`, which also uploads a model to the 3D layer and
draws it (`ModelView`).

The test model (`tests/fixtures/models/`, `assets/models/figure.glb`) is
original and written by `tools/art/make_test_model.py`.

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
