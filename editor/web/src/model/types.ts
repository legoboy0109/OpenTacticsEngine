// Types mirroring docs/GAME_FORMAT.md. Every object carries an index
// signature so keys the UI does not know about survive load -> edit -> save.

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
export interface Rules extends Extra {
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
export interface Style extends Named {
  background?: string; showGlyphs?: boolean; terrainColors?: Record<string, string>
}
export interface MapRef extends Named { file: string }

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
}

export interface Terrain extends Named {
  color?: string; glyph?: string; moveCost?: NumMap; props?: NumMap; capture?: number
  income?: NumMap; produces?: string[]; tags?: string[]
}
export interface Weapon extends Named {
  range?: [number, number]; counter?: boolean; afterMove?: boolean
  props?: NumMap; versus?: NumMap; versusDefault?: number; damage?: string
}
export interface Unit extends Named {
  glyph?: string; moveType: string; stats?: NumMap; growths?: NumMap
  weapons?: string[]; cost?: NumMap; tags?: string[]; canCapture?: boolean
}

export interface MapSide extends Extra {
  id: string; name: string; color?: string; team?: string
  controller?: string; resources?: NumMap
}
export interface MapUnit extends Extra {
  type: string; side: string; x: number; y: number
  name?: string; level?: number; leader?: boolean; stats?: NumMap
}
export interface MapFile extends Extra {
  name: string
  grid: { type?: string; width: number; height: number } & Extra
  legend: Record<string, string>
  tiles: string[]
  heights?: number[][]
  sides?: MapSide[]
  owners?: Array<{ x: number; y: number; side: string } & Extra>
  units?: MapUnit[]
}

/** The editable project: exactly the GET shape minus id/revision/problems. */
export interface ProjectDoc {
  game: Game | null
  terrain: Terrain[] | null
  weapons: Weapon[] | null
  units: Unit[] | null
  maps: Record<string, MapFile | null>
  scripts: Record<string, string>
}

export interface Problem { file: string; message: string }

/** The GET /api/projects/:id response. */
export interface ServerProject extends ProjectDoc {
  id: string
  revision: string
  problems: Problem[]
}

export type Section = 'game' | 'rules' | 'units' | 'weapons' | 'terrain' | 'maps'
export const SECTIONS: Section[] = ['game', 'rules', 'units', 'weapons', 'terrain', 'maps']
