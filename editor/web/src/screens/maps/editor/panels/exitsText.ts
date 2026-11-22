// Wording of the map's exits (docs/AI.md "Looting and escaping"). All of it is placeholder wording.
export const X = {
  title: 'Exits',
  help: 'Squares a computer unit that escapes (or a looter with nothing left to take) walks to and leaves the map by. Counted from 1, like the rest of the editor. None: nobody can escape.',
  square: (n: number) => `Exit ${n}`,
  column: (n: number) => `Exit ${n}: column`,
  row: (n: number) => `Exit ${n}: row`,
  add: 'Add an exit',
  remove: (n: number) => `Remove exit ${n}`,
}
