// Types mirroring docs/GAME_FORMAT.md. Every object carries an index
// signature so keys the UI does not know about survive load -> edit -> save.
import type { AiRulesBlock } from './aiRules'
import type { Behaviour } from './behaviour'
import type { DifficultySet, MapDifficulties } from './difficultyTypes'
import type { Audio } from './audioTypes'
import type { Campaign } from './campaignTypes'
import type { MapPrep } from './prepTypes'
import type { Ability, Caps, Character, Condition, Learn, Promotion, PromotionRules, RankMap, Ranks, RescueRules, Skill, Supports } from './srpgTypes'
import type { Objectives, StoryEvent } from './storyTypes'

export interface Extra { [key: string]: unknown }
export type NumMap = Record<string, number>

export interface Named extends Extra { id: string; name: string }
export interface Roles extends Extra { hp: string; move: string }

export interface FormulaParams { [key: string]: string | number | boolean }
export interface FormulaState extends Extra { preset: string; params?: FormulaParams }
export interface EditorData extends Extra { formulas?: Record<string, FormulaState> }

export interface Feature extends Extra { enabled?: boolean }
export interface Capture extends Feature { amount?: string }
export interface Production extends Feature { builtUnitsCanAct?: boolean }
export interface Progression extends Feature {
  xpPerLevel?: number; maxLevel?: number; xpHit?: string; xpKill?: string
}
/** docs/ITEMS.md `rules.items`; every key is optional and a default is never written. */
export type StorageMode = 'off' | 'anywhere' | 'structure' | 'leader'
/** `sellPercent`: 0 to 100, default 50: what a shop that buys items back pays (docs/EVENTS.md "Preparation"). */
export interface ItemRules extends Extra { weapons?: 'fixed' | 'carried'; inventorySize?: number; storage?: StorageMode; sellPercent?: number; drops?: 'marked' | 'last' }
/** docs/ECONOMY.md `rules.supply`; no block = nothing is repaired or resupplied. */
export interface RepairRules extends Extra { amount?: number; costShare?: number }
export interface SupplyRules extends Extra { repair?: RepairRules; structures?: boolean }
/** docs/ECONOMY.md `rules.research` (needsStructure, default false) and `rules.fog` (ambush, default "endsTurn"). */
export type AmbushRule = 'endsTurn' | 'canAct'
export interface ResearchRules extends Extra { needsStructure?: boolean }
export interface FogRules extends Extra { ambush?: AmbushRule }
/** docs/SRPG.md section 10 `rules.places`: "menu" (default) = explicit Open / Visit entries in the unit's menu, "auto" = it happens when a unit ends a move there. */
export type PlaceActions = 'menu' | 'auto'
export interface PlaceRules extends Extra { actions?: PlaceActions }
/** game.json `unitResources` entry: a pool whose maximum is a stat. */
export interface UnitResource extends Extra { id: string; name?: string; max: string; start?: number; regen?: { perTurn?: number; percent?: number } }
/** rules.abilityPoints (docs/JOBS.md "Learning from equipment"): every key optional, defaults never written. */
export interface AbilityPointRules extends Extra { name?: string; perBattle?: number; perAction?: number; perDefeat?: number }
/** An item's `teaches` entry: exactly one of `ability` and `skill`, and the points that master it. */
export interface Teach extends Extra { ability?: string; skill?: string; points: number }

export interface Rules extends Extra {
  abilityPoints?: AbilityPointRules
  ai?: AiRulesBlock  // docs/AI.md "Scripted-style computer players" (model/aiRules.ts)
  places?: PlaceRules
  items?: ItemRules
  supply?: SupplyRules
  research?: ResearchRules
  fog?: FogRules
  turnOrder?: string
  counterAttacks?: boolean
  defeat?: string[]
  economy?: Feature
  production?: Production
  capture?: Capture
  progression?: Progression
  /** docs/SRPG.md sections 5 and 2; missing = neither carrying nor a promotion rule. */
  rescue?: RescueRules
  promotion?: PromotionRules
}
export interface Formulas extends Extra {
  damage?: string; hit?: string; crit?: string; critMultiplier?: number; strikes?: string
}
export interface View extends Extra { width?: number; height?: number; tileSize?: number }
export interface FontSpec extends Extra { name?: string; file?: string; size?: number; smooth?: boolean }
/** Isometric sizes in pixels (docs/VIEWS.md); each missing key takes its default. */
export interface IsoSizes extends Extra { tileWidth?: number; tileHeight?: number; heightStep?: number; unitWidth?: number }
export interface Style extends Named {
  /** Kind of view: missing = "top-down"; "isometric"; "isometric-3d"; "3d" (full 3D; does not use the isometric measures). */
  view?: string; isometric?: IsoSizes
  background?: string; showGlyphs?: boolean; terrainColors?: Record<string, string>
  /** Placeholder colour of structure blocks in this style (structure id -> colour). */
  structureColors?: Record<string, string>
}
export interface MapRef extends Named { file: string }

