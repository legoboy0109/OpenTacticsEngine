// A 3D model as the engine holds it after loading a glTF file
// (docs/RENDER3D.md "Models"). Plain data: nothing here talks to a renderer.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "gfx3d/render3d.h"

namespace ote {

struct ModelImage {
  int width = 0, height = 0;
  std::vector<uint8_t> rgba;  // width * height * 4, top row first; sides are powers of two within the caps
};

struct ModelMaterial {
  std::string name;
  Rgba3D color;          // glTF baseColorFactor
  int image = -1;        // index into Model3D::images, -1 = none
  Blend3D blend = Blend3D::Opaque;
  float cutoff = 0.5f;
  bool twoSided = false;
  bool repeat = true;    // texture wrap
  bool smooth = true;    // texture filter (false = hard pixels)
  bool lit = true;       // false for KHR_materials_unlit
};

// One mesh with one material (a glTF primitive). Never more vertices than the caps allow.
struct ModelPart {
  MeshData mesh;
  int material = -1;     // index into Model3D::materials, -1 = plain white
};

struct ModelNode {
  std::string name;
  int parent = -1;       // index into Model3D::nodes; parents come before children
  Vec3 translation;
  Quat rotation;
  Vec3 scale = Vec3(1, 1, 1);
  std::vector<int> parts;  // indices into Model3D::parts drawn with this node's transform
};

struct Model3D {
  std::vector<ModelNode> nodes;
  std::vector<ModelPart> parts;
  std::vector<ModelMaterial> materials;
  std::vector<ModelImage> images;
  Vec3 boundsMin, boundsMax;  // box around the whole model in its rest pose, model space

  // Model-space transform of a node in the rest pose.
  Mat4 nodeMatrix(int node) const {
    Mat4 m;
    for (int n = node; n >= 0; n = nodes[(size_t)n].parent)
      m = Mat4::trs(nodes[(size_t)n].translation, nodes[(size_t)n].rotation, nodes[(size_t)n].scale) * m;
    return m;
  }
};

// Turns PNG/JPEG bytes into pixels. gfx3d does not decode images itself (it
// must not depend on the platform layer): the caller passes a decoder, e.g.
// one wrapping platform/common/image.h's decodeImage. False with a reason in err.
typedef bool (*ModelImageDecoder)(const uint8_t* data, size_t size, ModelImage& out, std::string& err);

// Loads a .glb or .gltf file (the nodes of its default scene; skins and
// animations are ignored, the model is in its rest pose). False with a
// plain-English reason in `err` (which does not repeat the path). Content is
// fitted to `caps`: large textures are shrunk, large meshes split. See
// model_gltf.cpp for what is and is not supported.
bool loadModel(const std::string& path, const Caps3D& caps, ModelImageDecoder decode, Model3D& out, std::string& err);

// The same from bytes already in memory (.glb or .gltf text). `baseDir` is the
// folder that the file's external .bin / image files are relative to.
bool loadModelBytes(const uint8_t* data, size_t size, const std::string& baseDir, const Caps3D& caps,
                    ModelImageDecoder decode, Model3D& out, std::string& err);

}  // namespace ote
