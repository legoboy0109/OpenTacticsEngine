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
#include "core/data/speed_rules.h"
#include "core/data/jobs.h"
#include "core/data/unit_pools.h"
#include "core/data/weapon_area.h"
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

// One line of dialogue as a character's data writes it (a "say" command): support conversations and quotes. The
// numbers are indices into the owner's text table (SupportData::texts, QuoteData::texts).
struct SayLine {
  int text = -1;
  std::string speaker;   // a character id ("" = none)
  int speakerName = -1;  // free speaker name (-1 = none)
  int portrait = -1;     // portrait id (-1 = none)
};
using SupportLine = SayLine;
using QuoteLine = SayLine;

// Battle and defeat quotes (docs/EVENTS.md "Battle and defeat quotes"): data on a character.
enum class QuoteOnce { Map, Campaign, Always };

struct BattleQuote {
  std::string againstId;  // "" = against anyone
  int against = -1;       // index into Database::characters (resolved by the loader), -1 = anyone
  QuoteOnce once = QuoteOnce::Map;
  std::vector<QuoteLine> lines;
  int flat = -1;          // position among all battle quotes of the game (the key of the "already said" state)
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
  std::vector<int> resist;        // per damage type: this character's own resistance, kResistUnset = the unit type's value; empty = none written
  std::vector<int> capAdjust;     // per stat index: added to the cap the class / game gives (may be negative)
  // Quotes (docs/EVENTS.md "Battle and defeat quotes"); empty = none.
  std::vector<BattleQuote> battleQuotes;
  std::vector<QuoteLine> fallsLines, retreatsLines;  // said when the character is struck down (for good / retreating)
  // Free-form labels (docs/SRPG.md "Tags"): a character matches `unit` / `near.match` / aura `match` keys by them like its type's tags.
  std::vector<std::string> tags;
  // "promotion": { "fromLevel": n }: the lowest level of every promotion of this character (0 = the promotion entries' own).
  int promotionFromLevel = 0;
  // Jobs (docs/JOBS.md): the job it starts in and its starting progress; `jobs.job` < 0 = this character does not use the job system. Unit type
  // `type` above is replaced by the job's when it has one.
  JobState jobs;
};

// The wording of all quotes (Battle::storyText, from kQuoteTextBase) and how many battle quotes there are.
struct QuoteData {
  std::vector<std::string> texts;
  int count = 0;
};

// Text indices at or above this are looked up in QuoteData::texts (Dialogue events of a quote).
const int kQuoteTextBase = 1 << 25;

// ---- 3. skills (data/skills.json) ----

enum class SkillWhen { Always, TurnStart, Combat, Strike, Struck, Defeat, AfterCombat,
                       StaffUse /* after the unit used a staff (docs/SRPG.md "A staff used") */ };

