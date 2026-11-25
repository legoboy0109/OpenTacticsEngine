// All wording of the Poses card on a unit's or character's Picture card (docs/GRAPHICS.md "Poses and facing").
// PLACEHOLDERS: every wording here is maker-facing text the owner has not chosen yet (see docs/EDITOR_UI.md "Owner decisions").
export const PS = {
  title: 'Poses',
  help: 'Extra pictures for a unit seen from the front and from behind, and while it strikes, is hurt or is defeated (map views only). Each is drawn from rows of an image, like the walking pictures. A pose you do not give falls back as each says.',
  forWho: 'For',
  forAll: 'All sides (this picture)',
  forSide: (name: string) => `${name} (its own picture)`,
  kinds: [
    { key: 'facing', label: 'Standing, facing', help: 'The standing picture seen from the front (down-left on the 2D isometric screen) and from behind (up-left). Without it the unit shows its standing picture.' },
    { key: 'attack', label: 'Striking', help: 'Played once when the unit strikes (the lunge on the map). Without it the unit keeps its standing picture.' },
    { key: 'hurt', label: 'Hurt', help: 'Played once on the target while a hit lands. Without it the unit keeps its standing picture.' },
    { key: 'ko', label: 'Defeated', help: 'Shown while a defeated unit fades out. Without it the hurt pictures are used.' },
  ] as const,
  add: (label: string) => `Add “${label}” pictures`,
  remove: (label: string) => `Remove “${label}” pictures`,
  image: 'Image',
  sameImage: 'The same image as the standing picture',
  frames: 'Frames',
  framesHelp: 'How many pictures in each row, counted from the left (1 to 64).',
  ms: 'Milliseconds per frame',
  front: 'Front row (down-left)',
  back: 'Back row (up-left)',
  rowHelp: 'The row of the image, counted from 1 at the top. Leave a row empty to give that view none: the nearest given one is used.',
  other: (dirs: string) => `This pose also has rows for ${dirs} (written in the file; kept as they are). Changing the number of frames changes them too.`,
  handWritten: (dirs: string) => `The frames for ${dirs} were written by hand, not as a row from the left. Changing a row or the number of frames replaces them.`,
}
