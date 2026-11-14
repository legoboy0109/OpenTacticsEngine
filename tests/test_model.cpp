// The glTF model loader (engine/gfx3d/model_gltf.cpp and friends), with the fixtures made by
// tools/art/make_test_model.py, plus damaged files.
#include <cmath>
#include <cstring>

#include "core/base/json.h"
#include "gfx3d/model.h"
#include "gfx3d/soft3d.h"
#include "platform/common/image.h"
#include "test.h"

using namespace ote;

namespace {

bool decodeForModel(const uint8_t* data, size_t size, ModelImage& out, std::string& err) {
  Image im;
  if (!decodeImage(data, size, im, err)) return false;
  out.width = im.width;
  out.height = im.height;
  out.rgba.swap(im.rgba);
  return true;
}

// A picture of 5 x 3 pixels whatever the bytes are (for testing the fitting to power-of-two sizes).
bool decodeOdd(const uint8_t*, size_t, ModelImage& out, std::string&) {
  out.width = 5;
  out.height = 3;
  out.rgba.assign(5 * 3 * 4, 200);
  return true;
}

std::string dir() { return ote_test::sourceDir() + "/tests/fixtures/models"; }

bool load(const char* file, const Caps3D& caps, Model3D& m, std::string& err) {
  return loadModel(dir() + "/" + file, caps, decodeForModel, m, err);
}

std::vector<uint8_t> readAll(const char* file) {
  std::string text;
  std::vector<uint8_t> bytes;
  FILE* f = std::fopen((dir() + "/" + file).c_str(), "rb");
  if (!f) return bytes;
  uint8_t buf[4096];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) bytes.insert(bytes.end(), buf, buf + n);
  std::fclose(f);
  return bytes;
}

bool near(float a, float b, float tol = 1e-4f) { return std::fabs(a - b) <= tol; }

size_t triangles(const Model3D& m) {
  size_t n = 0;
  for (const ModelPart& p : m.parts) n += p.mesh.indices.size() / 3;
  return n;
}

// Loads bytes as a .gltf next to the fixtures; true if the loader accepted them.
bool loadText(const std::string& text, Model3D& m, std::string& err) {
  return loadModelBytes((const uint8_t*)text.data(), text.size(), dir(), Caps3D(), decodeForModel, m, err);
}

// figure.gltf with the first occurrence of `from` replaced by `to`.
std::string gltfWith(const std::string& from, const std::string& to) {
  std::vector<uint8_t> b = readAll("figure.gltf");
  std::string s(b.begin(), b.end());
  size_t at = s.find(from);
  CHECK(at != std::string::npos);
  if (at != std::string::npos) s.replace(at, from.size(), to);
  return s;
}

// The loader must refuse with a plain sentence (non-empty, no path in it).
void expectRefused(const std::string& text, const char* mustContain = nullptr) {
  Model3D m;
  std::string err;
  bool ok = loadText(text, m, err);
  CHECK(!ok);
  CHECK(!err.empty());
  CHECK(err.find("/home") == std::string::npos && err.find("tests/fixtures") == std::string::npos);
  if (mustContain) CHECK(err.find(mustContain) != std::string::npos);
  if (ok || err.empty() || (mustContain && err.find(mustContain) == std::string::npos))
    std::printf("    (message was: \"%s\")\n", err.c_str());
  CHECK(m.parts.empty());
}

