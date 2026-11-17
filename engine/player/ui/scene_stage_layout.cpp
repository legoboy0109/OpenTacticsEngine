#include "ui/scene_stage_layout.h"

#include <algorithm>
#include <cmath>

namespace ote {

namespace {

// Neighbours overlap by at most this share of the (wider) portrait's width.
constexpr float kMaxOverlap = 0.40f;

struct Size {
  int w, h;
  float s;
};

Size sizeAt(const StageItem& it, float s) {
  return Size{std::max(1, (int)std::lround((double)it.artW * s)), std::max(1, (int)std::lround((double)it.artH * s)), s};
}

// The largest scale that fits the art in 55% of the screen height and 45% of its width (today's rule): whole numbers when it
// grows, a fraction only when it has to shrink.
float fitScale(const StageItem& it, int screenW, int screenH, int baseY) {
  const int maxH = std::max(16, std::min(baseY - 2, screenH * 55 / 100)), maxW = std::max(16, screenW * 45 / 100);
  float s = std::min((float)maxH / (float)std::max(1, it.artH), (float)maxW / (float)std::max(1, it.artW));
  if (s >= 1.0f) s = std::floor(s);
  return s;
}

}  // namespace

std::vector<size_t> phoneVisible(const std::vector<StageItem>& items) {
  std::vector<size_t> order;  // by slot, left to right
  for (size_t i = 0; i < items.size(); ++i) order.push_back(i);
  std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return items[a].slot < items[b].slot; });
  if ((int)order.size() <= kMaxPhonePortraits) return order;
  size_t first = 0;
  bool anyLit = false;
  for (size_t k = 0; k < order.size(); ++k)
    if (items[order[k]].lit) {
      anyLit = true;
      first = k == 0 ? 0 : k + 1 >= order.size() ? order.size() - kMaxPhonePortraits : k - 1;  // speaker +- its neighbours
    }
  if (!anyLit) first = (order.size() - kMaxPhonePortraits) / 2;
  return std::vector<size_t>(order.begin() + (long)first, order.begin() + (long)first + kMaxPhonePortraits);
}

std::vector<StagePlace> layoutStage(int W, int H, int baseY, bool wide, const std::vector<StageItem>& items) {
  std::vector<StagePlace> out(items.size());
  if (items.empty()) return out;
  const int margin = std::max(2, W / 60);
  const bool phone = H > W;
  std::vector<size_t> drawn;
  if (phone && wide) drawn = phoneVisible(items);
  else
    for (size_t i = 0; i < items.size(); ++i) drawn.push_back(i);
  // Phone: the drawn ones take the slots 0, 3, 6 (or 0, 6; or where they are when alone), whatever slots they stand in.
  std::vector<float> at(items.size(), 0);
  for (size_t i = 0; i < items.size(); ++i) at[i] = items[i].slot;
  if (phone && wide && drawn.size() > 1) {
    std::vector<size_t> byslot = drawn;
    std::stable_sort(byslot.begin(), byslot.end(), [&](size_t a, size_t b) { return items[a].slot < items[b].slot; });
    for (size_t k = 0; k < byslot.size(); ++k) at[byslot[k]] = 6.0f * (float)k / (float)(byslot.size() - 1);
  }
  // Scale: today's fit for each, then (wide only) smaller until neighbours overlap acceptably.
  std::vector<float> scale(items.size(), 1);
  for (size_t i : drawn) scale[i] = fitScale(items[i], W, H, baseY);
  auto xOf = [&](size_t i, const Size& sz) {
    if (!wide) {  // classic: left / center / right = slots 1 / 3 / 5 against the edges and in the middle
      const float t = (at[i] - 3.0f) / 2.0f;  // -1 left .. 0 centre .. +1 right (fractional while sliding)
      const int left = margin, mid = (W - sz.w) / 2, right = W - sz.w - margin;
      if (t == 0) return mid;
      return t < 0 ? mid + (int)std::lround((double)(left - mid) * -t) : mid + (int)std::lround((double)(right - mid) * t);
    }
    return margin + (int)std::lround((double)at[i] * (double)(W - 2 * margin - sz.w) / 6.0);
  };
  if (wide) {
    for (int guard = 0; guard < 40; ++guard) {
      bool ok = true;
      std::vector<size_t> byx = drawn;
      std::stable_sort(byx.begin(), byx.end(), [&](size_t a, size_t b) { return at[a] < at[b]; });
      for (size_t k = 0; k + 1 < byx.size() && ok; ++k) {
        const Size a = sizeAt(items[byx[k]], scale[byx[k]]), b = sizeAt(items[byx[k + 1]], scale[byx[k + 1]]);
        const int gap = xOf(byx[k + 1], b) - xOf(byx[k], a);
        if ((float)gap < (1.0f - kMaxOverlap) * (float)std::max(a.w, b.w)) ok = false;
      }
      if (ok) break;
      for (size_t i : drawn) scale[i] = scale[i] > 1.5f ? std::floor(scale[i]) - 1 : std::max(0.25f, scale[i] * 0.85f);
    }
  }
  for (size_t i : drawn) {
    const Size sz = sizeAt(items[i], scale[i]);
    out[i].shown = true;
    out[i].w = sz.w;
    out[i].h = sz.h;
    out[i].scale = scale[i];
    out[i].x = xOf(i, sz);
  }
  return out;
}

}  // namespace ote
