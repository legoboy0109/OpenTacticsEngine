#include "ui/scene_stage.h"

#include <algorithm>
#include <cmath>

#include "core/data/scene_fx.h"

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

float SceneStage::Portrait::slotNow() const {
  const float to = (float)stageSlot(position);
  return slideT >= 1 ? to : fromSlot + (to - fromSlot) * slideT;
}

bool SceneStage::wide() const {
  for (const Portrait& p : portraits_)
    if (p.position >= kPosFarLeft) return true;
  return false;
}

bool SceneStage::animating() const {
  if ((bgT_ < 1) || fading() || shakeLeft_ > 0) return true;
  for (const Portrait& p : portraits_)
    if (p.alphaSpeed != 0 || p.slideT < 1 || p.leaving) return true;
  return false;
}

// One portrait per place: a newcomer replaces whoever stood there (and `key` leaves its old place).
static void clearPlace(std::vector<SceneStage::Portrait>& list, const std::string& key, int position) {
  list.erase(std::remove_if(list.begin(), list.end(),
                            [&](const SceneStage::Portrait& p) { return p.position == position || (!key.empty() && p.key == key); }),
             list.end());
}

bool SceneStage::say(const std::string& key, const std::string& art, int position) {
  speaker_ = key;
  if (position == 0) return !key.empty() && staged(key);  // standing already: it lights up where it stands
  // A speaker already standing in that very place just lights up (a fade-in under way goes on); otherwise it takes the place.
  for (Portrait& p : portraits_)
    if (p.key == key && p.position == position && !p.leaving) {
      if (!art.empty()) p.art = art;
      return true;
    }
  clearPlace(portraits_, key, position);
  Portrait p;
  p.key = key;
  p.art = art;
  p.position = position;
  portraits_.push_back(p);
  return true;
}

void SceneStage::enter(const std::string& key, const std::string& art, int position, int fadeMs) {
  if (key.empty() || position == 0) return;
  clearPlace(portraits_, key, position);
  Portrait p;
  p.key = key;
  p.art = art;
  p.position = position;
  if (fadeMs > 0) {
    p.alpha = 0;
    p.alphaSpeed = 1000.0f / (float)fadeMs;
  }
  portraits_.push_back(p);
}

void SceneStage::move(const std::string& key, int position, int ms) {
  if (key.empty() || position == 0) return;
  Portrait* mover = nullptr;
  for (Portrait& p : portraits_)
    if (p.key == key && !p.leaving) mover = &p;
  if (!mover) return;
  const Portrait copy = *mover;
  if (copy.position == position) return;
  // Whoever stood in the target slot leaves (the mover keeps its entry: find it again after the erase).
  portraits_.erase(std::remove_if(portraits_.begin(), portraits_.end(),
                                  [&](const Portrait& p) { return p.position == position && p.key != key; }),
                   portraits_.end());
  for (Portrait& p : portraits_)
    if (p.key == key && !p.leaving) {
      p.fromSlot = p.slotNow();
      p.position = position;
      if (ms > 0) {
        p.slideT = 0;
        p.slideSpeed = 1000.0f / (float)ms;
      } else {
        p.slideT = 1;
      }
    }
}

void SceneStage::leave(const std::string& key, int ms) {
  if (key.empty()) {
    if (ms <= 0) {
      clearPortraits();
      return;
    }
    for (Portrait& p : portraits_) {
      p.leaving = true;
      p.alphaSpeed = -1000.0f / (float)ms;
    }
    speaker_.clear();
    return;
  }
  if (ms <= 0) {
    portraits_.erase(std::remove_if(portraits_.begin(), portraits_.end(), [&](const Portrait& p) { return p.key == key; }),
                     portraits_.end());
  } else {
    for (Portrait& p : portraits_)
      if (p.key == key) {
        p.leaving = true;
        p.alphaSpeed = -1000.0f / (float)ms;
      }
  }
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
  for (Portrait& p : portraits_) {
    if (p.alphaSpeed != 0) {
      p.alpha += p.alphaSpeed * d;
      if (p.alpha >= 1) { p.alpha = 1; p.alphaSpeed = 0; }
      if (p.alpha <= 0) { p.alpha = 0; p.alphaSpeed = 0; }
    }
    if (p.slideT < 1) p.slideT = std::min(1.0f, p.slideT + p.slideSpeed * d);
  }
  portraits_.erase(std::remove_if(portraits_.begin(), portraits_.end(), [](const Portrait& p) { return p.leaving && p.alpha <= 0; }),
                   portraits_.end());
}

void SceneStage::finishNow() {
  if (bgT_ < 1) {
    bgT_ = 1;
    prev_.clear();
  }
  fadeAmt_ = fadeOut_ ? 1.0f : 0.0f;
  fadeSpeed_ = 0;
  shakeLeft_ = 0;
  for (Portrait& p : portraits_) {
    p.alpha = p.leaving ? 0.0f : 1.0f;
    p.alphaSpeed = 0;
    p.slideT = 1;
  }
  portraits_.erase(std::remove_if(portraits_.begin(), portraits_.end(), [](const Portrait& p) { return p.leaving; }), portraits_.end());
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
