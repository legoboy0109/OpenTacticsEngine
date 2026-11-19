// Each section's code is a separate file fetched when the section first opens. If that fetch fails (helper
// stopped, offline, stale tab after a rebuild) the page must say so plainly instead of going blank.
import { expect, test } from './fixtures'

test('a section whose code cannot be fetched shows a plain error, and the rest of the editor stays', async ({ page, game }) => {
  const id = await game.create('strategy-rpg', 'Lazy failure')
  await page.route(/\/app\/RulesScreen-[^/]*\.js/, (route) => route.abort())
  await page.goto(`/p/${id}/game`)
  await expect(page.getByRole('navigation').first()).toBeVisible()
  await page.goto(`/p/${id}/rules`)
  await expect(page.getByRole('alert')).toContainText('Could not load this section')
  await expect(page.getByRole('navigation').first()).toBeVisible()
})
