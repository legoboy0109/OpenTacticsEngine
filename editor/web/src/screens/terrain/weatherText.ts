// Wording of the terrain's weather costs and platform (placeholder wording for the owner to review).
export const WX = {
  title: 'Weather and platform',
  help: 'While it rains or snows on a map (the map’s Weather setting or a story command), this terrain costs this much to enter instead of its usual cost. A building with a cost of its own keeps it. Empty = the usual cost. “Closed” = nobody of that movement type can enter it while the weather lasts (a unit already standing there stays and can walk off).',
  costs: 'Cost to enter in the weather',
  moveType: 'Movement type',
  weather: { rain: 'Rain', snow: 'Snow' } as Record<'rain' | 'snow', string>,
  same: 'Usual',
  closed: 'Closed',
  platform: 'Platform in the battle scene',
  platformHelp: 'The picture each fighter stands on when a fight is decided on this terrain (made in the Art section, “Platforms”). The game’s default platform when left alone.',
  platformDefault: 'Default',
}
