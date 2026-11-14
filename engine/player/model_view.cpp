#include "model_view.h"

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

bool ModelView::load(Renderer3D& r3d, const std::string& path, std::string& err) {
  release(r3d);
  if (!loadModel(path, r3d.caps(), decodeForModel, model_, err)) return false;
  // A texture per material, not per image: repeat/clamp belongs to the material.
  textures_.assign(model_.materials.size(), 0);
  for (size_t i = 0; i < model_.materials.size(); ++i) {
    const ModelMaterial& m = model_.materials[i];
    if (m.image < 0 || (size_t)m.image >= model_.images.size()) continue;
    const ModelImage& im = model_.images[(size_t)m.image];
    textures_[i] = r3d.createTexture(im.rgba.data(), im.width, im.height, m.repeat);
  }
  for (size_t n = 0; n < model_.nodes.size(); ++n) {
    Mat4 node = model_.nodeMatrix((int)n);
    for (int p : model_.nodes[n].parts) {
      if (p < 0 || (size_t)p >= model_.parts.size()) continue;
      const ModelPart& part = model_.parts[(size_t)p];
      Part out;
      out.mesh = r3d.createMesh(part.mesh);
      if (!out.mesh) continue;
      out.node = node;
      if (part.material >= 0 && (size_t)part.material < model_.materials.size()) {
        const ModelMaterial& m = model_.materials[(size_t)part.material];
        out.material.texture = textures_[(size_t)part.material];
        out.material.color = m.color;
        out.material.blend = m.blend;
        out.material.cutoff = m.cutoff;
        out.material.lit = m.lit;
        out.material.twoSided = m.twoSided;
        out.material.smooth = m.smooth;
      }
      parts_.push_back(out);
    }
  }
  if (parts_.empty()) {
    err = "nothing in it could be drawn";
    release(r3d);
    return false;
  }
  return true;
}

void ModelView::draw(Renderer3D& r3d, const Mat4& place) const {
  for (const Part& p : parts_) r3d.draw(p.mesh, place * p.node, p.material);
}

void ModelView::release(Renderer3D& r3d) {
  for (const Part& p : parts_) r3d.destroyMesh(p.mesh);
  for (Texture3DId t : textures_) r3d.destroyTexture(t);
  parts_.clear();
  textures_.clear();
  model_ = Model3D();
}

}  // namespace ote