// One entry of a block's "if" (also of an ability's "if"). Which fields count depends on `kind`.
struct SkillCond {
  enum class Kind { Hp, Near, Weapon, Terrain, Structure, Unit, Carrying, Round, Initiating, Distance, Formula,
                    Condition,  // Condition: the unit has the condition with id `key` (docs/SRPG.md section 9)
                    // Skill blocks, general (docs/SRPG.md "More blocks"): WeaponKind: the weapon's `damageKind` is `key`;
                    // Turn: the battle's turn number fits `turnMode`.
                    WeaponKind, Turn,
                    // Counterattack: the fight has a counterattack (strike / struck blocks); Carries: `key` is an item the unit carries;
                    // Inventory: the number of items carried (`op`, `value`), or full / not full (`flag`, `value` = -1).
                    Counterattack, Carries, Inventory,
                    // Strike: which strike of the fight (strike / struck blocks; `op`, `value`; "first" = 1, "followUp" = 2 or more).
                    Strike,
                    // Marked: the owner (or its foe with "who") has the mark `key` in the fight now running; `flag` false = does not (docs/SRPG.md "Linking two strike blocks").
                    Marked,
                    // DamageType: the weapon's declared damage type (game.json "damageTypes") is `key` (docs/SRPG.md "Damage types").
                    DamageType,
                    // Arc: the owner stands in the foe's front / side / back arc (`value` = Arc: 0 front, 1 side, 2 back; with "who": "foe" the foe stands in the owner's
                    // arc). Needs rules.facing and a combat (docs/SRPG.md "Facing").
                    Arc };
  enum class TurnMode { Odd, Even, Range, Every };
  Kind kind = Kind::Hp;
  bool foe = false;       // "who": "foe" (an ability's condition: the target unit)
  CmpOp op = CmpOp::Ge;   // Hp, Near, Round, Distance
  int value = 0;          // Hp: percent; Near: count; Round / Distance: the number
  int within = 1;         // Near: steps (1..10)
  bool enemy = false;     // Near: false = "ally", true = "enemy"
  bool flag = true;       // Structure (true = any structure when `key` is empty), Carrying, Initiating
  std::vector<std::string> keys;  // Terrain: every id or tag it may be (the list form; `key` is the first)
  std::string key;        // Near: "match" (a unit type id or tag, "" = any); Weapon: weapon id or group;
                          // Terrain: terrain id or tag; Structure: structure id or tag; Unit: unit type id or tag
  int expr = -1;          // Formula: index into Database::exprs
  TurnMode turnMode = TurnMode::Odd;  // Turn
  int turnA = 1, turnB = 1000000;     // Turn: Range = from..to; Every = every `turnA` with remainder `turnB`
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
    CannotAct, CannotMove, Forbid, Berserk, Immune, Inflict,
    // General blocks (docs/SRPG.md "More skill blocks"). FollowUp: value 1 force / -1 forbid the unit's follow-up strike;
    // FollowShift: a number the strike formulas read as `a.followShift`; TrianglePercent: read as `a.trianglePercent`.
    FollowUp, FollowShift, TrianglePercent, WeaponXpPercent, NoCarryPenalty, CancelEffective,
    Cure,  // timed blocks: the owner is cured of the conditions in `names` / `conds`, or of all (`cureAll`)
    // Second set (docs/SRPG.md "Skill and ability blocks, second set"). NullifySkills (combat): the foe's skills do not apply in
    // this fight; `value` 0 = all, 1 = the skills `skillIds` (`names` as written), 2 = the skills with the tag `names[0]`.
    // StaffRange (always): `value` added to the reach of staves (only those of the group `names[0]` when it is set).
    // CarryBonus (always): `value` is read by the canCarry formula as `a.carryBonus`.
    NullifySkills, StaffRange, CarryBonus,
    // Third set (docs/SRPG.md "Skill blocks, third set"). Recoil (strike blocks): the wielder is hurt; `expr` >= 0 = a formula amount (`dealt` = the
    // damage of the strike), else `value`; `instead`: the strike's damage goes to the wielder, not to the foe.
    // CancelEffective may now list effectiveness tags (`names`); empty = all.
    Recoil,
    Vision,  // standing (fog): `value` (-10..10) is added to the unit's vision (docs/ECONOMY.md "Fog of war")
    // strike / struck blocks: when the block takes effect, the owner gets the mark `names[0]` until the fight (and its afterCombat blocks) is over.
    Mark
  };
  enum class To { Self, Foe, Allies, Enemies };
  Kind kind = Kind::Stats;
  To to = To::Self;
  int value = 0;            // the effect's number (hit 15, damagePercent 150, heal 5, maxMoveCost 1, ...)
  int valueMax = 0;         // Hurt with "amount": [lo, hi]: `value` is lo and this is hi (> lo); the battle's random numbers pick one each time. 0 = a plain number
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
  // A combat number written as a formula (docs/SRPG.md "Formula-sized bonuses"): `expr` >= 0 replaces `value` for Hit, Crit,
  // Damage, Avoid, CritAvoid, DamageTaken, DamagePercent, DamageTakenPercent; `statExprs[i]` >= 0 replaces `stats[i]` of Stats.
  std::vector<int> statExprs;
  bool cureAll = false;     // Cure: every condition
  int cureKind = 0;         // Cure: "harmful" (1) / "helpful" (2): every condition of that kind (expanded into `conds` when the block is resolved)
  bool instead = false;     // Recoil: the strike's damage hits the wielder instead of the foe
  std::vector<int> skillIds;  // NullifySkills (mode 1): indices into Database::skills, resolved last
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
  std::string auraMatch;             // aura: only units whose type or character has this id / tag receive it ("" = all)
  std::string auraWeapon;            // aura: only a receiver whose weapon in this fight has this weapon id or group ("" = any)
  std::string auraWeaponKind;        // aura: only a receiver whose weapon in this fight has this damage kind ("" = any)
  int auraTest = -1;                 // aura: a formula the receiver must satisfy (a = the owner, b = the receiver); -1 = none
  std::vector<SkillEffect> effects;  // "do": at least one
};