export type SavingMode = 'any' | 'continue' | 'off'

/** game.json `grid` (docs/HEX.md): missing = squares. Chosen when the game is made; the editor never changes it. */
export interface GameGrid extends Extra { type?: string; layout?: string }

export interface Game extends Extra {
  grid?: GameGrid
  formatVersion?: number
  name: string
  description?: string
  author?: string
  view?: View
  font?: FontSpec
  stats: Named[]
  roles: Roles
  resources?: Named[]
  /** Pools each unit has for itself, spent by abilities (docs/SRPG.md "Unit resources"; model/unitResources.ts). */
  unitResources?: UnitResource[]
  moveTypes: Named[]
  rules?: Rules
  formulas?: Formulas
  styles?: Style[]
  scripts?: string[]
  maps: MapRef[]
  editor?: EditorData
  /** Who may save (docs/SAVES.md): missing = 'any'. */
  saving?: SavingMode
  /** How fights are shown unless the player chose otherwise (docs/BATTLE_SCENE.md): missing = 'map' (quick fights on the map). */
  battleScene?: 'scene' | 'map'
  /** Whose square decides the battle scene's background and platform (docs/BATTLE_SCENE.md): missing = 'defender'. Which side each fighter stands on: missing = 'map'. */
  battleBackgroundFrom?: 'defender' | 'attacker'
  /** Whether each fighter's platform comes from its own square ('own') or from the square that decides the background ('same', the default). */
  battlePlatformFrom?: 'same' | 'own'
  battleSceneSides?: 'map' | 'playerRight' | 'playerLeft'
  /** A staff used on one other unit plays in the battle scene (docs/BATTLE_SCENE.md "Staves in the scene"): missing = false (staves stay on the map). */
  battleSceneStaves?: boolean
  /** How many battle quotes a fight may have (docs/EVENTS.md "Battle and defeat quotes"): missing = 'both' (each fighter may speak); 'one' = at most one. */
  quotesPerFight?: 'both' | 'one'
  /** An action with nothing to act on in the unit's menu (docs/SRPG.md "Menu options"): missing = 'grey' (it stays, greyed); 'hide' leaves it out. */
  menuUnavailable?: 'grey' | 'hide'
  /** What Attack does first (docs/SRPG.md "Menu options"): missing = 'equipped' (straight to the targets); 'pick' asks which weapon. */
  attackWeaponChoice?: 'equipped' | 'pick'
  /** Story switches (off / on) and variables (whole numbers) that events read and set (docs/EVENTS.md). */
  switches?: Named[]
  variables?: Named[]
  /** The maker's own list of difficulties (docs/DIFFICULTY.md; model/difficulty.ts); missing = the game has none. */
  difficulties?: DifficultySet
}