void checkFigure(const Model3D& m) {
  CHECK_EQ((int)m.nodes.size(), 2);
  CHECK(m.nodes[0].name == "pawn" && m.nodes[0].parent == -1);
  CHECK(m.nodes[1].name == "hat" && m.nodes[1].parent == 0);
  CHECK(near(m.nodes[1].translation.y, 1.55f) && near(m.nodes[1].translation.z, 0.25f));
  CHECK(near(m.nodes[1].rotation.y, std::sin(0.2617994f)) && near(m.nodes[1].rotation.w, std::cos(0.2617994f)));
  CHECK(near(m.nodes[1].scale.x, 1) && near(m.nodes[0].scale.y, 1));

  CHECK_EQ((int)m.parts.size(), 2);
  CHECK_EQ((int)m.nodes[0].parts.size(), 1);
  CHECK_EQ((int)m.nodes[1].parts.size(), 1);
  CHECK_EQ((int)m.parts[0].mesh.vertices.size(), 13 * 17);
  CHECK_EQ((int)m.parts[0].mesh.indices.size() % 3, 0);
  CHECK(m.parts[0].mesh.indices.size() / 3 > 300);   // a few hundred triangles
  CHECK_EQ((int)m.parts[1].mesh.vertices.size(), 36);  // the box has no normals: 12 triangles of their own vertices
  CHECK_EQ((int)m.parts[1].mesh.indices.size(), 36);
  CHECK(m.parts[0].material == 0 && m.parts[1].material == 1);
  // Interleaved data with a stride: the first vertex is the bottom middle point, its normal points down.
  const Vertex3D& v0 = m.parts[0].mesh.vertices[0];
  CHECK(near(v0.x, 0) && near(v0.y, 0) && near(v0.ny, -1));
  // Generated flat normal of the box: unit length.
  const Vertex3D& b0 = m.parts[1].mesh.vertices[0];
  CHECK(near(b0.nx * b0.nx + b0.ny * b0.ny + b0.nz * b0.nz, 1));

  CHECK_EQ((int)m.materials.size(), 2);
  const ModelMaterial &a = m.materials[0], &b = m.materials[1];
  CHECK(a.name == "checker" && a.image == 0 && a.blend == Blend3D::Opaque && a.lit && !a.twoSided);
  CHECK(a.repeat && !a.smooth);  // mirrored wrap counts as repeat; magFilter NEAREST
  CHECK(near(a.color.r, 1) && near(a.color.a, 1));
  CHECK(b.name == "glass" && b.image == -1 && b.blend == Blend3D::Blend && !b.lit && b.twoSided);
  CHECK(near(b.color.r, 0.2f) && near(b.color.g, 0.6f) && near(b.color.a, 0.5f));

  CHECK_EQ((int)m.images.size(), 1);
  const ModelImage& im = m.images[0];
  CHECK(im.width == 8 && im.height == 8 && im.rgba.size() == 8 * 8 * 4);
  auto px = [&](int x, int y, int c) { return (int)im.rgba[((size_t)y * 8 + x) * 4 + c]; };
  CHECK(px(0, 0, 0) == 200 && px(0, 0, 1) == 40 && px(0, 0, 3) == 255);
  CHECK(px(2, 0, 0) == 240 && px(2, 0, 1) == 220 && px(2, 0, 2) == 60);
  CHECK(px(7, 7, 0) == 0 && px(7, 7, 2) == 255);

  // Rest-pose box around everything: the pawn is 0.45 wide; the turned hat reaches 0.25 + 0.2 * (cos 30 + sin 30) forward.
  CHECK(near(m.boundsMin.x, -0.45f, 1e-3f) && near(m.boundsMin.y, 0, 1e-3f) && near(m.boundsMin.z, -0.45f, 1e-3f));
  CHECK(near(m.boundsMax.x, 0.45f, 1e-3f) && near(m.boundsMax.y, 1.75f, 1e-3f));
  CHECK(near(m.boundsMax.z, 0.25f + 0.2f * (std::cos(0.5235988f) + std::sin(0.5235988f)), 1e-3f));
}

}  // namespace

TEST(model_loads_glb) {
  Model3D m;
  std::string err;
  CHECK(load("figure.glb", Caps3D(), m, err));
  CHECK(err.empty());
  checkFigure(m);
}

TEST(model_loads_gltf_with_external_files_the_same_as_glb) {
  Model3D g, t;
  std::string err;
  CHECK(load("figure.glb", Caps3D(), g, err));
  CHECK(load("figure.gltf", Caps3D(), t, err));
  checkFigure(t);
  CHECK_EQ((int)g.parts.size(), (int)t.parts.size());
  for (size_t i = 0; i < g.parts.size(); ++i) {
    CHECK(g.parts[i].mesh.indices == t.parts[i].mesh.indices);
    CHECK_EQ((int)g.parts[i].mesh.vertices.size(), (int)t.parts[i].mesh.vertices.size());
    CHECK(g.parts[i].mesh.vertices.empty() ||
          std::memcmp(g.parts[i].mesh.vertices.data(), t.parts[i].mesh.vertices.data(),
                      g.parts[i].mesh.vertices.size() * sizeof(Vertex3D)) == 0);
  }
  CHECK(g.images[0].rgba == t.images[0].rgba);
  CHECK(near(g.boundsMax.z, t.boundsMax.z, 0));
}

