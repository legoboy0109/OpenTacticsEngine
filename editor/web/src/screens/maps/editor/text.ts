// All wording of the map editor (canvas, tools, panels, dialogs).
export const T = {
  canvasLabel: 'Map. Arrow keys move the cursor, Enter uses the current tool, Delete removes a unit.',
  toolsLabel: 'Map tools',
  tools: {
    terrain: { label: 'Terrain', key: 'T', help: 'Paint the ground.' },
    units: { label: 'Units', key: 'U', help: 'Place and move units.' },
    owners: { label: 'Owners', key: 'O', help: 'Set who owns buildings at the start.' },
    select: { label: 'Select', key: 'S', help: 'Look at a square.' },
  },
  zoomIn: 'Zoom in',
  zoomOut: 'Zoom out',
  look: 'Look',
  lookAria: 'Look to preview',
  lookHelp: 'Preview the map in one of the game’s looks (its pictures, colours and background).',
  fit: 'Fit',
  fitHelp: 'Show the whole map',
  shortcuts: 'Keys: T U O S pick a tool, F fill, + / − zoom, Delete removes a unit, Esc deselects, Space + drag pans.',

  // status line
  statusSquare: (x: number, y: number) => `${x + 1}, ${y + 1}`,
  statusNothing: 'Hover over the map to see a square.',
  statusOwner: (n: string) => `owned by ${n}`,
  statusUnit: (type: string, side: string, name?: string) => `${name ? `${name}, ` : ''}${side} ${type}`,
  big: (w: number, h: number) => `${w} × ${h} squares is a big map: painting and zooming will feel slower than on a small one.`,

  // warnings
  warningsTitle: (n: number) => (n === 1 ? '1 thing to check' : `${n} things to check`),
  warningsHelp: 'The game still saves; these are only hints. Click one to find it on the map.',

  // repair
  repairTitle: 'This map’s squares are stored inconsistently.',
  repairBody: 'The editor can fix it for you: rows are padded or cut to the right size, and squares it does not understand become the first kind of terrain.',
  repairButton: 'Repair this map',
  repairUndo: 'You can undo the repair.',

  // terrain panel
  terrainTitle: 'Terrain',
  terrainHelp: 'Pick a kind of terrain, then drag on the map to paint it. Alt-click on the map (or press and hold on a phone) picks the terrain under the pointer.',
  brush: 'Brush',
  fill: 'Fill',
  fillHelp: 'Fill replaces the whole connected area of one terrain with the chosen one.',
  oneTerrain: 'This game has only one kind of terrain, so there is nothing else to paint. Add more under Terrain.',

  // units panel
  unitsTitle: 'Units',
  unitsHelp: 'Pick a side and a unit type, then click an empty square to place one. Click a unit to select it; drag it to move it.',
  sideLabel: 'Side',
  unitTypeLabel: 'Unit type',
  noUnitsTypes: 'This game has no unit types yet. Add some under Units.',

  // selected unit
  unitTitle: 'Selected unit',
  unitAt: (x: number, y: number) => `at ${x + 1}, ${y + 1}`,
  unitNone: 'Click a unit on the map to change its details.',
  unitName: 'Name',
  unitNameHelp: 'Optional. Shown in battle.',
  unitLevel: 'Level',
  unitLevelHelp: 'Starting level (1 or more).',
  unitLeader: 'Leader',
  unitLeaderHelp: 'Some games are lost when the leader is defeated.',
  unitRemove: 'Remove unit',
  statsTitle: 'Own stats',
  statsHelp: 'Leave a box empty to use the unit type’s value (shown greyed). Type a number to give just this unit a different one.',

  // owners panel
  ownersTitle: 'Owners',
  ownersHelp: 'Buildings can belong to a side from the start. Ownership matters for terrain that can be captured, gives income or can build units, and for headquarters. Click or drag on the map to set it.',
  neutral: 'No owner',
  ownerNoEffect: (terrain: string) => `${terrain} has no use for an owner (it cannot be captured, earns nothing and builds nothing). It was set anyway.`,

  // select panel
  selectTitle: 'Square',
  selectHelp: 'Click a square to look at it. Click a unit to change its details.',
  selectNone: 'Nothing selected.',
  terrainOf: 'Terrain',
  ownerOf: 'Owner',
  unitOf: 'Unit',
  noOwner: 'nobody',
  noUnit: 'none',
  moveCostsTitle: 'Movement costs',
  cannotEnter: 'cannot enter',

  // panel tabs
  tabs: { tool: 'Tool', map: 'Map', sides: 'Sides' },
  panelLabel: 'Map options',
  sheetOpen: 'Show options',
  sheetClose: 'Hide options',

  // map settings
  settingsTitle: 'Map',
  nameLabel: 'Name',
  nameHelp: 'What players see.',
  sizeLabel: 'Size',
  sizeValue: (w: number, h: number) => `${w} × ${h} squares`,
  resizeButton: 'Resize…',
  resizeTitle: 'Resize the map',
  widthLabel: 'Width',
  heightLabel: 'Height',
  anchorLabel: 'Keep the existing map',
  anchorHelp: 'Where the current map stays. New squares are added (or old ones cut off) on the opposite sides.',
  anchorNames: { nw: 'top left', n: 'top', ne: 'top right', w: 'left', c: 'middle', e: 'right', sw: 'bottom left', s: 'bottom', se: 'bottom right' },
  sizeRange: 'Sizes go from 1 to 256 squares.',
  sizeBigWarn: 'Big maps are slow to edit.',
  cropTitle: 'This will remove:',
  cropUnits: (n: number) => (n === 1 ? '1 unit' : `${n} units`),
  cropOwners: (n: number) => (n === 1 ? '1 owned square' : `${n} owned squares`),
  cropNothing: 'Nothing on the map is lost.',
  cropCells: 'Squares outside the new size.',
  resizeApply: 'Resize',
  resizeUndo: 'You can undo this with the Undo button.',

  // sides
  sidesTitle: 'Sides',
  sidesHelp: 'Sides take turns in this order. Each needs units to play with.',
  addSide: 'Add a side',
  removeSide: 'Remove',
  removeSideTitle: (n: string) => `Remove the side “${n}”?`,
  removeSideBody: (units: number, owners: number) => `Its ${units} unit${units === 1 ? '' : 's'} and ${owners} owned square${owners === 1 ? '' : 's'} are removed with it.`,
  removeSideMin: 'A battle needs at least two sides.',
  sideName: 'Name',
  sideColor: 'Colour',
  sideControl: 'Played by',
  controlHuman: 'A person',
  controlAi: 'The computer',
  sideTeam: 'Team',
  sideTeamHelp: 'Sides with the same team name are allies. Leave empty to play alone.',
  sideResources: 'Starting resources',
} as const
