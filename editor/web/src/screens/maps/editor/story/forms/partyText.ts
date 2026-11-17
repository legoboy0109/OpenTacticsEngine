// Wording of the two party commands of a campaign scene (docs/EVENTS.md "Parties"). Placeholder wording.
export const PT = {
  group: 'Party',
  kinds: { switchParty: 'Switch to another party', mergeParties: 'Merge parties' },
  noParties: 'no parties yet',

  switchWhich: 'Party to switch to',
  switchNone: 'The campaign has no parties yet. Add some under “The parties” on the Campaign screen.',
  switchHelp: 'From here on the story follows this party: its own characters, items, storage and funds are what the next battles and the preparation screen use. The story itself (switches, variables, the chapters) goes on as it is. A party used for the first time starts empty, like a new game. The party that was active is kept as it is and comes back with the same command.',

  mergeFrom: 'Parties to merge',
  mergeFromNone: 'Nothing to choose from yet.',
  mergeInto: 'Merge into',
  mergeHelp: 'The chosen parties join the one they are merged into, which becomes the active party: their characters move over, their storages and funds are added together, their finished research is shared. A character both parties have stays as the receiving party has it, and the other copy’s items go to the storage. A merged party no longer exists.',
  mergeSelf: 'A party cannot be merged into itself.',
}