TEST(model_fits_caps_by_splitting_meshes_and_shrinking_textures) {
  Model3D whole, split, small;
  std::string err;
  CHECK(load("figure.glb", Caps3D(), whole, err));
  Caps3D tiny;
  tiny.maxMeshVertices = 30;
  CHECK(load("figure.glb", tiny, split, err));
  CHECK(split.parts.size() > whole.parts.size());
  for (const ModelPart& p : split.parts) {
    CHECK((int)p.mesh.vertices.size() <= 30);
    CHECK(p.material == 0 || p.material == 1);
    for (uint32_t i : p.mesh.indices) CHECK(i < p.mesh.vertices.size());
  }
  CHECK_EQ((int)triangles(split), (int)triangles(whole));
  size_t onHat = 0;
  for (const ModelNode& n : split.nodes) onHat += n.parts.size();
  CHECK_EQ((int)onHat, (int)split.parts.size());  // every piece belongs to a node
  CHECK(near(split.boundsMax.y, whole.boundsMax.y, 1e-5f));

  Caps3D smallTex;
  smallTex.maxTextureSize = 4;
  CHECK(load("figure.glb", smallTex, small, err));
  CHECK(small.images[0].width == 4 && small.images[0].height == 4);
  CHECK(small.images[0].rgba[0] == 200 && small.images[0].rgba[1] == 40);  // a 2 x 2 block of red stays red
  CHECK(small.images[0].rgba[(0 * 4 + 1) * 4 + 0] == 240);                 // the yellow block next to it

  // Pictures that are not a power of two shrink to the power of two at or below: 5 x 3 -> 4 x 2.
  std::vector<uint8_t> glb = readAll("figure.glb");
  Model3D odd;
  CHECK(loadModelBytes(glb.data(), glb.size(), dir(), Caps3D(), decodeOdd, odd, err));
  CHECK(odd.images[0].width == 4 && odd.images[0].height == 2);
  CHECK(odd.images[0].rgba[0] == 200 && odd.images[0].rgba[3] == 200);
}

TEST(model_decomposes_node_matrices) {
  std::vector<uint8_t> b = readAll("figure.gltf");
  Json doc;
  std::string err;
  CHECK(Json::parse(std::string(b.begin(), b.end()), doc, err));
  Mat4 mat = Mat4::trs(Vec3(0, 1.55f, 0.25f), Quat::axisAngle(Vec3(0.3f, 1, 0.2f), 0.9f), Vec3(1, 2, 1.5f));
  Json arr = Json::makeArray();
  for (int i = 0; i < 16; ++i) arr.push(Json::makeNumber(mat.m[i]));
  Json hat = Json::makeObject();
  hat.set("name", Json::makeString("hat"));
  hat.set("mesh", Json::makeNumber(1));
  hat.set("matrix", arr);
  Json nodes = Json::makeArray();
  nodes.push(doc["nodes"][0]);
  nodes.push(hat);
  doc.set("nodes", nodes);
  Model3D m;
  CHECK(loadText(doc.dump(1), m, err));
  CHECK_EQ((int)m.nodes.size(), 2);
  Mat4 back = m.nodeMatrix(1);
  for (int i = 0; i < 16; ++i) CHECK(near(back.m[i], mat.m[i], 1e-4f));
  CHECK(near(m.nodes[1].scale.y, 2, 1e-4f));
}

