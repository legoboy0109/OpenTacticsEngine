#include "battle/batch3d.h"

namespace ote {

namespace {
bool same(const Material3D& a, const Material3D& b) {
  return a.texture == b.texture && a.blend == b.blend && a.lit == b.lit && a.twoSided == b.twoSided &&
         a.smooth == b.smooth && a.depthTest == b.depthTest && a.cutoff == b.cutoff && a.color.r == b.color.r &&
         a.color.g == b.color.g && a.color.b == b.color.b && a.color.a == b.color.a;
}
}  // namespace

MeshData& Batch3D::mesh(const Material3D& material) {
  for (size_t i = 0; i < slots_.size(); ++i)
    if (same(slots_[i].material, material)) {
      if (!slots_[i].used) {
        slots_[i].used = true;
        order_.push_back(i);
      }
      return slots_[i].data;
    }
  Slot s;
  s.material = material;
  s.used = true;
  slots_.push_back(s);
  order_.push_back(slots_.size() - 1);
  return slots_.back().data;
}

void Batch3D::flush(Renderer3D& r3d) {
  for (size_t i : order_) {
    Slot& s = slots_[i];
    s.used = false;
    if (s.data.indices.empty()) continue;
    if (s.id) r3d.updateMesh(s.id, s.data);
    else s.id = r3d.createMesh(s.data);
    if (s.id) r3d.draw(s.id, Mat4(), s.material);
    s.data.vertices.clear();
    s.data.indices.clear();
  }
  order_.clear();
}

void Batch3D::release(Renderer3D& r3d) {
  for (Slot& s : slots_)
    if (s.id) r3d.destroyMesh(s.id);
  slots_.clear();
  order_.clear();
}

}  // namespace ote
