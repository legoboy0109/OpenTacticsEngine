#include "battle/models3d.h"

#include <algorithm>

#include "core/base/log.h"
#include "platform/common/image.h"

namespace ote {

namespace {
bool decodeForModel(const uint8_t* data, size_t size, ModelImage& out, std::string& err) {
  Image im;
  if (!decodeImage(data, size, im, err)) return false;
  out.width = im.width;
  out.height = im.height;
  out.rgba.swap(im.rgba);
  return true;
}
}  // namespace

const ModelAsset* Models3D::get(Renderer3D& r3d, const std::string& file) {
  for (ModelAsset* a : assets_)
    if (a->file == file) return a->ok ? a : nullptr;
  ModelAsset* a = new ModelAsset();
  assets_.push_back(a);
  a->file = file;
  std::string err;
  if (!loadModel(dir_ + "/" + file, r3d.caps(), decodeForModel, a->model, err)) {
    OTE_ERROR("model \"%s\": %s; what uses it keeps the placeholder look", file.c_str(), err.c_str());
    return nullptr;
  }
  const Model3D& m = a->model;
  a->textures.assign(m.materials.size(), 0);
  for (size_t i = 0; i < m.materials.size(); ++i) {
    int im = m.materials[i].image;
    if (im < 0 || (size_t)im >= m.images.size()) continue;
    a->textures[i] = r3d.createTexture(m.images[(size_t)im].rgba.data(), m.images[(size_t)im].width, m.images[(size_t)im].height,
                                       m.materials[i].repeat, m.materials[i].smooth);  // smooth (glTF sampler: anything but NEAREST): mipmaps too
  }
  for (size_t n = 0; n < m.nodes.size(); ++n)
    for (int p : m.nodes[n].parts) {
      if (p < 0 || (size_t)p >= m.parts.size()) continue;
      const ModelPart& part = m.parts[(size_t)p];
      ModelAsset::Piece piece{(int)n, p, 0};
      if (part.skinned() && m.nodes[n].skin >= 0) a->bends = true;
      else if (!(piece.rigid = r3d.createMesh(part.mesh))) continue;
      a->pieces.push_back(piece);
    }
  if (a->pieces.empty()) {
    OTE_ERROR("model \"%s\": nothing in it could be drawn; what uses it keeps the placeholder look", file.c_str());
    return nullptr;
  }
  a->height = std::max(0.01f, m.boundsMax.y);
  a->width = std::max(0.01f, std::max(m.boundsMax.x - m.boundsMin.x, m.boundsMax.z - m.boundsMin.z));
  a->ok = true;
  OTE_INFO("model \"%s\": %d piece(s), %d animation(s)", file.c_str(), (int)a->pieces.size(), (int)m.clips.size());
  return a;
}

void Models3D::release(Renderer3D& r3d) {
  for (ModelAsset* a : assets_) {
    for (const ModelAsset::Piece& p : a->pieces)
      if (p.rigid) r3d.destroyMesh(p.rigid);
    for (Texture3DId t : a->textures)
      if (t) r3d.destroyTexture(t);
    delete a;
  }
  assets_.clear();
}

void ModelActor::release(Renderer3D& r3d) {
  for (MeshId m : meshes)
    if (m) r3d.destroyMesh(m);
  meshes.clear();
  asset = nullptr;
}

void poseActor(Renderer3D& r3d, const ModelAsset& asset, ModelActor& actor, int clip, float time, bool loop) {
  if (actor.asset != &asset) {
    actor.release(r3d);
    actor.asset = &asset;
  }
  poseModel(asset.model, clip, time, loop, actor.pose);
  if (!asset.bends) return;
  actor.meshes.resize(asset.pieces.size(), 0);
  for (size_t i = 0; i < asset.pieces.size(); ++i) {
    const ModelAsset::Piece& p = asset.pieces[i];
    if (p.rigid) continue;
    skinPart(asset.model, actor.pose, p.node, p.part, actor.posed);
    if (actor.posed.indices.empty()) continue;
    if (actor.meshes[i]) r3d.updateMesh(actor.meshes[i], actor.posed);
    else actor.meshes[i] = r3d.createMesh(actor.posed);
  }
}

void drawModel(Renderer3D& r3d, const ModelAsset& asset, const ModelActor& actor, const Mat4& place, const ModelTint& tint) {
  const Model3D& m = asset.model;
  for (size_t i = 0; i < asset.pieces.size(); ++i) {
    const ModelAsset::Piece& p = asset.pieces[i];
    MeshId mesh = p.rigid ? p.rigid : (i < actor.meshes.size() ? actor.meshes[i] : 0);
    if (!mesh) continue;
    Material3D mat;
    mat.depthTest = tint.depthTest;
    Rgba3D col(tint.dim, tint.dim, tint.dim, tint.alpha);
    int mi = m.parts[(size_t)p.part].material;
    if (mi >= 0 && (size_t)mi < m.materials.size()) {
      const ModelMaterial& mm = m.materials[(size_t)mi];
      mat.texture = asset.textures[(size_t)mi];
      mat.blend = mm.blend;
      mat.cutoff = mm.cutoff;
      mat.lit = mm.lit;
      mat.twoSided = mm.twoSided;
      mat.smooth = mm.smooth;
      col = Rgba3D(mm.color.r * tint.dim, mm.color.g * tint.dim, mm.color.b * tint.dim, mm.color.a * tint.alpha);
      if (!tint.sideMaterial.empty() && mm.name == tint.sideMaterial) {
        col.r *= tint.side.r;
        col.g *= tint.side.g;
        col.b *= tint.side.b;
      }
    }
    if (tint.alpha < 0.999f) mat.blend = Blend3D::Blend;
    mat.color = col;
    // A rigid piece follows its node; a bent one is already in the model's space.
    bool rigid = p.rigid != 0 && (size_t)p.node < actor.pose.world.size();
    r3d.draw(mesh, rigid ? place * actor.pose.world[(size_t)p.node] : place, mat);
  }
}

}  // namespace ote