TEST(model_draws_with_the_software_renderer) {
  Model3D m;
  std::string err;
  CHECK(load("figure.glb", Caps3D(), m, err));
  SoftRenderer3D r;
  std::vector<MeshId> meshes;
  for (const ModelPart& p : m.parts) {
    meshes.push_back(r.createMesh(p.mesh));
    CHECK(meshes.back() != 0);
  }
  Texture3DId tex = r.createTexture(m.images[0].rgba.data(), m.images[0].width, m.images[0].height, true);
  CHECK(tex != 0);
  Frame3D f;
  f.width = f.height = 64;
  f.clear = Rgba3D(0, 0, 0.5f, 1);
  f.view = Mat4::lookAt(Vec3(0.8f, 1.2f, 3.5f), Vec3(0, 0.8f, 0), Vec3(0, 1, 0));
  f.proj = Mat4::perspective(0.8f, 1.0f, 0.5f, 30.0f);
  r.begin(f);
  for (size_t n = 0; n < m.nodes.size(); ++n)
    for (int pi : m.nodes[n].parts) {
      const ModelPart& p = m.parts[(size_t)pi];
      Material3D mat;
      if (p.material >= 0) {
        const ModelMaterial& mm = m.materials[(size_t)p.material];
        mat.texture = mm.image >= 0 ? tex : 0;
        mat.color = mm.color;
        mat.blend = mm.blend;
        mat.cutoff = mm.cutoff;
        mat.lit = mm.lit;
        mat.twoSided = mm.twoSided;
        mat.smooth = mm.smooth;
      }
      r.draw(meshes[(size_t)pi], m.nodeMatrix((int)n), mat);
    }
  r.end();
  CHECK(r.trianglesDrawn() > 100);
  int w, h;
  const uint8_t* px = r.framePixels(w, h);
  CHECK(px != nullptr);
  int covered = 0;
  for (int y = 24; y < 40; ++y)
    for (int x = 24; x < 40; ++x) {
      const uint8_t* q = px + ((size_t)y * w + x) * 4;
      if (!(q[0] == 0 && q[1] == 0 && q[2] == 128)) ++covered;
    }
  CHECK(covered > 80);  // the slim body fills a good part of the middle, it is not the clear colour
  CHECK(px[0] == 0 && px[1] == 0 && px[2] == 128);  // a corner stays clear
}

