// Shared by the glTF loader's files (model_gltf.cpp, model_io.cpp,
// model_mesh.cpp, model_material.cpp, model_fit.cpp). Not for outside use.
#pragma once
#include <string>
#include <vector>

#include "core/base/json.h"
#include "gfx3d/model.h"

namespace ote {
namespace gltf {

struct Buffer {
  const uint8_t* data = nullptr;
  size_t size = 0;
  std::vector<uint8_t> own;  // when the bytes were read from a file / data: URI (data points into it)
};

struct Context {
  Json json;
  std::vector<Buffer> buffers;
  std::string baseDir;
  Caps3D caps;
  ModelImageDecoder decode = nullptr;
};

// A JSON number that is a whole number in 0..1e9 (so that products of two such values cannot overflow).
bool asIndex(const Json& v, size_t& out);

// Loads a data: URI or a file next to the model. err is a plain sentence.
bool loadUri(const std::string& baseDir, const std::string& uri, std::vector<uint8_t>& out, std::string& err);
bool readFileBytes(const std::string& path, std::vector<uint8_t>& out);

// The bytes of a bufferView, range-checked against its buffer. `stride` is the view's byteStride (0 = tight).
bool viewData(const Context& c, size_t view, const uint8_t*& data, size_t& size, size_t& stride, std::string& err);

enum { kByte = 5120, kUByte = 5121, kShort = 5122, kUShort = 5123, kUInt = 5125, kFloat = 5126 };

// A checked view of one accessor: every element 0..count-1 can be read.
struct Accessor {
  const uint8_t* data = nullptr;
  size_t stride = 0, count = 0;
  int comp = 0, n = 0;  // component type, components per element (MAT4 = 16)
  bool norm = false;
};
bool openAccessor(const Context& c, size_t idx, Accessor& a, std::string& err);
// Component k of element i as a float (normalised integers mapped to 0..1 / -1..1). Little-endian on any machine.
float readComp(const Accessor& a, size_t i, int k);
uint32_t readIndex(const Accessor& a, size_t i);  // an unsigned integer accessor's element (first component)

// Four joints and four weights per vertex (ModelPart::joints / weights).
struct SkinWeights {
  std::vector<uint16_t> joints;
  std::vector<float> weights;
};

// One glTF primitive as engine geometry (not yet split to the caps). skip = true (a warning is logged): a
// primitive that is not made of triangles or is empty. With jointCount > 0 (the primitive is drawn by a node with a
// skin of that many joints) `skin` gets its JOINTS_0 / WEIGHTS_0, one entry per output vertex; it stays empty (and a
// warning is logged) when the primitive has none.
bool readPrimitive(const Context& c, const Json& prim, MeshData& out, bool& skip, std::string& err, size_t jointCount = 0,
                   SkinWeights* skin = nullptr);

// Skins (inverse bind matrices; `nodeMap` = glTF node index -> Model3D node index, -1 = not kept) for the glTF skins
// listed in `used` (they become Model3D::skins in that order). False (err set): a joint is not a kept node, or the
// data is damaged.
bool buildSkins(const Context& c, const std::vector<int>& nodeMap, const std::vector<size_t>& used, Model3D& out,
                std::string& err);
// Animations into out.clips. Damaged or unusable pieces are skipped with a warning; never fails.
void buildClips(const Context& c, const std::vector<int>& nodeMap, Model3D& out);

// Fills out.materials (same indices as the document's) and out.images (only pictures some material uses).
bool buildMaterials(Context& c, Model3D& out, std::string& err);

// Splits a mesh into meshes of at most maxVertices vertices (re-indexed, triangles kept in order). With `skin`
// (one entry per vertex of `in`; may be null) the matching pieces of it come out in `skinOut`, one per mesh.
void splitMesh(const MeshData& in, int maxVertices, std::vector<MeshData>& out, const SkinWeights* skin = nullptr,
               std::vector<SkinWeights>* skinOut = nullptr);
// Resizes to power-of-two sides no larger than maxSide (nearest at or below the current size).
void fitImage(ModelImage& img, int maxSide);

}  // namespace gltf
}  // namespace ote
