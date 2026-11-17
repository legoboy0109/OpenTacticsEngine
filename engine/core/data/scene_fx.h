// Story scene presentation (docs/EVENTS.md "Scene presentation"): backgrounds, fades, waits, shakes, staged portraits,
// weather and the camera/walk options of focus and moveUnit. All of it is presentation only: the rules never read it, the
// simulator skips it, and one struct serves map events (Command::fx) and campaign scenes (SceneCommand::fx).
#pragma once
#include <cstdint>

namespace ote {

struct SceneFx {
  enum class Kind {
    None,        // the command is not a scene command (Say / Focus / MoveUnit use the extra keys below)
    Background,  // text = picture id of data/graphics.json "backgrounds" (-1 = back to the map / nothing); ms, fadeTransition
    Fade,        // out = fade to `color` (stays until faded in); ms
    Wait,        // ms
    Shake,       // ms, strength (logical pixels)
    Leave,       // text = speaker key that leaves the stage (-1 = everybody)
    Weather      // text = "rain" | "snow" | "none" (map events only)
  };
  Kind kind = Kind::None;
  int text = -1;                 // index into the owner's text table (StoryData::texts / CampaignDef::texts)
  int ms = -1;                   // length; Say: "auto" ms; Focus: scroll ms (-1 = not given)
  uint32_t color = 0;            // Fade: 0xRRGGBB
  int strength = 3;              // Shake
  bool out = false;              // Fade: out (true) or in
  bool fadeTransition = false;   // Background: "transition": "fade" (else a cut)
  int wait = -1;                 // Focus / MoveUnit: "wait" -1 = not given, 0 = false, 1 = true
  bool walk = false;             // MoveUnit: "walk": true
  int position = 0;              // Say: 0 = none (today's look), kPosLeft / kPosRight / kPosCenter
  bool narrate = false;          // Say: a "narrate" line (no name, no portrait, centred)
};

enum : int { kPosNone = 0, kPosLeft = 1, kPosRight = 2, kPosCenter = 3 };

// Limits the loaders enforce (milliseconds).
constexpr int kMaxSceneMs = 10000;   // fade, background transition, shake, focus scroll
constexpr int kMaxWaitMs = 60000;    // wait
constexpr int kMaxAutoMs = 60000;    // a line's "auto"
constexpr int kMaxShakeStrength = 32;
constexpr int kDefaultFadeMs = 500;

// The entries a scene command leaves in a battle's event stream (EventType::SceneEffect) and the flags of the
// Dialogue / Focus / UnitRelocated entries that carry the extra keys.
enum : int {
  kSceneOut = 1,         // SceneEffect Fade: out
  kSceneFadeTrans = 2,   // SceneEffect Background: fade transition
  kSceneAuto = 4,        // SceneEffect Fade: the core faded it in by itself at the end of the run of events
  kDialoguePosMask = 3,  // Dialogue flags: position (kPos*)
  kDialogueNarrate = 8,  // Dialogue flags: narrate line
  kFocusNoWait = 16,     // Focus flags: "wait": false
  kRelocateWalk = 32     // UnitRelocated flags: walk (data = the path, from..to)
};

}  // namespace ote