/** Extra sight for some unit types only (docs/ECONOMY.md "Fog of war"): unit type ids or tags, never empty in a good file. */
export interface VisionLimit { amount: number; units: string[] }
export interface Terrain extends Named {
  color?: string; glyph?: string; moveCost?: NumMap; props?: NumMap; tags?: string[]
  /** Fog of war: extra sight (0 to 10, missing = 0) for a unit standing here (a plain number: every unit; or an
   *  object: those unit types only); a unit here is seen only from next to it. */
  visionBonus?: number | VisionLimit; hides?: boolean
  /** Fog: a unit looking along a line of squares does not see past a square of this terrain (a wall); docs/ECONOMY.md "Fog of war". */
  blocksSight?: boolean
  /** What the terrain costs while it rains / snows, per movement type (docs/ECONOMY.md "Weather and movement cost"); model/battleTables.ts. */
  weatherCost?: { rain?: Record<string, number | 'blocked'>; snow?: Record<string, number | 'blocked'> }
  // Settings that moved to structures (docs/STRUCTURES.md). Only old projects still have them; the Convert flow
  // (model/structuresConvert.ts) moves them. `capture` is the key older games used, `capturePoints` the new name.
  capture?: number; capturePoints?: number; income?: NumMap; produces?: string[]
}
/** A building standing on top of terrain (docs/STRUCTURES.md). It sets only the rules it lists; the rest comes from the ground. */
export interface Structure extends Named {
  color?: string; glyph?: string
  /** Footprint in squares, 1 to 8 (missing = 1). */
  width?: number; height?: number
  /** Hexagon games only: 0 = one hexagon (missing), 1 = a hexagon and the six around it. */
  rings?: number
  /** Listed movement types replace the terrain's cost; 0 = cannot enter. */
  moveCost?: NumMap
  /** Listed numbers replace (or add to) the terrain's numbers of the same name. */
  props?: NumMap
  capturePoints?: number; income?: NumMap; produces?: string[]; tags?: string[]
  /** Item ids sold here; each needs a price. */
  sells?: string[]
  /** A side owning one may research (docs/ECONOMY.md). */
  research?: boolean
  /** Fog of war, as on terrain; they add up with the terrain's. */
  visionBonus?: number | VisionLimit; hides?: boolean
  /** Kinds it can be built up into. */
  upgrades?: Upgrade[]
  /** Unit type ids or tags it repairs; missing = all, [] = nothing. */
  repairs?: string[]
  /** Doors, chests and visitable places (docs/SRPG.md section 10; model/places.ts). A kind has at most one of the three blocks. */
  blocksSight?: boolean
  /** Fog: a fixed light (a brazier): every square within this many squares (1 to 10) is in sight of every team. */
  light?: number
  /** Map weapons (docs/SRPG.md "Map weapons"; model/mapWeapon.ts): the weapon item a unit standing here attacks with, its uses (missing = unlimited), who may use it. */
  weapon?: string; uses?: number; by?: StructureWeaponBy; ignoreRank?: boolean; becomes?: string
  door?: DoorBlock
  chest?: ChestBlock
  visit?: VisitBlock
}
/** Who may use a structure's weapon: these unit types and every type with one of these tags (missing = any unit that can stand there). */
export interface StructureWeaponBy extends Extra { types?: string[]; tags?: string[] }
/** Where a unit may stand to use a door, chest or place: on it, next to it, or either. */
export type PlaceFrom = 'on' | 'next' | 'either'
/** Ways a door or chest can be opened by the player's units (a story command and an unlock staff open any). */
export type OpenWay = 'key' | 'trait' | 'break'
export interface DoorBlock extends Extra { opens: string; locked?: boolean; keys?: string[]; by?: OpenWay[]; hp?: number; stats?: NumMap; from?: PlaceFrom; /** What the unit's menu calls it when attacked ("door" when missing). */ word?: string }
export interface ChestBlock extends Extra { opens: string; locked?: boolean; keys?: string[]; by?: OpenWay[]; from?: PlaceFrom }
export interface VisitBlock extends Extra { becomes?: string; ruin?: string; repeat?: boolean; from?: PlaceFrom }
/** One way to build a structure up into another kind (same footprint). */
export interface Upgrade extends Extra { to: string; cost?: NumMap; requires?: string }
/** One technology of data/research.json (docs/ECONOMY.md "Research"). */
export interface Technology extends Named {
  description?: string
  cost?: NumMap
  /** 0 = finished at once; missing = 1. */
  turns?: number
  requires?: string[]
  unlocks?: { units?: string[] } & Extra
  bonus?: { types?: string[]; stats?: NumMap } & Extra
}
export interface Weapon extends Named {
  /** Carried weapons only: a unit type that `wields` this group can use the weapon. */
  group?: string
  /** A free word for the kind of damage it deals ("physical", "magic", ...): skills can test it (docs/SRPG.md "Damage kind"). */
  damageKind?: string
  range?: [number, number]; counter?: boolean; afterMove?: boolean
  props?: NumMap; versus?: NumMap; versusDefault?: number; damage?: string
  /** A formula for this weapon's hit chance, in place of the game's (docs/SRPG.md "Abilities that strike"). */
  hit?: string
  /** Attacks per supply (fixed weapons only); missing = unlimited. */
  ammo?: number
  /** Sound overrides (event -> sound id, "" = silence): hit, miss, critical (docs/SOUND.md). */
  sounds?: Record<string, string>
  /** Skills while this is the weapon in use (docs/SRPG.md section 3). */
  skills?: string[]
  /** docs/SRPG.md section 7: the lowest rank that may wield it (a rank id) and the weapon experience it earns per use. */
  rank?: string
  rankXp?: number
  /** docs/SRPG.md "Character-locked weapons": ids of the characters that may wield it (needs characters; absent or empty = no lock). */
  wieldedBy?: string[]
  /** …and unit types (class ids of units.json) that may wield it; a unit passes by either list. Needs nothing else. */
  wieldedByTypes?: string[]
  /** docs/SRPG.md section 9: a chance, on a hit, to put a condition on the unit struck (`chance`: a percent or a formula, default every hit). */
  inflict?: { condition: string; chance?: number | string; turns?: number } & Extra
}
export interface Unit extends Named {
  glyph?: string; moveType: string; stats?: NumMap; growths?: NumMap
  weapons?: string[]; cost?: NumMap; tags?: string[]; canCapture?: boolean
  /** Weapon groups it can wield (carried weapons). */
  wields?: string[]
  /** Starting items. */
  items?: ItemEntry[]
  /** docs/SRPG.md: skills it always has, skills learned at a level, menu abilities, promotions, and what it gives
   *  when it is the one behind in a pair. */
  skills?: string[]
  learns?: Learn[]
  abilities?: string[]
  promotions?: Promotion[]
  pairBonus?: NumMap
  /** docs/SRPG.md section 7: starting ranks per weapon group, and the class's own stat caps. */
  ranks?: RankMap
  caps?: NumMap
  /** docs/SRPG.md section 11: this class's top level (0 = never levels); missing = the game's. */
  maxLevel?: number
  /** Tank size; missing = no fuel. */
  fuel?: number
  fuelPerTurn?: number
  /** Refills the units of its side next to it. */
  supplies?: boolean
  /** How far its team sees on a map with fog of war, 0 to 20 (missing = 2). */
  vision?: number
  /** Sound overrides: step, hit, miss, critical, defeated, select, heal (docs/SOUND.md). */
  sounds?: Record<string, string>
}

