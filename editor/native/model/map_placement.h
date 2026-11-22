// Placing things on the map by square: deployment slots (prep.slots) and units. Each call is ONE undo step (a macro on the
// project's stack) and writes only the numbers it must, so the rest of the file stays byte for byte. Plain-words refusals go to
// `err`; the canvas shows them in its status line.
#pragma once
#include <QString>

#include "json_document.h"

namespace ote_native {

class Project;

class MapPlacement {
 public:
  static constexpr int kMaxSlots = 64;  // the loader's limit

  MapPlacement(Project* project, JsonDocument* doc) : project_(project), doc_(doc) {}

  // Index of the slot / unit standing on a square, or -1.
  int slotAt(int x, int y) const;
  int unitAt(int x, int y) const;

  // A map without "prep" gets one ({ "slots": [...] }): that switches its preparation screen on.
  bool addSlot(int x, int y, int mapW, int mapH, QString* err);
  bool moveSlot(size_t index, int x, int y, int mapW, int mapH, QString* err);
  // The slots after it are renumbered, so "required" entries that name a slot number follow (the removed slot's own entry loses its
  // "slot" key) and "limit" is lowered to the slot count. The last slot cannot be removed (the loader wants at least one).
  bool removeSlot(size_t index, QString* err);
  // A unit may not share a square with another unit.
  bool moveUnit(size_t index, int x, int y, int mapW, int mapH, QString* err);

 private:
  Project* project_;
  JsonDocument* doc_;
};

}  // namespace ote_native
