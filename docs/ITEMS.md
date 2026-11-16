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
    "price": { "gold": 300 },               // optional; missing = shops cannot sell it
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
`UnitTypeDef::wields`, `Database::canWield(unitType, weapon)`.

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
marked `drop` go to that unit, in order.

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

## Strategy-RPG additions

Accessories can carry skills, a usable item can be a promotion item (`promotes`), a weapon can give skills while wielded, and a character
can start with its own items instead of its class's. These keys, their checks and the items of the demo and template are in
docs/SRPG.md (sections 1 to 3 and "Demo and template content").

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
formula or script (status effects, damage items, staves at range); weapon
ranks; forging/repair; a preparations screen before battle; carrying items
from one map to the next (needs the campaign layer, PLAN 3f); AI that
trades or shops.

## Build order

1. Core: data, loader, state, rules, saves, tests. 
2. Player menus and panel; AI's simple use.
3. Editor screens; demo content (`demo-srpg`: carried weapons, a salve,
   a shop, a drop).