export type ItemKind = 'usable' | 'weapon' | 'accessory' | 'staff'
/** Who a staff works on (docs/SRPG.md section 8); a usable item only has 'self' and 'ally'. */
export type ItemTarget = 'self' | 'ally' | 'team' | 'foe' | 'any'
export type StaffArea = 'single' | 'all' | { radius: number }
/** One item of data/items.json (docs/ITEMS.md). Fields of another kind than `kind` are refused by the loader. */
export interface Item extends Named {
  description?: string
  kind: ItemKind
  /** Missing = unlimited. */
  uses?: number
  /** Any item with `uses`: false = a staff's repair never offers it (missing = repairable; docs/SRPG.md "Staves: stage two"). */
  repairable?: boolean
  /** Weapons and accessories only: entries a unit may use while it has the item, mastered with ability points (docs/JOBS.md). */
  teaches?: Teach[]
  /** false = a steal ability never takes this item (missing = it can be stolen; docs/SRPG.md "Steal"). */
  stealable?: boolean
  /** false = a shop that buys items back never takes this item (missing = it can be sold). */
  sellable?: boolean
  /** Per resource; missing = not for sale. */
  price?: NumMap
  target?: ItemTarget
  heal?: number | 'full'
  raise?: NumMap
  weapon?: string
  bonus?: NumMap
  /** Accessories only: skills while carried. Usable items only: `promotes` makes it a promotion item (docs/SRPG.md). */
  skills?: string[]
  promotes?: boolean
  /** Usable items only: a torch; lights the square the user stands on (fog of war; docs/ECONOMY.md). Radius 1-10, turns 1-10, shrink 1 to radius - 1. */
  light?: { radius?: number; turns?: number; shrink?: number }
  /** Staves only (docs/SRPG.md section 8). `group`, `rank`, `rankXp`, `wieldedBy` and `props` are a weapon's, so the weapon rank and
   *  lock code serves both (model/gear.ts). `effects`, `if` and anything a later stage adds are kept as the file has them. */
  group?: string
  rank?: string
  rankXp?: number
  wieldedBy?: string[]
  wieldedByTypes?: string[]
  props?: NumMap
  range?: number | [number, number] | string
  area?: StaffArea
  effects?: Extra[]
  hit?: number | string
  xp?: number | string
  /** docs/SRPG.md section 11: what the user earns when the staff missed everyone (needs `hit`). */
  xpMiss?: number | string
  if?: Extra[]
  /** Usable items (docs/SRPG.md section 9): conditions it cures (a list of ids or "all") and the one it puts on the unit it is used on. */
  cure?: string[] | 'all'
  inflict?: { condition: string; turns?: number } & Extra
  /** Keys (usable items only; docs/SRPG.md section 10): "door", "chest" and/or ids of structure kinds. */
  opens?: string[]
  /** Sound overrides: use (docs/SOUND.md). */
  sounds?: Record<string, string>
}
/** An item in a list: the id, or an object with the uses left and (on units) whether it drops when the unit is defeated. */
export type ItemEntry = string | ({ item: string; uses?: number; drop?: boolean } & Extra)

