// Data of the strategy-RPG features (contract: docs/SRPG.md): characters, skills, menu abilities and summons,
// promotion, rescue / pair up, supports. Plain structs filled by the loaders (core/data/loader_*.cpp of each
// area) and only read by Battle. Included by database.h, so nothing here may include it.
//
// Cross references: where a file may name something that is loaded later (a promotion's unit type, a summon's
// unit type, a skill's ability), the loader keeps the id as written (`...Id`) and resolveSrpgRefs
// (loader_srpg_refs.cpp) fills the index next to it once everything is loaded. An index of -1 = not resolved.
//
// APPEND ONLY for the steps after F1: add fields and types at the end of a struct / of this file; do not rename
// or reorder what is here (other steps are built against it).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace ote {

struct ItemGrant;  // database.h

// Comparisons used by story conditions and skill conditions: = != < <= > >=.
enum class CmpOp { Eq, Ne, Lt, Le, Gt, Ge };

// ---- limits (the loaders' refusals name them) ----
const int kMaxCharacters = 500;
const int kMaxSkills = 500;
const int kMaxSkillBlocks = 8;
const int kMaxBlockConditions = 8;
const int kMaxBlockEffects = 8;
const int kMaxAbilities = 200;
const int kMaxSupportPairs = 500;
const int kMaxSupportLevels = 5;
const int kMaxSupportLines = 200;
const int kMaxPromotions = 8;
const int kMaxSupportPoints = 1000000;
const int kMaxConditions = 100;  // data/conditions.json (section 9)
const int kMaxConditionTurns = 99;

// ---- expressions (docs/SRPG.md "Formulas in this document") ----
// One Lua expression in the formula language, registered by a loader (Database::addExpr) and compiled by
// Formulas::init. `label` is what a syntax error and the run-time log name it by, e.g.
// `data/skills.json: skill "heavy_blow": block 1: "chance"`.
struct ExprDef {
  std::string text;
  std::string label;
};

// ---- 1. characters (data/characters.json) ----

// A skill learned for good at a level (unit types and characters).
struct LearnDef {
  int level = 2;
  std::string skillId;
  int skill = -1;  // index into Database::skills
};

struct CharacterDef {
  std::string id, name, description;
  int type = -1;                  // index into Database::unitTypes: the unit type it starts as
  int level = 1;                  // the level it joins at
  std::vector<int> stats;         // per stat index, ADDED to the unit type's base stats (may be negative)
  std::vector<int> growths;       // per stat index, ADDED to the unit type's growths (% points)
  std::vector<std::string> skillIds;
  std::vector<int> skills;        // personal skills (indices into Database::skills)
  std::vector<LearnDef> learns;   // personal skills learned at a level
  std::vector<std::string> abilityIds;
  std::vector<int> abilities;     // personal menu abilities (indices into Database::abilities)
  bool hasItems = false;          // "items" was written: replaces the unit type's starting items
  std::vector<ItemGrant> items;
  // Weapon ranks and stat caps (docs/SRPG.md section 7); empty when the character says nothing.
  std::vector<std::string> wields;  // extra weapon groups it can wield beyond its class's (docs/SRPG.md section 7); only ever adds
  std::vector<int> rankStart;     // per RankRules::groups index: personal weapon experience (the higher of it and the class's wins)
  std::vector<int> capAdjust;     // per stat index: added to the cap the class / game gives (may be negative)
};

// ---- 3. skills (data/skills.json) ----

enum class SkillWhen { Always, TurnStart, Combat, Strike, Struck, Defeat, AfterCombat };

// One entry of a block's "if" (also of an ability's "if"). Which fields count depends on `kind`.
struct SkillCond {
  enum class Kind { Hp, Near, Weapon, Terrain, Structure, Unit, Carrying, Round, Initiating, Distance, Formula,
                    Condition };  // Condition: the unit has the condition with id `key` (docs/SRPG.md section 9)
  Kind kind = Kind::Hp;
  bool foe = false;       // "who": "foe" (an ability's condition: the target unit)
  CmpOp op = CmpOp::Ge;   // Hp, Near, Round, Distance
  int value = 0;          // Hp: percent; Near: count; Round / Distance: the number
  int within = 1;         // Near: steps (1..10)
  bool enemy = false;     // Near: false = "ally", true = "enemy"
  bool flag = true;       // Structure (true = any structure when `key` is empty), Carrying, Initiating
  std::string key;        // Near: "match" (a unit type id or tag, "" = any); Weapon: weapon id or group;
                          // Terrain: terrain id or tag; Structure: structure id or tag; Unit: unit type id or tag
  int expr = -1;          // Formula: index into Database::exprs
};

