// Hexagon screenshots for design review (npm run screenshots; skipped otherwise): maps at several zooms in both layouts, with and
// without art, the Structures screen, the Art screen, the picture picker with the 8 x 8 join block, the Game screen.
import type { Page } from '@playwright/test'
import { clickPiece, makePng, picker, pickerGrid, seedArt } from './artkit'
import { editorUrl, test } from './fixtures'
import { canvas, openMap, pickStructure, tool } from './mapkit'
import { hexGame, hoverHex, type Layout } from './hexkit'

const OUT = process.env.SHOTS_DIR ?? 'screenshots'
test.skip(!process.env.SHOTS, 'only when SHOTS=1')

async function shot(page: Page, name: string) {
  await page.waitForTimeout(350)
  await page.screenshot({ path: `${OUT}/${name}.png` })
}

/** Pieces with a hexagon painted in a flat colour per piece (transparent corners), so the art's own shape is visible. */
const hexSheet = (layout: Layout, cols: number, rows: number) => {
  const [cw, ch] = layout === 'pointy' ? [24, 28] : [28, 24]
  return { cw, ch, png: makePng(cols * cw, rows * ch, (x, y) => {
    const px = x % cw, py = y % ch, c = Math.floor(x / cw), r = Math.floor(y / ch)
    // inside a rough hexagon of the piece's box (corners cut off)
    const fx = Math.abs(px - cw / 2) / (cw / 2), fy = Math.abs(py - ch / 2) / (ch / 2)
    const inside = layout === 'pointy' ? fx <= 1 && fy + fx / 2 <= 1 : fy <= 1 && fx + fy / 2 <= 1
    return inside ? [(c * 53 + 60) % 256, (r * 71 + 100) % 256, ((c + r) * 37 + 60) % 256, 255] : [0, 0, 0, 0]
  }) }
}

for (const layout of ['pointy', 'flat'] as Layout[]) {
  test(`${layout}: hexagon maps with art, structures and zoom`, async ({ page, request }) => {
    await page.setViewportSize({ width: 1400, height: 900 })
    const id = hexGame(layout)
    const s = hexSheet(layout, 10, 9)
    await seedArt(request, id, { tiles: s.png }, {
      sheets: [{ id: 'tiles', file: 'art/tiles.png', cellWidth: s.cw, cellHeight: s.ch, name: 'Tiles' }],
      terrain: { plain: { sheet: 'tiles', variants: [[0, 0], [1, 0]] }, road: { sheet: 'tiles', join: [1, 1] }, sea: { sheet: 'tiles', frames: [[2, 0], [3, 0]], frameMs: 400 } },
      structures: { city: { sheet: 'tiles', cell: [4, 0] }, hq: { sheet: 'tiles', cell: [5, 0] } },
    })
    await openMap(page, id, 'skirmish')
    await shot(page, `hex-${layout}-art-fit`)
    await tool(page, 'Structures').click()
    await pickStructure(page, 'Headquarters').click()
    await hoverHex(page, layout, 7, 1)
    await shot(page, `hex-${layout}-art-ghost`)
    await canvas(page).hover({ position: { x: 300, y: 200 } })
    for (let k = 0; k < 4; k++) await page.mouse.wheel(0, -240)
    await shot(page, `hex-${layout}-art-zoom`)
  })

  test(`${layout}: the Structures, Art and picker screens`, async ({ page, request }) => {
    await page.setViewportSize({ width: 1180, height: 900 })
    const id = hexGame(layout)
    const s = hexSheet(layout, 10, 9)
    await seedArt(request, id, { tiles: s.png }, {
      sheets: [{ id: 'tiles', file: 'art/tiles.png', cellWidth: s.cw, cellHeight: s.ch, name: 'Tiles' }],
      terrain: { road: { sheet: 'tiles', join: [1, 1] } }, structures: { hq: { sheet: 'tiles', cell: [5, 0] } },
    })
    await page.goto(editorUrl(id, 'structures', 'hq'))
    await shot(page, `hex-${layout}-structures`)
    await page.goto(editorUrl(id, 'art', 'tiles'))
    await shot(page, `hex-${layout}-art-screen`)
    await page.goto(editorUrl(id, 'terrain', 'road'))
    await page.getByRole('button', { name: 'Change picture…' }).click()
    await clickPiece(page, pickerGrid(page), 1, 1)
    await picker(page).getByText('Which piece goes where').scrollIntoViewIfNeeded()
    await shot(page, `hex-${layout}-picker-join`)
    await page.goto(editorUrl(id, 'game'))
    await shot(page, `hex-${layout}-game`)
  })
}