export interface MapSide extends Extra {
  id: string; name: string; color?: string; team?: string
  controller?: string; resources?: NumMap
  /** The side's shared storage at the start. */
  storage?: ItemEntry[]
  /** Fog, computer sides only: true = sees everything, false = bound by fog; missing = the map's `aiSeesAll`. */
  aiSeesAll?: boolean
  /** Computer sides: false = the difficulty's enemy numbers never touch this side's units (docs/DIFFICULTY.md). */
  difficultyShift?: boolean
}
export interface MapUnit extends Extra {
  /** Missing only for a unit placed as a character: its type is then the character's (docs/SRPG.md section 1). */
  type?: string; side: string; x: number; y: number
  name?: string; level?: number; leader?: boolean; stats?: NumMap
  /** The story name events and objectives point at (letters, digits, _ and -; unique in the map). */
  id?: string
  /** Missing = the unit type's starting items; a list (even an empty one) replaces them. */
  items?: ItemEntry[]
  /** A character placed on the map (docs/SRPG.md section 1); then `type`, `name`, `level` are optional overrides. */
  character?: string
  /** Replaces the starting value of the weapon groups it names (docs/SRPG.md section 7). */
  ranks?: RankMap
  /** Skills of its own, learned in addition to those of its type and character (docs/SRPG.md "More skill blocks"). */
  skills?: string[]
  /** Computer-side units only (docs/SRPG.md section 10): what the unit goes for. */
  goesFor?: Array<'chests' | 'villages' | 'doors' | 'steal' | 'walls' | 'weapons'>
  /** Computer-side units only (docs/AI.md): how it moves and when it starts; model/behaviour.ts. */
  behaviour?: Behaviour
  /** Appears only on these difficulties (docs/DIFFICULTY.md); missing = on all. */
  difficulty?: string[]
  /** false = never shifted by the difficulty's enemy numbers (a boss). */
  difficultyShift?: boolean
}
export interface MapFile extends Extra {
  name: string
  grid: { type?: string; width: number; height: number } & Extra
  legend: Record<string, string>
  tiles: string[]
  heights?: number[][]
  sides?: MapSide[]
  /** Old projects only: the engine now refuses a non-empty list (the Convert flow turns it into `structures`). */
  owners?: Array<{ x: number; y: number; side: string } & Extra>
  structures?: MapStructure[]
  units?: MapUnit[]
  /** The squares a computer unit leaves the map by (docs/AI.md "Looting and escaping"); missing = none; model/exits.ts. */
  exits?: Array<{ x: number; y: number }>
  /** Fog of war (docs/ECONOMY.md): missing = off. */
  fog?: boolean
  /** Fog options (docs/ECONOMY.md "Fog of war"): vision added to every unit (-10 to 10) and computer sides that see everything. Only with fog. */
  fogVision?: number
  aiSeesAll?: boolean
  /** The battle scene's background / platform for each terrain on this map, and a "default" (they win over the game's tables); the weather the battle starts with. */
  battleBackgrounds?: Record<string, string>
  battlePlatforms?: Record<string, string>
  weather?: 'rain' | 'snow'
  /** Named map changes (docs/EVENTS.md "Changing the map"); model/mapChanges.ts. A cell may carry the new `tile` (-1 / null = none) of the tile layer. */
  changes?: Record<string, { cells?: Array<{ x: number; y: number; terrain: string; tile?: number | null; keepTile?: boolean }>; structures?: Array<{ id: string; kind: string }>; keepTile?: boolean }>
  /** The tile layer (docs/GRAPHICS.md "Tile layer"): one string per row, four hex digits per square ("...." = no tile); model/tiles.ts. */
  tileLayer?: TileLayer
  /** Music (docs/SOUND.md): a track id of data/audio.json, "" = none on this map; missing = the game's. */
  music?: string
  playerTurnMusic?: string
  enemyTurnMusic?: string
  /** Music by situation (docs/SOUND.md "Music by situation"): the preparation screen's, a side id -> track ("" = none), and the few-enemies track. */
  prepMusic?: string
  phaseMusic?: Record<string, string>
  fewEnemies?: { music?: string; count?: number }
  /** Story events and objectives (docs/EVENTS.md); model/storyTypes.ts. */
  events?: StoryEvent[]
  objectives?: Objectives
  /** The preparation screen before the battle (docs/EVENTS.md "Preparation"); missing = none. */
  prep?: MapPrep
  /** This map's own difficulty numbers (docs/DIFFICULTY.md); model/difficulty.ts. */
  difficulty?: MapDifficulties
}
/** `x`, `y` = the top-left square of the footprint (on hexagon maps: the middle hexagon); `side` missing = nobody owns it. */
export interface MapStructure extends Extra {
  type: string; x: number; y: number; side?: string
  /** Doors, chests and places (docs/SRPG.md section 10): the story name, a lock override, a chest's contents, who may visit. */
  id?: string; locked?: boolean; contents?: ChestLoot[]; visitSides?: string[]; visitUnits?: string[]
  /** A kind with a weapon to use: this placement's own uses (missing = the kind's). */
  uses?: number
}
/** One thing in a chest: an item id, an item with fewer uses left, or an amount of a resource. */
export type ChestLoot = string | { item: string; uses?: number } | { resource: string; amount: number }

