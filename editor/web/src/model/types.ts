// Types mirroring docs/GAME_FORMAT.md. Every object carries an index
// signature so keys the UI does not know about survive load -> edit -> save.
import type { Campaign } from './campaignTypes'
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
export interface Rules extends Extra {
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

export interface Game extends Extra {
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
}
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
  range?: [number, number]; counter?: boolean; afterMove?: boolean
  props?: NumMap; versus?: NumMap; versusDefault?: number; damage?: string
  /** Attacks per supply (fixed weapons only); missing = unlimited. */
  ammo?: number
}
export interface Unit extends Named {
  glyph?: string; moveType: string; stats?: NumMap; growths?: NumMap
  weapons?: string[]; cost?: NumMap; tags?: string[]; canCapture?: boolean
  /** Weapon groups it can wield (carried weapons). */
  wields?: string[]
  /** Starting items. */
  items?: ItemEntry[]
  /** Tank size; missing = no fuel. */
  fuel?: number
  fuelPerTurn?: number
  /** Refills the units of its side next to it. */
  supplies?: boolean
  /** How far its team sees on a map with fog of war, 0 to 20 (missing = 2). */
  vision?: number
}

export type ItemKind = 'usable' | 'weapon' | 'accessory'
/** One item of data/items.json (docs/ITEMS.md). Fields of another kind than `kind` are refused by the loader. */
export interface Item extends Named {
  description?: string
  kind: ItemKind
  /** Missing = unlimited. */
  uses?: number
  /** Per resource; missing = not for sale. */
  price?: NumMap
  target?: 'self' | 'ally'
  heal?: number | 'full'
  raise?: NumMap
  weapon?: string
  bonus?: NumMap
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
  type: string; side: string; x: number; y: number
  name?: string; level?: number; leader?: boolean; stats?: NumMap
  /** The story name events and objectives point at (letters, digits, _ and -; unique in the map). */
  id?: string
  /** Missing = the unit type's starting items; a list (even an empty one) replaces them. */
  items?: ItemEntry[]
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
  /** Story events and objectives (docs/EVENTS.md); model/storyTypes.ts. */
  events?: StoryEvent[]
  objectives?: Objectives
}
/** `x`, `y` = the top-left square of the footprint; `side` missing = nobody owns it. */
export interface MapStructure extends Extra { type: string; x: number; y: number; side?: string }

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
}

export interface Problem { file: string; message: string }

/** The GET /api/projects/:id response. */
export interface ServerProject extends ProjectDoc {
  id: string
  revision: string
  problems: Problem[]
}

export type Section = 'game' | 'rules' | 'units' | 'weapons' | 'items' | 'terrain' | 'structures' | 'research' | 'art' | 'maps' | 'campaign'
export const SECTIONS: Section[] = ['game', 'rules', 'units', 'weapons', 'items', 'terrain', 'structures', 'research', 'art', 'maps', 'campaign']
