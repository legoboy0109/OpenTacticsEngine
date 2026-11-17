#include "ui/scene_stage.h"

#include <algorithm>
#include <cmath>

namespace ote {

void SceneStage::setBackground(const std::string& id, bool fade, int ms) {
  if (id == bg_ && prev_.empty()) return;
  if (!fade || ms <= 0) {  // a cut
    bg_ = id;
    prev_.clear();
    bgT_ = 1;
    return;
  }
  prev_ = bg_;  // the picture on screen now fades out as the new one fades in (a clear: to the map)
  bg_ = id;
  bgT_ = 0;
  bgSpeed_ = 1000.0f / (float)ms;
}

void SceneStage::fade(bool out, uint32_t color, int ms) {
  fadeOut_ = out;
  if (out) color_ = color;  // a fade in comes back from the colour the screen went out to, whatever it is given
  const float target = out ? 1.0f : 0.0f;
  if (ms <= 0) {
    fadeAmt_ = target;
    fadeSpeed_ = 0;
    return;
  }
  fadeSpeed_ = 1000.0f / (float)ms;
  if (fadeAmt_ == target) fadeSpeed_ = 0;
}

void SceneStage::shake(int ms, int strength) {
  if (ms <= 0 || strength <= 0) return;
  shakeLeft_ = shakeTotal_ = (float)ms / 1000.0f;
  strength_ = strength;
  shakeElapsed_ = 0;
}

void SceneStage::setWeather(const std::string& kind) { weather_ = kind == "none" ? std::string() : kind; }

bool SceneStage::staged(const std::string& key) const {
  for (const Portrait& p : portraits_)
    if (p.key == key) return true;
  return false;
}

bool SceneStage::say(const std::string& key, const std::string& art, int position) {
  speaker_ = key;
  if (position == 0) return !key.empty() && staged(key);  // standing already: it lights up where it stands
  // One portrait per place: a newcomer replaces whoever stood there; a speaker that moves leaves its old place.
  portraits_.erase(std::remove_if(portraits_.begin(), portraits_.end(),
                                  [&](const Portrait& p) { return p.position == position || (!key.empty() && p.key == key); }),
                   portraits_.end());
  Portrait p;
  p.key = key;
  p.art = art;
  p.position = position;
  portraits_.push_back(p);
  return true;
}

void SceneStage::leave(const std::string& key) {
  if (key.empty()) {
    clearPortraits();
    return;
  }
  portraits_.erase(std::remove_if(portraits_.begin(), portraits_.end(), [&](const Portrait& p) { return p.key == key; }),
                   portraits_.end());
  if (speaker_ == key) speaker_.clear();
}

void SceneStage::clearPortraits() {
  portraits_.clear();
  speaker_.clear();
}

void SceneStage::syncFrom(const std::string& background, const std::string& weather) {
  bg_ = background;
  prev_.clear();
  bgT_ = 1;
  fadeAmt_ = 0;
  fadeOut_ = false;
  fadeSpeed_ = 0;
  shakeLeft_ = 0;
  weather_ = weather == "none" ? std::string() : weather;
  clearPortraits();
}

void SceneStage::update(double dt) {
  const float d = (float)dt;
  clock_ += d;
  if (bgT_ < 1) {
    bgT_ = std::min(1.0f, bgT_ + bgSpeed_ * d);
    if (bgT_ >= 1) prev_.clear();
  }
  const float target = fadeOut_ ? 1.0f : 0.0f;
  if (fadeAmt_ != target) {
    fadeAmt_ += (target > fadeAmt_ ? 1 : -1) * fadeSpeed_ * d;
    if ((fadeOut_ && fadeAmt_ >= 1.0f) || (!fadeOut_ && fadeAmt_ <= 0.0f) || fadeSpeed_ <= 0) fadeAmt_ = target;
  }
  if (shakeLeft_ > 0) {
    shakeLeft_ = std::max(0.0f, shakeLeft_ - d);
    shakeElapsed_ += d;
  }
}

void SceneStage::finishNow() {
  if (bgT_ < 1) {
    bgT_ = 1;
    prev_.clear();
  }
  fadeAmt_ = fadeOut_ ? 1.0f : 0.0f;
  fadeSpeed_ = 0;
  shakeLeft_ = 0;
}

// A fast wobble that dies away linearly; the same on every machine (no random numbers).
int SceneStage::shakeX() const {
  if (shakeLeft_ <= 0) return 0;
  const float k = shakeLeft_ / shakeTotal_;
  return (int)std::lround((double)strength_ * k * std::sin(shakeElapsed_ * 55.0f));
}

int SceneStage::shakeY() const {
  if (shakeLeft_ <= 0) return 0;
  const float k = shakeLeft_ / shakeTotal_;
  return (int)std::lround((double)strength_ * k * 0.6 * std::sin(shakeElapsed_ * 71.0f + 1.0f));
}

}  // namespace ote