// --- art (docs/GRAPHICS.md, data/graphics.json) ---
export type CellRef = [number, number]
/** An image file and how it is cut into pieces (the file format calls this a "sheet"; the UI says "image"). */
export interface Sheet extends Extra { id: string; file: string; cellWidth: number; cellHeight: number; name?: string; smooth?: boolean }
/** How one terrain or unit is drawn: `sheet` plus exactly one of cell / variants / frames / join. */
export interface Look extends Extra {
  sheet: string
  cell?: CellRef
  variants?: CellRef[]
  frames?: CellRef[]
  frameMs?: number
  /** One 4 x 4 block [c, r], or a list of 1-16 blocks played as an animation (speed: frameMs). */
  join?: CellRef | CellRef[]
  joinWith?: string[]
  /** Joins with every terrain except these (exclusive with joinWith; never its own terrain). */
  joinExcept?: string[]
  joinEdge?: boolean
  /** Units and structures only: a different picture for particular sides (by map side id). */
  sides?: Record<string, Look>
  /** Terrain in a style of a 3D kind only: what stands on its ground (model/stand.ts). */
  stand?: Stand
  /** Units only: frames shown while the unit moves (docs/GRAPHICS.md "Walking"). */
  walk?: WalkDef
}
/** Walking frames of a unit picture: lists of cells of `sheet` (default: the picture's own image) per direction; `side` faces right and
 *  the opposite direction is it turned round unless `left` and `right` are both given. */
export interface WalkDef extends Extra {
  sheet?: string
  down?: CellRef[]
  up?: CellRef[]
  side?: CellRef[]
  left?: CellRef[]
  right?: CellRef[]
  frameMs?: number
}
/** A terrain's standing part: an upright picture (`sheet` with cell / variants / frames) or 3D models (`model`, or `variants`
 *  as a list of files). docs/GRAPHICS.md "Standing terrain". */
export interface Stand extends Extra {
  sheet?: string
  cell?: CellRef
  variants?: CellRef[] | string[]
  frames?: CellRef[]
  frameMs?: number
  /** Picture: how wide it is drawn, in tile-size pixels (empty = one square). */
  width?: number
  model?: string
  /** Models: how tall it stands, in squares (empty = 1). */
  height?: number
  turn?: number
  scatter?: boolean
}
/** A visual style's overrides; `null` = no picture in that style. */
export interface StyleArt extends Extra { terrain?: Record<string, Look | null>; units?: Record<string, Look | null>; structures?: Record<string, Look | null> }
/** A dialogue portrait: one piece of an image (docs/EVENTS.md "Dialogue"). */
export interface Portrait extends Extra { sheet: string; cell: CellRef; faces?: 'left' | 'right'; talk?: CellRef[]; blink?: CellRef[] }
/** A full-screen picture of a story scene: the whole image of a sheet, or one piece of it (docs/GRAPHICS.md "Backgrounds"). */
export interface SceneBackground extends Extra { sheet: string; cell?: CellRef }
/** One animation of a battle set: cells of an image played in order (docs/BATTLE_SCENE.md). */
export interface BattleAnimDef extends Extra { sheet: string; frames: CellRef[]; ms?: number | number[]; loop?: boolean; hit?: number; spawn?: number; sides?: Record<string, SideSheetDef>; sounds?: Record<string, string>
  /** Holding a pose until the result has been shown (docs/BATTLE_SCENE.md "Holding a pose"); startFrame / startDelayMs are for a dodge. Frame numbers count from 0. */
  holdFrame?: number; startFrame?: number; startDelayMs?: number }
