// Wording of the Platforms card on the Art screen (placeholder wording for the owner to review).
export const PT = {
  title: 'Platforms',
  help: 'A picture drawn under each fighter in the battle scene (a patch of grass, a stone slab). It is enlarged like the fighter and centred on its feet. A platform may have a second picture for fights at range 2 or more. Choose them per terrain on the Terrain screen, or per map in the map’s settings.',
  needImage: 'Add an image on this page first, then make a platform from it.',
  none: 'No platforms yet.',
  name: (id: string) => `Platform ${id}`,
  farName: (id: string) => `Platform ${id}, at range`,
  choose: 'Choose picture',
  chooseFar: 'Choose far picture',
  addFar: 'Add a far picture',
  clearFar: (id: string) => `Remove the far picture of ${id}`,
  clearFarText: 'Remove far',
  delete: (id: string) => `Delete platform ${id}`,
  newName: 'Name of a new platform',
  add: 'Add platform',
  default: 'Default platform',
  noDefault: 'None',
}
