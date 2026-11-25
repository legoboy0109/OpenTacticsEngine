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
    Weather,     // text = "rain" | "snow" | "none" (map events only)
    Enter,       // text = speaker key, portrait = art id (-1 = the key), position; a portrait on stage without a line
    Move         // text = speaker key, position (the slot it slides to); ms
  };
  Kind kind = Kind::None;
  int text = -1;                 // index into the owner's text table (StoryData::texts / CampaignDef::texts)
  int ms = -1;                   // length; Say: "auto" ms; Focus: scroll ms (-1 = not given)
  uint32_t color = 0;            // Fade: 0xRRGGBB
  int strength = 3;              // Shake
  bool out = false;              // Fade: out (true) or in
  bool fadeTransition = false;   // Background / Enter / Leave: "transition": "fade" (else a cut)
  int portrait = -1;             // Enter: text index of the "portrait" art id (-1 = the speaker key itself)
  int speedMilli = 0;            // Focus: "speed" in squares per second x 1000 (0 = not given; then "ms" or the usual ease)
  int wait = -1;                 // Focus / MoveUnit: "wait" -1 = not given, 0 = false, 1 = true
  bool walk = false;             // MoveUnit: "walk": true
  int position = 0;              // Say / Enter / Move: 0 = none (today's look), kPos* (seven slots)
  bool narrate = false;          // Say: a "narrate" line (no name, no portrait, centred)
  // docs/EVENTS.md "Notices, faces, names and loops":
  bool notice = false;           // Say: a "notice" line (the system window, no speaker); Choice: "notice": true (the question and answers in it)
  int face = 0;                  // Say / Choice: "face" N (1..kMaxFaces) = the portrait's expression N; 0 = its own cell
  int cursor = 0;                // Choice: "cursor" N (1-based, as written) = the answer the highlight starts on; 0 = the first
};

// The seven stage slots. left / right / center keep their first values (saved dialogue flags and logs); the four added ones
// follow. Left to right on the stage: farLeft, left, midLeft, center, midRight, right, farRight (see stageSlot()).
enum : int { kPosNone = 0, kPosLeft = 1, kPosRight = 2, kPosCenter = 3, kPosFarLeft = 4, kPosMidLeft = 5, kPosMidRight = 6, kPosFarRight = 7 };
constexpr int kStageSlots = 7;
// Order across the stage, 0 (farLeft) .. 6 (farRight); -1 for kPosNone.
inline int stageSlot(int pos) {
  static const int order[8] = {-1, 1, 5, 3, 0, 2, 4, 6};
  return pos >= 0 && pos < 8 ? order[pos] : -1;
}
inline const char* positionName(int pos) {
  static const char* const names[8] = {"", "left", "right", "center", "farLeft", "midLeft", "midRight", "farRight"};
  return pos >= 0 && pos < 8 ? names[pos] : "";
}

// Limits the loaders enforce (milliseconds).
constexpr int kMaxSceneMs = 10000;   // fade, background transition, shake, focus scroll
constexpr int kMaxWaitMs = 60000;    // wait
constexpr int kMaxAutoMs = 60000;    // a line's "auto"
constexpr int kMaxShakeStrength = 32;
constexpr double kMinFocusSpeed = 0.1, kMaxFocusSpeed = 100;  // squares per second, a focus "speed"
constexpr int kDefaultFadeMs = 500;
constexpr int kDefaultSlideMs = 400;  // a portrait moving to another slot
constexpr int kMaxFaces = 15;         // a portrait's "expressions" and a line's "face"

// The entries a scene command leaves in a battle's event stream (EventType::SceneEffect) and the flags of the
// Dialogue / Focus / UnitRelocated entries that carry the extra keys.
enum : int {
  kSceneOut = 1,         // SceneEffect Fade: out
  kSceneFadeTrans = 2,   // SceneEffect Background: fade transition
  kSceneAuto = 4,        // SceneEffect Fade: the core faded it in by itself at the end of the run of events
  kDialoguePosMask = 7,  // Dialogue flags: position (kPos*)
  kDialogueNarrate = 8,  // Dialogue flags: narrate line
  kDialogueQuote = 64,   // Dialogue flags: a line of a battle quote (before the fight's first strike); the battle scene may play it inside the scene
  kDialogueDefeat = 128, // Dialogue flags: a line of a defeat quote (right after UnitDied); the battle scene may play it inside the scene
  kFocusNoWait = 16,     // Focus flags: "wait": false
  kRelocateWalk = 32,    // UnitRelocated flags: walk (data = the path, from..to)
  kDialogueNotice = 256, // Dialogue / Choice flags: shown in the notice window (docs/EVENTS.md "Notices")
  kDialogueFaceShift = 12, kDialogueFaceMask = 0xF000,     // Dialogue / Choice flags: the "face" (0 = the portrait's own cell)
  kChoiceCursorShift = 16, kChoiceCursorMask = 0x70000     // Choice flags: where the highlight starts, among the OFFERED answers
};

}  // namespace ote
