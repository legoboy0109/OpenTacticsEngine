// Wording of the character-locked weapons card and lines (placeholders: the project owner reviews them).
export const L = {
  title: 'Who can wield it',
  help: 'Tick the classes and characters who may use this weapon; leave everything unticked for no lock. A unit passes if it is on either list. A lock only narrows: the class must still be able to use this kind of weapon, and the rank it asks for still applies. Anyone can still carry, trade and store it.',
  who: 'Characters who can wield it',
  whoTypes: (word: string) => `${word.charAt(0).toUpperCase()}${word.slice(1)} that can wield it`,
  none: 'No lock: anyone whose class and rank allow it can wield it.',
  some: (chars: number, types: number, word: string) => {
    const c = `${chars} ${chars === 1 ? 'character' : 'characters'}`
    const t = `${types} ${word}`
    return (chars > 0 && types > 0 ? `Locked to ${c} and to ${t}` : chars > 0 ? `Locked to ${c}` : `Locked to ${t}`) + '. Everyone else can carry it but not use it.'
  },
  unknown: (id: string) => `${id} (not a character of this game)`,
  unknownType: (id: string, word: string) => `${id} (not a ${word} of this game)`,
  offNote: (n: number, staves = 0) => (staves > 0 ? `${n} weapons and staves are locked to named characters; those locks are removed too.` : `${n === 1 ? 'One weapon is' : `${n} weapons are`} locked to named characters; those locks are removed too.`),
  charTitle: 'Weapons and staves locked to this character',
  charHelp: 'Weapons and staves that only named characters can wield, this one among them. Change it on the Weapons or Items screen.',
  helpStaff: 'Tick the classes and characters who may use this staff; leave everything unticked for no lock. A unit passes if it is on either list. A lock only narrows: the class must still be able to use this staff’s group, and the rank it asks for still applies. Anyone can still carry, trade and store it.',
  noneStaff: 'No lock: anyone whose class and rank allow it can use it.',
}