/** Another image with the same layout, drawn by the fighters of one side (a map side id). */
export interface SideSheetDef extends Extra { sheet: string }
/** A fighter's animation set: `stand` is required, the other animations are optional (the map picture moves instead). */
export interface BattleSetDef extends Extra { anchor?: [number, number]; faces?: 'right' | 'left'; mirror?: boolean; animations: Record<string, BattleAnimDef>; sides?: Record<string, SideSheetDef> }
/** Who uses a set: a unit type, a character, a weapon group or one weapon, a side (the most specific rule wins). */
export interface BattleAssignDef extends Extra { set: string; type?: string; character?: string; group?: string; weapon?: string; side?: string }
export interface BattleBackgroundDef extends Extra { sheet: string; cell: CellRef }
/** `atRangeFixed` on a row: timed from the effect's start before its "atRange" delay (docs/BATTLE_SCENE.md "Leaving a part out of the delay"). */
export interface FxFlashDef extends Extra { atMs: number; ms: number; color?: string; atRangeFixed?: boolean }
export interface FxShakeDef extends Extra { atMs: number; ms: number; strength: number; atRangeFixed?: boolean }
/** An effect's scene-wide parts (docs/BATTLE_SCENE.md "Hiding and fading"; model/battleEffects.ts). */
export interface FxHideDef extends Extra { atMs: number; ms: number; what: Array<'backdrop' | 'platforms'>; color?: string; atRangeFixed?: boolean }
export interface FxFadeDef extends Extra { atMs: number; ms: number; to?: string; alpha?: number; holdMs?: number; over?: 'all' | 'backdrop'; atRangeFixed?: boolean }
/** One step of an effect's "atRange": from a fight `from` squares apart the whole effect starts `delayMs` later and / or a flight stops `endOffset` pixels short of (negative: past) the foe. */
export interface FxRangeDef extends Extra { from: number; delayMs?: number; endOffset?: number }
/** A battle scene effect (docs/BATTLE_SCENE.md "Effects"; model/battleEffects.ts). `frames` entries are [column, row] or, from another image, [column, row, image id]. */
export interface BattleEffectDef extends Extra {
  id?: string; weapon?: string; group?: string
  sheet?: string; frames?: Array<CellRef | [number, number, string]>; ms?: number | number[]
  mode?: 'target' | 'travel'; travelMs?: number; anchor?: [number, number]
  sounds?: Record<string, string>; hitAtMs?: number; fromHitFrame?: boolean; hitAfterMs?: number; layer?: 'front' | 'behind' | 'screen'; mirror?: boolean
  flash?: FxFlashDef[]; shake?: FxShakeDef[]; at?: 'target' | 'striker'; returns?: boolean; lingerMs?: number
  effects?: string | string[]; byType?: Record<string, string | string[]>
  blend?: 'normal' | 'add'; hide?: FxHideDef[]; fade?: FxFadeDef[]; order?: 'backdrop' | 'behind' | 'front'; speed?: number
  /** Newer opt-in keys: a delay of the whole effect by fight distance, and a flight that stops short of the foe (docs/BATTLE_SCENE.md). */
  atRange?: FxRangeDef[]; endOffset?: number
  /** Fifth batch: a part that the distance's delay does not move, and the camera's own start for this effect (docs/BATTLE_SCENE.md). */
  atRangeFixed?: boolean; panAtMs?: number
}
/** data/graphics.json "battle" > "scene": how long a held pose waits after the result (docs/BATTLE_SCENE.md "Holding a pose"). */
export interface HitHoldDef extends Extra { minMs?: number; baseMs?: number; perHpMs?: number; killMs?: number; noChangeMs?: number }
/** "quote" (docs/BATTLE_SCENE.md "Quotes in the scene"): the hiding and sliding of a quote played inside the scene. */
export interface QuoteTimingDef extends Extra { leadMs?: number; slideOutMs?: number; slideInMs?: number }
export interface BattleSceneTimingDef extends Extra { resultHoldMs?: number; resultHoldAgainMs?: number; hitHold?: HitHoldDef; quote?: QuoteTimingDef }
/** data/graphics.json "battle". */
export interface BattleSection extends Extra {
  sets?: Record<string, BattleSetDef>
  effects?: BattleEffectDef[]
  assign?: BattleAssignDef[]
  backgrounds?: Record<string, BattleBackgroundDef>
  terrainBackground?: Record<string, string>
  defaultBackground?: string
  /** Pictures drawn under each fighter, by terrain (model/battleTables.ts); `far` is used when the fight is at range 2+. */
  platforms?: Record<string, { sheet: string; cell: CellRef; far?: CellRef; right?: CellRef; farRight?: CellRef } & Extra>
  terrainPlatform?: Record<string, string>
  defaultPlatform?: string
  /** An optional fixed stage (docs/BATTLE_SCENE.md "Fixed stage"; model/battleStage.ts). */
  stage?: BattleStageDef
  scene?: BattleSceneTimingDef
}
export type StagePoint = [number, number]
export interface StagePair { left: StagePoint; right: StagePoint }
export interface BattleStageDef extends Extra {
  width: number
  height: number
  feet: StagePair
  feetFar?: StagePair
  platform?: StagePair
  platformFar?: StagePair
  align?: 'top' | 'center' | 'bottom'
  fit?: 'cover' | 'contain'
  rangeLine?: boolean
  /** Extra layouts by fight distance (docs/BATTLE_SCENE.md "Range bands"; model/stageBands.ts). */
  ranges?: StageBandDef[]
}
export interface StageBandDef extends Extra {
  from: number
  feet: StagePair
  platform?: StagePair
  view?: 'both' | 'each'
  pan?: { panMs?: number; leftView: number; rightView: number; backdrop?: 'fixed' | 'scroll'; startMs?: number } & Extra
}
/** A tileset of hand-painted maps (docs/GRAPHICS.md "Tile layer"): a sheet whose cells are the tiles, numbered row by row from 0; model/tilesets.ts. */
export interface Tileset extends Extra { sheet: string; animations?: Record<string, { frames: number[]; ms?: number }> }
export interface TileLayer extends Extra { tileset: string; cells: string[] }
/** A small picture drawn on the map sprite of the units that carry a tag (docs/GRAPHICS.md "Unit badges"); model/badges.ts. */
export interface Badge extends Extra {
  id: string
  sheet: string
  cell?: CellRef
  tag: string
  sides?: 'all' | 'computer' | 'player' | string[]
  anchor?: 'topLeft' | 'topRight' | 'bottomLeft' | 'bottomRight' | 'center'
  offset?: [number, number]
}
/** A map marker (docs/GRAPHICS.md "Map markers"); model/markers.ts. */
export interface Marker extends Extra {
  id: string
  sheet: string
  cell?: CellRef
  anchor?: 'topLeft' | 'topRight' | 'bottomLeft' | 'bottomRight' | 'center'
  offset?: [number, number]
}
export interface Graphics extends Extra {
  sheets: Sheet[]
  /** Map markers, by name. */
  markers?: Marker[]
  /** Unit badges, in drawing order. */
  badges?: Badge[]
  /** Tilesets of tile layers: id -> sheet (and animated tiles). */
  tilesets?: Record<string, Tileset>
  /** The side-view battle scene's animation sets, rules and backgrounds. */
  battle?: BattleSection
  /** Portrait id -> picture; the id is what a `say` command's "portrait" names. */
  portraits?: Record<string, Portrait>
  /** Item and weapon icons: weapon or item id -> one cell of a sheet (no editor screen yet; kept as is, and dropped with their sheet). */
  icons?: Record<string, { sheet: string; cell: [number, number] }>
  /** Story scene backgrounds: id -> picture; the id is what a `background` command names (not the battle scene's `battle.backgrounds`). */
  backgrounds?: Record<string, SceneBackground>
  unitMarker?: boolean
  terrain?: Record<string, Look>
  units?: Record<string, Look>
  structures?: Record<string, Look>
  styles?: Record<string, StyleArt>
}
export type LookKind = 'terrain' | 'units' | 'structures'

