// Types mirroring docs/GAME_FORMAT.md. Every object carries an index
// signature so keys the UI does not know about survive load -> edit -> save.
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
export interface ItemRules extends Extra { weapons?: 'fixed' | 'carried'; inventorySize?: number; storage?: StorageMode }
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
export interface Rules extends Extra {
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
  moveTypes: Named[]
  rules?: Rules
  formulas?: Formulas
  styles?: Style[]
  scripts?: string[]
  maps: MapRef[]
  editor?: EditorData
  /** Who may save (docs/SAVES.md): missing = 'any'. */
  saving?: SavingMode
  /** Story switches (off / on) and variables (whole numbers) that events read and set (docs/EVENTS.md). */
  switches?: Named[]
  variables?: Named[]
}

/** Extra sight for some unit types only (docs/ECONOMY.md "Fog of war"): unit type ids or tags, never empty in a good file. */
export interface VisionLimit { amount: number; units: string[] }
export interface Terrain extends Named {
  color?: string; glyph?: string; moveCost?: NumMap; props?: NumMap; tags?: string[]
  /** Fog of war: extra sight (0 to 10, missing = 0) for a unit standing here (a plain number: every unit; or an
   *  object: those unit types only); a unit here is seen only from next to it. */
  visionBonus?: number | VisionLimit; hides?: boolean
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
  door?: DoorBlock
  chest?: ChestBlock
  visit?: VisitBlock
}
/** Where a unit may stand to use a door, chest or place: on it, next to it, or either. */
export type PlaceFrom = 'on' | 'next' | 'either'
/** Ways a door or chest can be opened by the player's units (a story command and an unlock staff open any). */
export type OpenWay = 'key' | 'trait' | 'break'
export interface DoorBlock extends Extra { opens: string; locked?: boolean; keys?: string[]; by?: OpenWay[]; hp?: number; stats?: NumMap; from?: PlaceFrom }
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
  /** false = a steal ability never takes this item (missing = it can be stolen; docs/SRPG.md "Steal"). */
  stealable?: boolean
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
  /** Staves only (docs/SRPG.md section 8). `group`, `rank`, `rankXp`, `wieldedBy` and `props` are a weapon's, so the weapon rank and
   *  lock code serves both (model/gear.ts). `effects`, `if` and anything a later stage adds are kept as the file has them. */
  group?: string
  rank?: string
  rankXp?: number
  wieldedBy?: string[]
  props?: NumMap
  range?: number | [number, number] | string
  area?: StaffArea
  effects?: Extra[]
  hit?: number | string
  xp?: number | string
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
  goesFor?: Array<'chests' | 'villages' | 'doors' | 'steal'>
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
  /** Fog of war (docs/ECONOMY.md): missing = off. */
  fog?: boolean
  /** Music (docs/SOUND.md): a track id of data/audio.json, "" = none on this map; missing = the game's. */
  music?: string
  playerTurnMusic?: string
  enemyTurnMusic?: string
  /** Story events and objectives (docs/EVENTS.md); model/storyTypes.ts. */
  events?: StoryEvent[]
  objectives?: Objectives
  /** The preparation screen before the battle (docs/EVENTS.md "Preparation"); missing = none. */
  prep?: MapPrep
}
/** `x`, `y` = the top-left square of the footprint (on hexagon maps: the middle hexagon); `side` missing = nobody owns it. */
export interface MapStructure extends Extra {
  type: string; x: number; y: number; side?: string
  /** Doors, chests and places (docs/SRPG.md section 10): the story name, a lock override, a chest's contents, who may visit. */
  id?: string; locked?: boolean; contents?: ChestLoot[]; visitSides?: string[]; visitUnits?: string[]
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
export interface Portrait extends Extra { sheet: string; cell: CellRef }
export interface Graphics extends Extra {
  sheets: Sheet[]
  /** Portrait id -> picture; the id is what a `say` command's "portrait" names. */
  portraits?: Record<string, Portrait>
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
