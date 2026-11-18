// A map's preparation screen (docs/EVENTS.md "Preparation"): the map file's optional "prep" block, as read by the
// loader (loader_prep.cpp). Plain data; the rules that use it are in core/campaign/prep.*.
#pragma once
#include <string>
#include <vector>

namespace ote {

struct PrepSlot {
  int x = 0, y = 0;
};

// A character that always fights on this map (cannot be benched); `slot` >= 0 also fixes the square it starts on.
struct PrepRequired {
  std::string id;  // story id (a character id)
  int slot = -1;   // index into PrepDef::slots, -1 = any slot
};

// One preparation shop (docs/EVENTS.md "Preparation"): what it sells, whether it buys, and the music played while it is open.
struct PrepShop {
  std::string id;            // "" for the single unnamed `shop`
  std::string name;          // shown in the preparation menu ("" = the plain word "Shop")
  std::vector<int> sells;    // item indices (docs/ITEMS.md "Shops"); every one has a price
  bool buys = true;          // the shop also buys the party's items
  std::string music;         // a music id of data/audio.json played while this shop's screens are open ("" = the game's shopMusic, if any)
};

struct PrepDef {
  std::vector<PrepSlot> slots;           // deployment squares, in numbering order (slot 1 is index 0)
  int limit = 0;                         // most characters that can be deployed (1 .. slots.size())
  std::vector<PrepRequired> required;    // always deployed
  std::vector<std::string> forbidden;    // story ids that cannot take part in this map
  std::vector<std::string> fixed;        // named human-side units placed by the map that stay as placed (not part of the choice)
  std::vector<PrepShop> shops;           // the preparation shops: the single `shop` is one entry with no id; `shops` lists several
  // The first shop's own fields, kept so that code that knows only one shop reads them unchanged.
  bool hasShop = false;                  // a preparation shop
  std::vector<int> shop;                 // item indices it sells (docs/ITEMS.md "Shops"); every one has a price
  bool buys = true;                      // the shop also buys the party's items (shop "buys"; default true)
  int side = -1;                         // map side the deployed characters join (a human side)
};

}  // namespace ote
