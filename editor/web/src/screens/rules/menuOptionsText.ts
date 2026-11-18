// Wording of the Menu options card on the Rules screen (placeholder wording for the owner to review).
import type { AttackWeaponChoice, MenuUnavailable } from '../../model/menuOptions'

export const MO = {
  title: 'Menu options',
  help: 'How the player’s action menu and the Attack step behave. Both only change what is shown and asked; the rules, saves and computer players are not affected. A player can still choose their own way for the first one in the player’s settings.',
  unavailable: 'Actions with nothing to act on',
  unavailableChoices: {
    grey: { label: 'Greyed out', help: 'An ability or a staff with nobody in reach to use it on stays in the menu, greyed, with the reason.' },
    hide: { label: 'Left out', help: 'An action with nothing to act on (an ability or a staff with nobody to use it on, a door with no key, Drop with no free square) is not in the menu at all. Ones that are off for another reason (used up, still recovering, not allowed before moving, a full pack) stay greyed with the reason. Wait is always there.' },
  } as Record<MenuUnavailable, { label: string; help: string }>,
  attack: 'Choosing Attack',
  attackChoices: {
    equipped: { label: 'Go straight to the targets', help: 'Attack shows the enemies in reach; the arrows also step through the weapons that reach them.' },
    pick: { label: 'Ask for the weapon first', help: 'Attack opens a list of the weapons the unit carries that reach somebody (the weapon in hand is highlighted). Choosing one shows the enemies it reaches; confirming puts it in hand, then attacks. Back goes one step at a time and changes nothing.' },
  } as Record<AttackWeaponChoice, { label: string; help: string }>,
}