struct SkillDef {
  std::string id, name, description;
  std::vector<std::string> tags;  // the maker's labels ("nullifySkills" can name one)
  std::vector<SkillBlock> blocks;
  // Not a skill of the maker's: the blocks of a condition, or what a weapon's "inflict" key turned into. Never shown in a
  // unit's skill list; only the unit's conditions / the weapon reach it (docs/SRPG.md section 9).
  bool hidden = false;
};

// ---- 4. menu abilities and summoning (data/abilities.json) ----

enum class AbilityTarget { Self, Ally, Enemy, Unit, Cell };
// Steal, Refresh, MovePast, Cure: docs/SRPG.md "Skill and ability blocks, second set".
enum class AbilityEffect { Push, Pull, Swap, MoveBehind, Heal, Summon, Unlock /* docs/SRPG.md section 10 */, Steal, Refresh, MovePast, Cure, None /* only "inflict" */,
                           Strike /* attacks with a weapon (docs/SRPG.md "Abilities that strike") */,
                           Revive /* stands a downed unit up (docs/SRPG.md "Downed units and revive"); HP in amount / healPercent / healExpr as a heal */ };

// One candidate of a summon's "items" (docs/SRPG.md "Summoning"): the item is carried when the summoner's level is at least `minLevel`.
struct SummonItem {
  std::string id;
  int item = -1;      // index into Database::items (resolved last)
  int minLevel = 1;
  int maxLevel = 0;   // "maxLevel": the candidate fits only up to this summoner level (0 = no ceiling)
  int weight = 1;     // "weight" 1..1000: its share of the roll when the summon picks at random
  bool nothing = false;  // { "item": null }: the summon carries no item at all
};

struct SummonDef {
  std::string typeId;
  int type = -1;         // index into Database::unitTypes
  int level = 0;         // 0 = "summoner" (the summoner's level), else a fixed level
  int limit = 1;         // living summons of this ability by this summoner (1..20)
  bool vanish = true;    // they leave when the summoner is defeated or removed
  int lifetime = 0;      // turns of its side; 0 = no limit
  bool canAct = false;   // false = it arrives with its turn finished
  bool givesXp = true;   // false = defeating or hitting it gives no XP
  std::vector<SummonItem> items;  // "items": candidates for what the summon carries (empty = the unit type's own items)
  bool pickRandom = false;        // "pick": "random" (one roll of the battle's random numbers when more than one candidate fits); false = "highest" (the highest minLevel that fits)
  bool replace = false;           // "replace": at its limit a new summon makes the oldest one vanish instead of being refused
};

