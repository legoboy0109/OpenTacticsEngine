#include "gfx3d/resilient3d.h"

#include "core/base/log.h"
#include "gfx3d/soft3d.h"

namespace ote {

ResilientRenderer3D::ResilientRenderer3D(std::unique_ptr<Renderer3D> inner, std::unique_ptr<DeviceKeeper> keeper,
                                         int simulateLossAt, bool refuseRebuild)
    : inner_(std::move(inner)), keeper_(std::move(keeper)), simulateAt_(simulateLossAt), refuseRebuild_(refuseRebuild) {}

ResilientRenderer3D::~ResilientRenderer3D() {
  inner_.reset();  // before the device objects go
  if (keeper_) keeper_->release();
}

MeshId ResilientRenderer3D::createMesh(const MeshData& data) {
  MeshId inner = inner_->createMesh(data);
  if (!inner && recoverIfLost()) inner = inner_->createMesh(data);
  if (!inner) return 0;
  size_t slot = 0;
  while (slot < meshes_.size() && meshes_[slot].used) ++slot;
  if (slot == meshes_.size()) meshes_.push_back(Mesh());
  meshes_[slot].used = true;
  meshes_[slot].data = data;
  meshes_[slot].inner = inner;
  recoverIfLost();
  return (MeshId)slot + 1;
}

void ResilientRenderer3D::updateMesh(MeshId mesh, const MeshData& data) {
  if (!validMesh(mesh)) return;
  Mesh& m = meshes_[(size_t)mesh - 1];
  m.data = data;
  inner_->updateMesh(m.inner, data);
  recoverIfLost();  // replays m.data, which is already the new one
}

void ResilientRenderer3D::destroyMesh(MeshId mesh) {
  if (!validMesh(mesh)) return;
  Mesh& m = meshes_[(size_t)mesh - 1];
  inner_->destroyMesh(m.inner);
  m = Mesh();
  recoverIfLost();
}

Texture3DId ResilientRenderer3D::createTexture(const uint8_t* rgba, int width, int height, bool repeat, bool mipmaps) {
  Texture3DId inner = inner_->createTexture(rgba, width, height, repeat, mipmaps);
  if (!inner && recoverIfLost()) inner = inner_->createTexture(rgba, width, height, repeat, mipmaps);
  if (!inner) return 0;
  size_t slot = 0;
  while (slot < textures_.size() && textures_[slot].used) ++slot;
  if (slot == textures_.size()) textures_.push_back(Texture());
  Texture& t = textures_[slot];
  t.used = true;
  t.rgba.assign(rgba, rgba + (size_t)width * (size_t)height * 4);
  t.w = width;
  t.h = height;
  t.repeat = repeat;
  t.mipmaps = mipmaps;
  t.inner = inner;
  recoverIfLost();
  return (Texture3DId)slot + 1;
}

void ResilientRenderer3D::destroyTexture(Texture3DId texture) {
  if (texture < 1 || (size_t)texture > textures_.size() || !textures_[(size_t)texture - 1].used) return;
  Texture& t = textures_[(size_t)texture - 1];
  inner_->destroyTexture(t.inner);
  t = Texture();
  recoverIfLost();
}

void ResilientRenderer3D::begin(const Frame3D& frame) {
  recoverIfLost();
  frame_ = frame;
  draws_.clear();
  inFrame_ = true;
  inner_->begin(frame);
}

void ResilientRenderer3D::draw(MeshId mesh, const Mat4& model, const Material3D& material) {
  if (!inFrame_ || !validMesh(mesh)) return;
  draws_.push_back(Draw{mesh, model, material});
  inner_->draw(meshes_[(size_t)mesh - 1].inner, model, innerMaterial(material));
}

void ResilientRenderer3D::end() {
  inner_->end();
  inFrame_ = false;
  if (!software_ && simulateAt_ > 0 && ++frames_ == simulateAt_) {
    OTE_WARN("%s: OTE_3D_SIMULATE_LOSS: losing the 3D device now", keeper_->name());
    keeper_->simulateLoss();  // the next use fails like after a real loss
    inner_->begin(frame_);    // one more frame, which finds the device gone (as after a real loss)
    inner_->end();
  }
  // The draws of this frame went nowhere if the device was gone: do them again on the rebuilt backend.
  if (recoverIfLost()) {
    inner_->begin(frame_);
    for (const Draw& d : draws_)
      if (validMesh(d.mesh)) inner_->draw(meshes_[(size_t)d.mesh - 1].inner, d.model, innerMaterial(d.material));
    inner_->end();
  }
}

Material3D ResilientRenderer3D::innerMaterial(Material3D m) const {
  if (m.texture >= 1 && (size_t)m.texture <= textures_.size() && textures_[(size_t)m.texture - 1].used)
    m.texture = textures_[(size_t)m.texture - 1].inner;
  else
    m.texture = 0;
  return m;
}

bool ResilientRenderer3D::recoverIfLost() {
  if (software_ || !keeper_->lost()) return false;
  OTE_WARN("%s: %s; rebuilding it", keeper_->name(), keeper_->lossText());
  inner_.reset();  // its objects went with the device; the backend notices and frees what it still can
  std::string why;
  if (refuseRebuild_) why = "OTE_3D_SIMULATE_NO_REBUILD";
  std::unique_ptr<Renderer3D> fresh = refuseRebuild_ ? nullptr : keeper_->rebuild(why);
  if (fresh) {
    inner_ = std::move(fresh);
    replay();
    OTE_INFO("%s: device rebuilt, %zu meshes and %zu textures uploaded again", keeper_->name(), countMeshes(),
             countTextures());
    return true;
  }
  OTE_WARN("%s: cannot rebuild the device (%s); using the software 3D renderer for the rest of the session",
           keeper_->name(), why.c_str());
  keeper_->release();
  inner_.reset(new SoftRenderer3D());
  software_ = true;
  replay();
  return true;
}

void ResilientRenderer3D::replay() {
  for (Mesh& m : meshes_)
    if (m.used) m.inner = inner_->createMesh(m.data);
  for (Texture& t : textures_)
    if (t.used) t.inner = inner_->createTexture(t.rgba.data(), t.w, t.h, t.repeat, t.mipmaps);
}

size_t ResilientRenderer3D::countMeshes() const {
  size_t n = 0;
  for (const Mesh& m : meshes_) n += m.used;
  return n;
}

size_t ResilientRenderer3D::countTextures() const {
  size_t n = 0;
  for (const Texture& t : textures_) n += t.used;
  return n;
}

}  // namespace ote
