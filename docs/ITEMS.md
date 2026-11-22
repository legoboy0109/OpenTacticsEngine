# Items

Contract for items: things units carry. Owner decisions (2026-11-14):

- **Weapons: the maker chooses per game.** Either weapons stay fixed per
  unit type (as before; Advance Wars-like), or weapons are items each unit
  carries and equips (Fire Emblem-like).
- First version has **usable items**, **carried weapons**, **accessories**
  and **limited uses** (durability; unlimited possible per item).
- Items are kept in a **per-unit inventory** plus one **shared storage per
  side**.
- Units get items as **starting items**, by **trading** with a neighbour of
  their side, from **shops** (structures that sell for the game's
  resources) and as **drops** from defeated enemies.

Everything below those four points is engine design and may change; wording
shown to players and makers is placeholder until the owner approves it.

Status: see "Build order" at the end.

## Data

### game.json `rules.items` (optional; missing = the game has no items)

```jsonc
"items": {
  "weapons": "fixed",        // "fixed" (default): a unit type's weapons, as before.
                             // "carried": weapons are items in the inventory.
  "inventorySize": 5,        // items one unit can carry, 1..20. Default 5.
  "sellPercent": 50,         // 0..100, default 50: what a shop that buys items back pays, as a share of the price
                             // (scaled by uses left: docs/EVENTS.md "Preparation", "Selling"). In-battle shops do not buy.
  "freeBattleStorage": { "each": 3 },  // optional: what the storage holds when a FREE battle starts (docs/GAME_FORMAT.md "Free battles")
  "drops": "marked",         // "marked" (default): a defeated unit gives every item marked `drop`; "last": see "Drops" below
  "storage": "anywhere"      // the side's shared storage: "off" (default), "anywhere",
                             // "structure" (standing on a structure of its own side),
                             // "leader" (the unit is a leader or stands next to one of its side).
}
```
Core: `RulesConfig::items` (`ItemRules`).

### data/items.json (optional; missing file = no items)

```jsonc
{ "items": [
  { "id": "salve", "name": "Salve",
    "description": "Restores 10 HP.",       // optional, one line
    "kind": "usable",                       // "usable" | "weapon" | "accessory"
    "uses": 3,                              // optional, 1..999; missing = unlimited
    "price": { "gold": 300 },               // optional; missing = shops cannot sell it (and none buys it back)
    "sellable": false,                      // optional, default true: a shop that buys items never takes this one
    "target": "self",                       // usable: "self" (default) or "ally" (a unit of the
                                            // same team on a neighbouring square)
    "heal": 10,                             // usable: HP restored; "full" = all of it
    "raise": { "strength": 2 }              // usable: stats raised for good
  },
  { "id": "iron_sword", "name": "Iron Sword", "kind": "weapon",
    "weapon": "iron_sword",                 // id in data/weapons.json: range, numbers, formulas
    "uses": 40 },
  { "id": "shield_ring", "name": "Shield Ring", "kind": "accessory",
    "bonus": { "defense": 2 } }             // added to the stats of whoever carries it
] }
```
A usable item may light the square its user stands on in fog (a torch): `"light": { "radius": 3, "turns": 2, "shrink": 1 }` (docs/ECONOMY.md "Fog options"); it
may have nothing else, and its `target` must be `self`.

Rules the loader enforces (each with a message naming the item):
- ids unique; `kind` one of the three; a usable item has `heal` and/or
  `raise`; a weapon item names an existing weapon; an accessory has a
  non-empty `bonus`; stat and resource ids exist; amounts are whole numbers.
- `"kind": "weapon"` needs `rules.items.weapons` = `"carried"`.
- `raise` / `bonus` may name any stat, also the HP and move stats.

Core: `ItemDef`, `Database::items`, `Database::findItem`.

### Who can wield what (carried weapons only)

A unit type can wield a weapon if the weapon is in the type's `weapons`
list (as before: now "can use", not "has"), or if the weapon's optional
`"group"` (weapons.json, e.g. `"sword"`) is in the type's optional
`"wields"` list (units.json). Core: `WeaponDef::group`,
`UnitTypeDef::wields`, `Database::canWield(unitType, weapon, character)`. A character (data/characters.json) may also list
`"wields": ["bow"]`: extra groups for that person only, on top of the class's (docs/SRPG.md section 7).