// One entry of a block's "do". Which fields count depends on `kind`.
struct SkillEffect {
  enum class Kind {
    // standing ("always")
    Stats, Growths, MaxMoveCost, Pass, MoveAgain, XpPercent, Ability,
    // combat numbers
    Hit, Crit, Damage, Avoid, CritAvoid, DamageTaken, DamagePercent, DamageTakenPercent, Strikes, MultiStrike,
    StrikeFirst, CounterAnyRange, NoCounter,
    // per strike
    Drain, StrikeAgain, Survive,
    // timed
    Heal, Hurt, ActAgain,
    // conditions (docs/SRPG.md section 9). CannotAct, CannotMove, Forbid and Berserk belong to the blocks of a condition;
    // Immune works in any skill; Inflict is an on-hit effect of "strike" / "struck" blocks (a weapon's "inflict" is one).
    CannotAct, CannotMove, Forbid, Berserk, Immune, Inflict
  };
  enum class To { Self, Foe, Allies, Enemies };
  Kind kind = Kind::Stats;
  To to = To::Self;
  int value = 0;            // the effect's number (hit 15, damagePercent 150, heal 5, maxMoveCost 1, ...)
  bool percent = false;     // Heal: `value` is a percent of the receiver's maximum
  int within = 0;           // Heal "allies" / Hurt "enemies": steps (1..10)
  std::vector<int> stats;   // Stats / Growths: per stat index
  std::string abilityId;    // Ability
  int ability = -1;         // index into Database::abilities
  // Conditions (section 9). Heal / Hurt: `expr` >= 0 = the amount is a formula (a = the receiver); `kill`: a Hurt may defeat.
  int expr = -1;
  bool kill = false;
  // CannotAct: may it still counterattack / still dodge (otherwise it answers nothing and every strike on it hits).
  bool counter = false, dodge = false;
  // Forbid: weapon / staff groups and item kinds it may not use. Immune: condition ids. Inflict: `names[0]` = the condition.
  std::vector<std::string> names, kinds;
  std::vector<int> conds;   // Immune / Inflict: the same as indices into Database::conditions
  int turns = 0;            // Inflict: duration override (0 = the condition's own)
};

struct SkillBlock {
  SkillWhen when = SkillWhen::Always;
  std::vector<SkillCond> conds;      // "if": all must hold
  bool hasChance = false;            // an activation: rolled with Battle::rng when it is judged
  int chance = 0;                    // percent, when chanceExpr < 0
  int chanceExpr = -1;               // a formula (index into Database::exprs)
  bool aura = false;                 // combat blocks only: the effects go to fighters near the owner
  bool auraEnemies = false;          // false = "allies"
  int auraWithin = 1;                // 1..10
  std::vector<SkillEffect> effects;  // "do": at least one
};

struct SkillDef {
  std::string id, name, description;
  std::vector<SkillBlock> blocks;
  // Not a skill of the maker's: the blocks of a condition, or what a weapon's "inflict" key turned into. Never shown in a
  // unit's skill list; only the unit's conditions / the weapon reach it (docs/SRPG.md section 9).
  bool hidden = false;
};

// ---- 4. menu abilities and summoning (data/abilities.json) ----

enum class AbilityTarget { Self, Ally, Enemy, Unit, Cell };
enum class AbilityEffect { Push, Pull, Swap, MoveBehind, Heal, Summon };

struct SummonDef {
  std::string typeId;
  int type = -1;         // index into Database::unitTypes
  int level = 0;         // 0 = "summoner" (the summoner's level), else a fixed level
  int limit = 1;         // living summons of this ability by this summoner (1..20)
  bool vanish = true;    // they leave when the summoner is defeated or removed
  int lifetime = 0;      // turns of its side; 0 = no limit
  bool canAct = false;   // false = it arrives with its turn finished
  bool givesXp = true;   // false = defeating or hitting it gives no XP
};

struct AbilityDef {
  std::string id, name, description;
  AbilityTarget target = AbilityTarget::Unit;
  int minRange = 1, maxRange = 1;  // grid distance; not used with Self
  AbilityEffect effect = AbilityEffect::Push;
  int amount = 0;                  // Push: squares (1..5); Heal: HP, or kHealFull
  SummonDef summon;                // Summon
  bool free = false;               // "cost": "free" (false = "action": ends the unit's turn)
  int uses = 0;                    // times per battle per unit; 0 = unlimited
  int cooldown = 0;                // turns of its side to wait after a use; 0 = none
  bool afterMove = true;           // false = only before moving
  std::vector<SkillCond> conds;    // "if"; `foe` = the target unit
};

