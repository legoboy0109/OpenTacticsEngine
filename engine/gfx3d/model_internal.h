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

// One glTF primitive as engine geometry (not yet split to the caps). skip = true (a warning is logged): a
// primitive that is not made of triangles or is empty.
bool readPrimitive(const Context& c, const Json& prim, MeshData& out, bool& skip, std::string& err);

// Fills out.materials (same indices as the document's) and out.images (only pictures some material uses).
bool buildMaterials(Context& c, Model3D& out, std::string& err);

// Splits a mesh into meshes of at most maxVertices vertices (re-indexed, triangles kept in order).
void splitMesh(const MeshData& in, int maxVertices, std::vector<MeshData>& out);
// Resizes to power-of-two sides no larger than maxSide (nearest at or below the current size).
void fitImage(ModelImage& img, int maxSide);

}  // namespace gltf
}  // namespace ote