/** The editable project: exactly the GET shape minus id/revision/problems. */
export interface ProjectDoc {
  game: Game | null
  terrain: Terrain[] | null
  /** null = the game has no structures yet (the first structure edit creates the list). */
  structures: Structure[] | null
  weapons: Weapon[] | null
  /** null = the game has no items yet (the first item edit creates the list). */
  items: Item[] | null
  /** null = the game has no research yet (the first technology edit creates the list; an empty list removes the file). */
  research: Technology[] | null
  units: Unit[] | null
  maps: Record<string, MapFile | null>
  scripts: Record<string, string>
  /** null = the game has no art yet (the first art edit creates it). */
  graphics: Graphics | null
  /** null = the game has no campaign (single battles). Removing it is written as `campaign: false` (see autosave). */
  campaign: Campaign | null
  /** null = the game has no sound (a silent game). Removing all of it is written as `audio: false` (see autosave). */
  audio: Audio | null
  /** Strategy-RPG files (docs/SRPG.md). null = the game has none (on screen an empty list can stand for "just turned
   *  on"; an empty list is never written: it removes the file). `supports` is false on the wire to remove the file. */
  characters: Character[] | null
  skills: Skill[] | null
  abilities: Ability[] | null
  /** Status conditions (docs/SRPG.md section 9, data/conditions.json). */
  conditions: Condition[] | null
  supports: Supports | null
  /** Weapon ranks and stat caps (section 7): the file's object, null = no file. An empty ladder / empty caps are written
   *  as they are, so a switch that is on stays on after a reload; `false` on the wire removes the file. */
  ranks: Ranks | null
  caps: Caps | null
}

export interface Problem { file: string; message: string }

/** The GET /api/projects/:id response. */
export interface ServerProject extends ProjectDoc {
  id: string
  revision: string
  problems: Problem[]
}

export type Section = 'game' | 'rules' | 'units' | 'characters' | 'skills' | 'abilities' | 'conditions' | 'supports' | 'weapons' | 'items' | 'terrain' | 'structures' | 'research' | 'art' | 'sound' | 'maps' | 'campaign'
export const SECTIONS: Section[] = ['game', 'rules', 'units', 'characters', 'skills', 'abilities', 'conditions', 'supports', 'weapons', 'items', 'terrain', 'structures', 'research', 'art', 'sound', 'maps', 'campaign']
