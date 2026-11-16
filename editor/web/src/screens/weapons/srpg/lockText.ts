// Wording of the character-locked weapons card and lines (placeholders: the project owner reviews them).
export const L = {
  title: 'Only these characters can wield it',
  help: 'Tick the characters who may use this weapon; leave all unticked for no lock. A lock only narrows: a character’s class must still be able to use this kind of weapon, and the rank it asks for still applies. Anyone can still carry, trade and store it.',
  who: 'Characters who can wield it',
  none: 'No lock: anyone whose class and rank allow it can wield it.',
  some: (n: number) => `Locked to ${n} ${n === 1 ? 'character' : 'characters'}. Everyone else can carry it but not use it.`,
  unknown: (id: string) => `${id} (not a character of this game)`,
  offNote: (n: number) => `${n === 1 ? 'One weapon is' : `${n} weapons are`} locked to named characters; those locks are removed too.`,
  charTitle: 'Weapons locked to this character',
  charHelp: 'Weapons that only named characters can wield, this one among them. Change it on the Weapons screen.',
}