TEST(decodeImage_reads_jpeg_and_png_and_refuses_other_bytes) {
  std::vector<uint8_t> jpg = readAll("grey200.jpg");
  CHECK(!jpg.empty());
  Image im;
  std::string err;
  CHECK(decodeImage(jpg.data(), jpg.size(), im, err));
  CHECK(im.width == 8 && im.height == 8);
  CHECK(std::abs((int)im.rgba[0] - 200) <= 2 && std::abs((int)im.rgba[(3 * 8 + 3) * 4 + 1] - 200) <= 2 && im.rgba[3] == 255);
  CHECK(!decodePng(jpg.data(), jpg.size(), im, err));  // decodePng stays PNG-only
  std::vector<uint8_t> png = readAll("figure_tex.png");
  CHECK(decodeImage(png.data(), png.size(), im, err));
  CHECK(im.width == 8 && im.rgba[0] == 200);
  const uint8_t junk[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  CHECK(!decodeImage(junk, sizeof junk, im, err));
  CHECK(!err.empty());
  CHECK(!decodeImage(jpg.data(), 20, im, err));  // cut off JPEG
}

TEST(model_refuses_damaged_input_without_crashing) {
  std::vector<uint8_t> glb = readAll("figure.glb");
  CHECK(glb.size() > 1000);
  Model3D m;
  std::string err;
  // Cut at every length (header unchanged: it then claims more than there is).
  for (size_t n = 0; n < glb.size(); ++n) {
    bool ok = loadModelBytes(glb.data(), n, dir(), Caps3D(), decodeForModel, m, err);
    CHECK(!ok);
    CHECK(!err.empty());
    if (ok || err.empty()) {
      std::printf("    (cut at %zu)\n", n);
      break;
    }
  }
  // Cut and with the header length corrected to match: the chunk and buffer checks must catch it.
  for (size_t n = 12; n < glb.size(); ++n) {
    std::vector<uint8_t> cut(glb.begin(), glb.begin() + (long)n);
    cut[8] = (uint8_t)n;
    cut[9] = (uint8_t)(n >> 8);
    cut[10] = (uint8_t)(n >> 16);
    cut[11] = (uint8_t)(n >> 24);
    bool ok = loadModelBytes(cut.data(), cut.size(), dir(), Caps3D(), decodeForModel, m, err);
    CHECK(!ok);
    CHECK(!err.empty());
    if (ok || err.empty()) {
      std::printf("    (cut with fixed header at %zu)\n", n);
      break;
    }
  }
  // Truncated .gltf text (all but the final newline is needed).
  std::vector<uint8_t> txt = readAll("figure.gltf");
  for (size_t n = 0; n + 2 < txt.size(); n += 7) {
    CHECK(!loadModelBytes(txt.data(), n, dir(), Caps3D(), decodeForModel, m, err));
    CHECK(!err.empty());
  }
  // Random damage: any outcome is fine, but never a crash (run under a sanitizer to make this strict).
  uint32_t seed = 12345;
  for (int round = 0; round < 400; ++round) {
    std::vector<uint8_t> bad = glb;
    for (int k = 0; k < 1 + round % 6; ++k) {
      seed = seed * 1664525u + 1013904223u;
      size_t at = (seed >> 8) % bad.size();
      seed = seed * 1664525u + 1013904223u;
      bad[at] = (uint8_t)(seed >> 16);
    }
    loadModelBytes(bad.data(), bad.size(), dir(), Caps3D(), decodeForModel, m, err);
  }
}

TEST(model_refuses_wrong_files_and_bad_references_with_a_message) {
  Model3D m;
  std::string err;
  const uint8_t empty = 0;
  CHECK(!loadModelBytes(&empty, 0, dir(), Caps3D(), decodeForModel, m, err));
  CHECK(!err.empty());
  const char* notGltf = "hello, this is not a model";
  CHECK(!loadModelBytes((const uint8_t*)notGltf, std::strlen(notGltf), dir(), Caps3D(), decodeForModel, m, err));
  CHECK(err.find("not a glTF") != std::string::npos);
  std::vector<uint8_t> glb = readAll("figure.glb");
  glb[0] = 'X';  // wrong magic: neither .glb nor JSON
  CHECK(!loadModelBytes(glb.data(), glb.size(), dir(), Caps3D(), decodeForModel, m, err));
  CHECK(err.find("not a glTF") != std::string::npos);
  CHECK(!load("no_such_model.glb", Caps3D(), m, err));
  CHECK(!err.empty());

  expectRefused(gltfWith("\"indices\": 3", "\"indices\": 99"));
  expectRefused(gltfWith("\"POSITION\": 0", "\"POSITION\": 99"));
  expectRefused(gltfWith("\"count\": 8,", "\"count\": 8000000,"), "reads past");
  expectRefused(gltfWith("\"componentType\": 5123", "\"componentType\": 1234"));
  expectRefused(gltfWith("\"type\": \"VEC2\"", "\"type\": \"VEC2\", \"sparse\": {}"), "sparse");
  expectRefused(gltfWith("\"mesh\": 1", "\"mesh\": 7"));
  expectRefused(gltfWith("\"material\": 1", "\"material\": 9"));
  expectRefused(gltfWith("\"byteLength\": 9316", "\"byteLength\": 99999"), "buffer 0 is shorter");
  expectRefused(gltfWith("\"uri\": \"figure.bin\"", "\"uri\": \"missing.bin\""), "missing.bin");
  expectRefused(gltfWith("\"uri\": \"figure_tex.png\"", "\"uri\": \"missing.png\""), "missing.png");
  expectRefused(gltfWith("\"uri\": \"figure.bin\"", "\"uri\": \"../figure.bin\""), "outside");
  expectRefused(gltfWith("\"uri\": \"figure_tex.png\"", "\"uri\": \"figure.bin\""), "image 0");  // not a picture
  expectRefused(gltfWith("\"version\": \"2.0\"", "\"version\": \"1.0\""), "version 2");
  expectRefused(gltfWith("\"scene\": 0", "\"extensionsRequired\": [\"KHR_draco_mesh_compression\"], \"scene\": 0"),
                "KHR_draco_mesh_compression");
  expectRefused(gltfWith("\"children\": [", "\"children\": [0, "), "more than once");

  // Primitives that are not triangles are skipped, not fatal.
  Model3D lines;
  CHECK(loadText(gltfWith("\"indices\": 5", "\"indices\": 5, \"mode\": 1"), lines, err));
  CHECK_EQ((int)lines.parts.size(), 1);
}