A weapon can also be limited to named people with weapons.json `"wieldedBy": ["kell"]` (characters, docs/SRPG.md "Character-locked
weapons"): the class test above must still pass, then the unit's character must be on the list. The weapon can be carried, traded
and stored by anyone; it is greyed ("Only Kell can wield this") for everyone else. Core: `WeaponDef::wielders`,
`Battle::meetsCharacterLock`. `"wieldedByTypes": ["knight"]` limits it to unit types the same way; with both keys a unit passes by either.

### Starting items

- units.json, per unit type: `"items": ["iron_sword", "salve"]`: what a
  unit of this type starts with (built units, and map units that say
  nothing).
- Map file, per unit: `"items": [...]` replaces the type's list. An entry is
  an id or `{ "item": "salve", "uses": 2, "drop": true }` (`uses`: what
  is left; `drop`: goes to whoever defeats the unit).
- Map file, per side: `"storage": [...]` (same entries, no `drop`).
- More entries than `inventorySize` is a loader error.

### Shops

structures.json: `"sells": ["salve", "iron_sword"]`. Every listed item
needs a `price`. Core: `StructureDef::sells`.

## State (Battle)

```cpp
struct ItemStack { int item; int uses; bool drop; };   // uses: left; -1 = unlimited
Unit::items            // the inventory, in order; at most inventorySize
SideState::storage     // the shared storage, no size limit
```
In a game with carried weapons the **equipped weapon** is the first item in
the inventory that is a weapon the unit can wield. Equipping moves an item
to the front. Both are saved (docs/SAVES.md); the save hash covers
items.json and `rules.items`.

## Rules

**Stats.** `Battle::stat(unit, index)` = the unit's own stat plus the
`bonus` of every different accessory it carries (two of the same item count
once). Everything reads stats through it: formulas (`a.defense`), maximum
HP, movement. HP above a lowered maximum is cut down to it.

**Weapons** (carried): `attackOptions` and `forecast` offer every wieldable
weapon item in the attacker's inventory; attacking with one equips it. The
defender answers with its equipped weapon only. Each strike made (hit or
miss) spends one use; at 0 the item breaks and is gone, and the rest of that
fight goes on without it. With fixed weapons nothing changes.

**Actions** (all need the acting unit to be able to act; `slot` = position
in the inventory):

| Action | What | Ends the unit's turn |
|---|---|---|
| `UseItem(unit, slot, target)` | a usable item on itself or a neighbouring ally; spends a use | yes |
| `Equip(unit, slot)` | moves a wieldable weapon to the front | no |
| `GiveItem(unit, slot, target)` | to a neighbouring unit of the same side with room | no |
| `TakeItem(unit, target, slot)` | from a neighbouring unit of the same side (its slot) | no |
| `StoreItem(unit, slot)` | into the side's storage | no |
| `FetchItem(unit, index)` | out of the side's storage (index into it) | no |
| `BuyItem(unit, item)` | on a square of a structure that sells it and is not owned by an enemy team; pays the price | no |
| `DiscardItem(unit, slot)` | throws it away | no |

The ones that do not end the turn can be repeated, and the unit can still
attack or wait afterwards; they cannot be undone (the player's "undo move"
is dropped after one). Using an item that would do nothing (full HP and no
`raise`) is refused. Storage actions need `rules.items.storage` and the
unit to be where that setting says.

**Getting an item without room** (buying, drops): it goes to the side's
storage if the game has one (wherever the unit is); otherwise buying is
refused and a drop is lost.

**Drops.** When a unit is defeated by an enemy unit's strike, its items
marked `drop` go to that unit, in order. With `rules.items` `"drops": "last"`
(the rule of the classic strategy RPGs) a defeated unit that has **any** item
marked gives **one** item instead: its last item that is not the weapon in its
hand (marked or not; when the last item is that weapon, the one before it).
What a computer unit loots from a chest or steals is added at the end of its
items and marked, so under "last" that is what it drops. Code:
`Battle::dropsOf` (items.cpp); tests `tests/test_ai_loot.cpp`.

**Events** (what the player animates): `ItemUsed` (unit, item, target),
`UnitHealed` (unit, amount), `StatsRaised` (unit; data = gain per stat),
`ItemBroke` (unit, item), `ItemGained` (unit or -1 = storage, item, side,
flag "dropped"), `ItemLost` (unit, item), `ItemMoved` (from unit / -1, to
unit / -1, item; trades and storage), plus the existing `ResourceChanged`
for a purchase.

**AI** (first version): attacks with what it carries; a unit below half HP
with a healing item and no kill available uses it. It does not trade, shop,
or use the storage.

## Player

Action menu (after moving, as today) gains, when they apply: **Items** (the
inventory; choosing one offers Use / Equip / Store / Discard), **Trade**
(a neighbour of the same side: both inventories side by side, choosing an
item hands it over), **Storage** (take from the side's storage), **Shop**
(buy). The info panel lists what the unit under the cursor carries, with
uses left; the equipped weapon is marked. With carried weapons, targeting
cycles through weapon and target as before; the forecast names the weapon
and its uses left.

**Stacked storage lists.** Wherever the shared storage is listed (the battle's Storage panel, the storage column of the
preparation trade panel, the preparation shop's Sell list for the storage) identical copies are ONE row with a count,
`Potion  x3`; a single copy shows no count. Identical = same item, same uses left and same `drop` flag, so a half-used copy is its
own row (`Potion  x3  2/2` beside `Potion  1/2`). Rows run in the game's item order (the order of `items.json`), then fuller copies
first (unlimited, then most uses left), then non-drop before drop. Choosing a row acts on ONE copy: the first (lowest position) of
the row's identical copies in the storage; the count goes down and the row disappears at 0. Giving an item to storage joins the
row that matches it. This is only how the lists are drawn: the storage itself (`SideState::storage`, a campaign's carried storage,
the saves) still holds one entry per copy, in the same order as before, so saves are unchanged. A unit's own inventory is never
stacked (its slots are individual). The count is never cut when a row is narrow: the name is shortened first
(`fitNameCount`, engine/player/ui/text.h) and the trade panel widens, up to the screen, when a row needs it (big UI scales).
Code: engine/player/battle/storage_stack.h (`stackStorage`).

## Strategy-RPG additions

Accessories can carry skills, a usable item can be a promotion item (`promotes`), a weapon can give skills while wielded, and a character
can start with its own items instead of its class's. These keys, their checks and the items of the demo and template are in
docs/SRPG.md (sections 1 to 3 and "Demo and template content").

## Staves

An item of `"kind": "staff"` is a staff: a unit **wields** it by group and rank like a weapon (`"group"`, `"rank"`, `"wieldedBy"`, `"wieldedByTypes"`, the unit
type's `"wields"`) and **uses it on another unit at range** as its action (`"range"`, `"target"`, `"area"`, `"effects"` such as a heal, an
optional `"hit"` chance, `"xp"`). Its uses, price, trading, storage, drops and saves are the item's own (everything above). Stages 1
(healing) and 2 (`warp`, `pull`, `swap`, `repair`, `light`) are built; the whole contract, including stage 3, is docs/SRPG.md section 8.
A staff's `repair` skips an item with `"repairable": false` (a key of any item that has `uses`; default true) and items with unlimited uses. It is not a `usable` item: the Items
screen offers no Use for it; the action menu has a Staff entry.

## Keys

A usable item with `"opens": ["door"]`, `["chest"]`, both and/or structure kind ids (`["strongbox"]`) is a **key**: used next to a locked door or chest
(the unit's menu: Open; with several fitting keys the first is the default), it opens it and spends one of its `uses` (it breaks at 0; no `uses` =
unlimited). A structure kind may list the `"keys"` that fit it. A chest's contents go to the opener's inventory, else the shared storage; with neither the
chest stays shut until the unit drops something. Specified in docs/SRPG.md section 10 "Doors, keys, chests and visiting". The staff effect `unlock`
and the thief's unlock ability are described there too.

## Status conditions

A usable item can **cure** conditions (`"cure": ["poison"]` or `"cure": "all"`) and/or **inflict** one on the unit it is used on (`"inflict": { "condition":
"curse", "turns": 2 }`), alone or together with `heal` / `raise`; the order is raise, heal, cure, inflict. An item that would change nothing (no condition to
cure, immune, has it already) is not offered. A weapon can inflict a condition on the unit it hits (`"inflict": { "condition": "poison", "chance": 30 }`, in
`data/weapons.json`), and a condition can forbid a weapon group, staves or usable items while it lasts (silence). The contract, the loader refusals and the
starter set (an Antidote is sold at the demo's forts) are in docs/SRPG.md section 9.

## Weapon ranks

A weapon item can require a **rank** in its group (`"rank"` on the weapon, `"ranks"` on unit types, characters and map units, the ladder in
`data/ranks.json`; docs/SRPG.md section 7). A unit that does not have the rank can carry, trade, store and buy the weapon but cannot
equip or attack with it: `Battle::weaponsOf`, `weaponSlot`, `equippedSlot` and the `Equip` action all use `Battle::canWieldNow`
(the class can wield it **and** the rank is high enough), the refusal reads "Needs rank B", and the computer opponent never picks it.
Stat-raising items (`raise`) respect stat caps (docs/SRPG.md section 7): a raise is cut at the cap, and one that could change nothing is
refused like a heal at full HP.

## Editor

- **Items** screen (list, add, rename, delete with "used by" check, like
  Weapons): kind, uses, price, and the fields of the kind.
- **Rules**: weapons fixed or carried, inventory size, storage setting.
- **Weapons**: group. **Units**: wields, starting items. **Structures**:
  what it sells.
- **Map editor**: a placed unit's items (uses left, drop) and each side's
  starting storage.

## Not in this version

Selling items; chests and items lying on the map; stealing; item effects by
formula or script (status effects, damage items, staves at range); ranks for
fixed weapons (carried weapons have them, see above); forging/repair; a preparations screen before battle; carrying items
from one map to the next (needs the campaign layer, PLAN 3f); AI that
trades or shops.

## Build order

1. Core: data, loader, state, rules, saves, tests. 
2. Player menus and panel; AI's simple use.
3. Editor screens; demo content (`demo-srpg`: carried weapons, a salve,
   a shop, a drop).
