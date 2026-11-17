// Wording of two small story pieces (docs/EVENTS.md "Branching on who acted" and "Sending a unit's items to storage"). Placeholder wording.
export const EX = {
  group: 'Storage',
  kinds: { storeItems: 'Send a unit’s items to storage' },
  noStorage: 'no shared storage',

  // The `actor` condition.
  conditionActor: 'The unit that set this off',
  actorWho: 'Which unit',
  actorHelp: 'True when the unit that set this event off is the one picked: the one that arrived (“A unit reaches an area”), fell, attacked, talked, was promoted, opened or visited something. Pick “Any unit” to test only that some unit did. It is never true in an event nobody sets off (a turn or battle event). Use it in “Only if”, in an If command, or on an answer of a question to branch inside one event.',
  actorNobody: 'Nobody sets this event off (its “When” is a turn, the battle or a support level), so this is never true.',

  // The `storeItems` command.
  storeWho: 'Whose items',
  storeWhoScene: 'Which character',
  storeKeep: 'Keep these with the unit',
  storeKeepNone: 'Nothing: every item is sent.',
  storeHelp: 'Every item the units carry goes to the shared storage of their side, the one the preparation screen and the item menu show (the weapon in hand too: the unit then fights with its next weapon, or none). Computer units are left alone. The storage has no size limit.',
  storeHelpScene: 'Every item the character carries goes to the campaign’s shared storage, the one the preparation screen shows. Nothing happens if they are not in the party.',
  storeSummary: (who: string, keep: string[]) => `Sends ${who}’s items to the storage${keep.length ? `, except ${keep.join(', ')}` : ''}`,
  storeNoStorage: 'The game has no shared storage, so there is nothing to send items to. Turn the storage on in the item rules (Items screen).',
}