// ---- 2. promotion (game.json rules.promotion, units.json "promotions") ----

enum class LevelAfter { Game, Reset, Keep };  // Game = follow rules.promotion.levelAfter
enum class PromoteAtLevel { Off, Offer, Auto };

struct PromotionDef {
  std::string toId;
  int to = -1;                       // index into Database::unitTypes
  int fromLevel = 1;                 // the lowest level at which it can happen
  std::vector<std::string> itemIds;
  std::vector<int> items;            // promotion items that trigger it (indices into Database::items)
  PromoteAtLevel atLevel = PromoteAtLevel::Off;
  LevelAfter levelAfter = LevelAfter::Game;
  bool hasGains = false;             // false = the difference of the two types' base stats
  std::vector<int> gains;            // per stat index
};

struct PromotionRules {
  bool keepLevel = false;  // "levelAfter": "keep" (default "reset": level 1, xp 0 after promoting)
};

// ---- 5. rescue and pair up (game.json rules.rescue) ----

enum class RescueMode { Off, Carry, PairUp };

// How one stat of the unit on the board changes while it has someone: percent first, then the added number.
struct StatChange {
  int percent = 100;  // 0..1000
  int add = 0;
};

struct RescueRules {
  RescueMode mode = RescueMode::Off;
  int canCarry = -1;                     // formula (index into Database::exprs); -1 = always allowed
  std::vector<StatChange> carrierStats;  // per stat index; empty = none
  std::vector<int> pairBonus;            // pairUp: per stat index, what the unit behind adds; empty = none
};

// ---- 6. supports (data/supports.json) ----

// The combat numbers a support level gives (the keys of section 3's combat effects).
struct SupportBonus {
  int hit = 0, avoid = 0, crit = 0, critAvoid = 0, damage = 0, damageTaken = 0;
  std::vector<int> stats;  // per stat index; empty = none
};

// One line of a support conversation (a "say" command). The numbers are indices into SupportData::texts.
struct SupportLine {
  int text = -1;
  std::string speaker;   // a character id ("" = none)
  int speakerName = -1;  // free speaker name (-1 = none)
  int portrait = -1;     // portrait id (-1 = none)
};

struct SupportLevelDef {
  int points = 0;  // closeness needed
  SupportBonus bonus;
  std::vector<SupportLine> talk;  // empty = the level is gained as soon as its points are reached
};

struct SupportPairDef {
  int a = -1, b = -1;  // indices into Database::characters
  int start = 0;       // points at the start of a playthrough
  std::vector<SupportLevelDef> levels;
};

enum class SupportTalks { Battle, Camp, Both };

struct SupportRules {
  std::vector<std::string> levelNames;  // wording, lowest first; empty = "1", "2", ...
  int endTurnAdjacent = 0, fightTogether = 0, heal = 0;  // points per source; 0 = off
  int range = 3;        // "near" for fightTogether and for the bonus (1..10)
  int maxLevels = 0;    // levels one character may hold over all its partners; 0 = no limit
  SupportTalks conversations = SupportTalks::Both;
};

struct SupportData {
  SupportRules rules;
  std::vector<SupportPairDef> pairs;
  std::vector<std::string> texts;  // wording of the conversations (Battle::storyText, from kSupportTextBase)
};

// Text indices at or above this are looked up in SupportData::texts (Dialogue events of a support conversation).
const int kSupportTextBase = 1 << 24;

// ---- 9. conditions (data/conditions.json; docs/SRPG.md section 9) ----

enum class ConditionUntil { Turns, Cured, MapEnd };

struct ConditionDef {
  std::string id, name, description;
  std::string tag;                   // a short text for the map marker (the engine has no small pictures for units)
  ConditionUntil until = ConditionUntil::Turns;
  int turns = 3;                     // Turns: the afflicted unit's own turns it lasts (1..99)
  int wearOff = 0;                   // percent chance each own turn that it ends early (0 = never)
  bool endsWhenStruck = false;       // a strike that does damage to the unit ends it
  std::vector<std::string> replaceIds;
  std::vector<int> replaces;         // applying this removes these
  int skill = -1;                    // the hidden SkillDef (index into Database::skills) that holds its blocks
};

}  // namespace ote
