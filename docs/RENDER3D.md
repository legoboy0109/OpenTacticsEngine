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
  - texture sampling: hard pixels or smooth, repeat or clamp;
  - the depth test can be switched off per material (things shown through
    walls).
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
