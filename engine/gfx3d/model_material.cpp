// glTF loader: materials, textures and images. Only what a fixed-function renderer can use: base colour, base
// colour texture, alpha mode, double-sided, unlit, texture wrap/filter. Everything else PBR is ignored on purpose.
#include <cstdio>

#include "core/base/log.h"
#include "gfx3d/model_internal.h"

namespace ote {
namespace gltf {

namespace {

const int kClamp = 33071, kNearest = 9728;

// Loads (once) glTF image `idx` into out.images; returns its index there or -1 with err set.
bool loadImage(Context& c, size_t idx, std::vector<int>& cache, Model3D& out, int& result, std::string& err) {
  char msg[128];
  if (cache[idx] >= 0) {
    result = cache[idx];
    return true;
  }
  const Json& j = c.json["images"][idx];
  std::vector<uint8_t> own;
  const uint8_t* data = nullptr;
  size_t size = 0;
  size_t view;
  if (j["uri"].isString()) {
    if (!loadUri(c.baseDir, j["uri"].asString(), own, err)) return false;
    data = own.data();
    size = own.size();
  } else if (asIndex(j["bufferView"], view)) {
    size_t stride;
    if (!viewData(c, view, data, size, stride, err)) return false;
  } else {
    std::snprintf(msg, sizeof msg, "its image %zu has neither a file nor embedded data", idx);
    err = msg;
    return false;
  }
  if (!c.decode) {
    err = "it has pictures but no image decoder was given";
    return false;
  }
  ModelImage img;
  std::string why;
  if (!c.decode(data, size, img, why)) {
    std::snprintf(msg, sizeof msg, "its image %zu cannot be read: ", idx);
    err = msg + why;
    return false;
  }
  if (img.width <= 0 || img.height <= 0 || img.rgba.size() < (size_t)img.width * (size_t)img.height * 4) {
    std::snprintf(msg, sizeof msg, "its image %zu decoded to nothing usable", idx);
    err = msg;
    return false;
  }
  fitImage(img, c.caps.maxTextureSize);
  out.images.push_back(img);
  cache[idx] = result = (int)out.images.size() - 1;
  return true;
}

}  // namespace

bool buildMaterials(Context& c, Model3D& out, std::string& err) {
  const Json& mats = c.json["materials"];
  std::vector<int> imageCache(c.json["images"].size(), -1);
  for (size_t m = 0; m < mats.size(); ++m) {
    const Json& j = mats[m];
    ModelMaterial mm;
    mm.name = j["name"].asString();
    const Json& pbr = j["pbrMetallicRoughness"];
    const Json& f = pbr["baseColorFactor"];
    if (f.size() >= 4)
      mm.color = Rgba3D((float)f[0].asNumber(1), (float)f[1].asNumber(1), (float)f[2].asNumber(1), (float)f[3].asNumber(1));
    std::string mode = j["alphaMode"].asString("OPAQUE");
    mm.blend = mode == "MASK" ? Blend3D::Cutout : mode == "BLEND" ? Blend3D::Blend : Blend3D::Opaque;
    mm.cutoff = (float)j["alphaCutoff"].asNumber(0.5);
    mm.twoSided = j["doubleSided"].asBool(false);
    mm.lit = !j["extensions"].has("KHR_materials_unlit");

    const Json& bt = pbr["baseColorTexture"];
    size_t tex;
    if (bt.isObject() && asIndex(bt["index"], tex)) {
      if (bt["texCoord"].asInt(0) != 0) OTE_WARN("model: material %zu uses a second texture coordinate set; its texture is ignored", m);
      else {
        const Json& t = c.json["textures"][tex];
        size_t src;
        if (!t.isObject()) {
          char msg[96];
          std::snprintf(msg, sizeof msg, "its material %zu uses texture %zu, which does not exist", m, tex);
          err = msg;
          return false;
        }
        if (asIndex(t["source"], src)) {  // textures only reachable through extensions (webp, basis) have none
          if (!c.json["images"][src].isObject()) {
            char msg[96];
            std::snprintf(msg, sizeof msg, "its texture %zu uses image %zu, which does not exist", tex, src);
            err = msg;
            return false;
          }
          if (!loadImage(c, src, imageCache, out, mm.image, err)) return false;
        }
        size_t smp;
        if (asIndex(t["sampler"], smp)) {
          const Json& s = c.json["samplers"][smp];
          // Mirrored wrap counts as repeat; clamping on either axis turns repeat off.
          mm.repeat = s["wrapS"].asInt(10497) != kClamp && s["wrapT"].asInt(10497) != kClamp;
          mm.smooth = s["magFilter"].asInt(0) != kNearest;
        }
      }
    }
    out.materials.push_back(mm);
  }
  return true;
}

}  // namespace gltf
}  // namespace ote
