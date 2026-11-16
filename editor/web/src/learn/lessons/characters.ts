// Lesson: "Characters and skills". Starts from the blank template: no characters, no skills, one unit type (soldier), stats Hit
// points and Move only, one map ("start") with a blue and a red soldier, levelling up off. (The strategy-rpg template ships
// with all of this switched on and filled, so no step would be real work there.) Nothing below is already true at the start.
import { featureEnabled } from '../../model/rules'
import { firstMapId } from '../helpers'
import { charactersOnMap, firstOwnSkillId, charactersWithBonus, charactersWithLearned, charactersWithSkill, ownSkillsEdited, starterSkillsAdded } from '../helpers/characters'
import type { Lesson } from '../types'

const firstCharacter = (doc: { characters: Array<{ id: string }> | null }) => doc.characters?.[0]?.id

export const characters: Lesson = {
  id: 'characters',
  title: 'Characters and skills',
  description: 'Make a named character with its own strengths, place it on the map, and give it skills.',
  minutes: 9,
  template: 'blank',
  playable: true,
  summary: [
    'A named character with a personal bonus, standing on the map.',
    'A set of starter skills, one given to the character and one it learns at a level.',
    'A small skill of your own, built with the rule builder.',
    'A game that you played, with the character in it.',
  ],
  steps: [
    {
      id: 'characters-on',
      title: 'Switch on characters',
      text: 'A character is a named person with their own strengths, not just another soldier. Games start without them. On the Rules screen, under Strategy-RPG features, switch on “This game has characters”.',
      where: { section: 'rules' },
      point: 'srpg-characters',
      done: (doc) => doc.characters !== null,
    },
    {
      id: 'new-character',
      title: 'Make a character',
      text: 'On the Characters screen, click New character and give them a name, like Mira. Their class is the kind of unit they are; the Soldier is the only one for now. Then, under Stats, type a number in the Personal column, like 5 for HP, to make them stronger than other soldiers.',
      where: { section: 'characters', item: firstCharacter },
      point: 'character-stats|new-item',
      // Typing "12" passes through "1", so the tick waits for a short pause.
      settleMs: 1200,
      done: (doc) => charactersWithBonus(doc) >= 1,
    },
    {
      id: 'place',
      title: 'Put them on the map',
      text: 'On the Maps screen, choose the Units tool, pick Blue, open the Characters tab, choose your character and click an empty square.',
      where: { section: 'maps', item: firstMapId },
      point: 'unit-characters-tab|tool-units',
      done: (doc) => charactersOnMap(doc) >= 1,
    },
    {
      id: 'skills-on',
      title: 'Switch on skills',
      text: 'Skills are bonuses and special effects that characters and units can have. Back on the Rules screen, switch on “This game has skills”.',
      where: { section: 'rules' },
      point: 'srpg-skills',
      done: (doc) => doc.skills !== null,
    },
    {
      id: 'starter',
      title: 'Add the starter skills',
      text: 'On the Skills screen, click Add starter skills, then Add. You get a ready-made set to browse and use.',
      where: { section: 'skills' },
      point: 'starter-open',
      done: (doc) => starterSkillsAdded(doc) >= 1,
    },
    {
      id: 'personal-skill',
      title: 'Give them a skill',
      text: 'On your character’s screen, find the Skills card and choose a skill from the list, such as Tough. They always have it.',
      where: { section: 'characters', item: firstCharacter },
      point: 'character-skills-add',
      done: (doc) => charactersWithSkill(doc) >= 1,
    },
    {
      id: 'levelling-on',
      title: 'Switch on levelling up',
      text: 'A skill can also be learned later, when a unit reaches a level. That needs levelling up. On the Rules screen, switch on “Experience and levels”.',
      where: { section: 'rules' },
      point: 'progression',
      done: (doc) => featureEnabled(doc, 'progression'),
    },
    {
      id: 'learned-skill',
      title: 'A skill learned at a level',
      text: 'Back on your character’s Skills card, click Add a learned skill, then pick a level and a skill, such as level 3 and Light Step.',
      where: { section: 'characters', item: firstCharacter },
      point: 'character-skills-learns',
      done: (doc) => charactersWithLearned(doc) >= 1,
    },
    {
      id: 'own-skill',
      title: 'Make a skill of your own',
      text: 'On the Skills screen, click New skill and name it. Its one rule starts as “always +1”. Change it with the rule builder: pick when it applies and what it does.',
      where: { section: 'skills', item: firstOwnSkillId },
      point: 'skill-rule|new-item',
      // Typing a number passes through other numbers on the way.
      settleMs: 1200,
      done: (doc) => ownSkillsEdited(doc) >= 1,
    },
    {
      id: 'play',
      title: 'Play it',
      text: 'Click Play. Your character stands on the map. Click your character, then choose Info in its menu to see their details: the personal bonus and the skills.',
      point: 'play',
      done: (_doc, _screen, signals) => signals.has('played'),
    },
  ],
}