// One condition an ability puts on (docs/SRPG.md "Areas and several effects for abilities"): what the single "inflict" object always said.
struct AbilityInflict {
  int cond = -1;        // index into Database::conditions
  int turns = 0;        // 0 = the condition's own
  int chance = 0;       // 1..99; 0 = no roll (no "chance", or 100)
  int chanceExpr = -1;  // a formula (a = the user, b = the target); -1 = none
  bool rolls() const { return chance > 0 || chanceExpr >= 0; }
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
  // Unlock (the thief's trait, docs/SRPG.md section 10): which kinds of structure it opens without a key, and the optional
  // lockpick item it needs (one use is spent each time); target is a square of the door / chest.
  bool unlockDoors = false, unlockChests = false;
  std::string unlockNeedsId;       // as written
  int unlockNeeds = -1;            // index into Database::items (resolved last)
  bool afterMove = true;           // false = only before moving
  std::vector<SkillCond> conds;    // "if"; `foe` = the target unit
  // Steal: the maker's condition (a = the user, b = the foe; -1 = always) and which items may be taken.
  int stealIf = -1;
  bool stealIfItem = false;  // the formula reads `it` (the item): judged for each item, not for the foe as a whole
  std::string stealWhy;      // the plain reason an item the formula rules out is greyed with
  bool stealWeapons = false, stealEquipped = false;
  // Refresh: how many times a unit can be refreshed per turn (by anyone, with any such ability).
  int refreshPerTurn = 1;
  // Cure: the conditions it cures (`cureIds` as written, `cures` resolved last) or every one.
  std::vector<std::string> cureIds;
  std::vector<int> cures;
  bool cureAll = false;
  int cureKind = 0;  // "cure": "harmful" (1) / "helpful" (2): `cures` holds every condition of that kind (docs/SRPG.md "Ability conditions")
  // "xp": experience the user gets for each use (a number or a formula; expression -1 = the number). "inflict": a condition the
  // target gets after the effect (docs/SRPG.md "Abilities: experience and conditions").
  int xp = 0, xpExpr = -1;
  int inflictCond = -1, inflictTurns = 0;  // index into Database::conditions (conditions load first); turns 0 = the condition's own
  // "inflict": { "chance": 60 | "formula" } (docs/SRPG.md "Ability conditions"): the percent it takes hold on each target, rolled once per target.
  // inflictChance 0 and inflictChanceExpr -1 = no "chance" key: it always takes hold (no roll), as before.
  int inflictChance = 0, inflictChanceExpr = -1;
  ChargeDef charge;         // "chargeSpeed" etc. (speed turn order only; docs/TURN_ORDER.md "Charged actions")
  bool spendsItem = false;  // "spendsItem": each use takes one use of the item that gives the ability (docs/SRPG.md "An ability that spends its item")
  // Strike (docs/SRPG.md "Abilities that strike"): the ability attacks with this weapon (index into Database::weapons, resolved last; -1 = not a strike).
  std::string strikeId;
  int strike = -1;
  // "effect": { "strike": ..., "reach": "equipped" }: the strike reaches as far as the weapon in the user's hand (its min and max range) instead of the
  // strike weapon's own range; a unit with no weapon in hand keeps the strike weapon's (docs/SRPG.md "Abilities that strike").
  bool strikeReachEquipped = false;
  // Heal by a share of the target's maximum (1..100) or by a formula (index into Database::exprs); both 0 / -1 = `amount` (docs/SRPG.md "Healing by formula").
  int healPercent = 0;
  int healExpr = -1;
  // "spend": what one use costs, unit resources and side resources (docs/SRPG.md "Unit resources").
  std::vector<AbilitySpend> spend;
  // docs/SRPG.md "Areas and several effects for abilities":
  // Every "inflict" entry in list order (a single object = one entry). inflictCond / inflictTurns / inflictChance / inflictChanceExpr above
  // are always the FIRST entry (kept for the code that reads one); the rules and the AI walk this list.
  std::vector<AbilityInflict> inflicts;
  // "effect": { "heal": ..., "cure": ... }: effect stays Heal and the cure fields above (cureIds, cures, cureAll, cureKind) are filled too;
  // the heal goes first, then the cure.
  bool healCures = false;
  // "area" (heal, cure, both, or only "inflict"): the ability aims at a square in range and acts on every unit in the area its "target"
  // accepts. shape / radius / length / vertical as on a weapon; friendlyFire and apExtra are never set (the target decides who is affected).
  AreaDef area;
  // With an area "range" may start at 0 (the user's own square); minRange is then 0.
  // Is it aimed at a square (a summon, an unlock, an area) rather than at a unit? (Strikes with an area weapon have target Cell.)
  bool aimsSquare() const { return target == AbilityTarget::Cell || area.on(); }
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
  // "fromLevelFor": { "<character id>": n }: this entry's lowest level for those characters (the ids as written, then as indices
  // into Database::characters once the characters are loaded). Wins over CharacterDef::promotionFromLevel.
  std::vector<std::pair<std::string, int>> fromLevelForIds;
  std::vector<std::pair<int, int>> fromLevelFor;
};

struct PromotionRules {
  bool keepLevel = false;  // "levelAfter": "keep" (default "reset": level 1, xp 0 after promoting)
  std::string testPromotedTag;  // "testPromotedTag": a unit type with this tag also counts as already promoted for the test level; "" = none
  int testAt = 0;  // "testAt": the level at which the test level promotes a unit that can promote (docs/DIFFICULTY.md "Test level"); 0 = never
  bool replaceClassSkills = false;  // "classSkills": "replace": skills the old class taught go away when promoting (default "keep")
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
  // "scale": "turnsLeft" (docs/SRPG.md "Effects that fade"): the numbers of its "always" blocks (stats, growths, vision, staffRange,
  // carryBonus) are multiplied by the turns it has left. Counted conditions only.
  bool scale = false;
  bool endsWhenStruck = false;       // a strike that does damage to the unit ends it
  // "kind": "harmful" (the default) or "helpful" (docs/SRPG.md "Ability conditions"): what a cure list "harmful" / "helpful" takes away,
  // and which side the computer gives it to.
  bool helpful = false;
  std::vector<std::string> replaceIds;
  std::vector<int> replaces;         // applying this removes these
  int skill = -1;                    // the hidden SkillDef (index into Database::skills) that holds its blocks
  // "autoRevive" (docs/SRPG.md "Downed units and revive"): when the unit would be downed it stands up at once with this many HP and the
  // condition ends. Off when !on(). Needs rules.downed.
  ReviveAmount autoRevive;
};

}  // namespace ote
