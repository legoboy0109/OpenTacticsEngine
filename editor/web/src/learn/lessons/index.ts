// Every lesson of the editor, in the order the Learn list shows them. To add a lesson: write
// lessons/<id>.ts (see "How to write a lesson" in docs/TUTORIAL.md) and add it here.
import type { Lesson } from '../types'
import { firstGame } from './first-game'
import { structures } from './structures'
import { economy } from './economy'
import { art } from './art'
import { hexagons } from './hexagons'

export const LESSONS: Lesson[] = [
  firstGame,
  structures,
  economy,
  art,
  hexagons,
]

/** The beginner lesson the first-run offer shows. */
export const FIRST_LESSON_ID = firstGame.id

export const lessonById = (id: string, lessons: Lesson[] = LESSONS): Lesson | undefined => lessons.find((l) => l.id === id)
