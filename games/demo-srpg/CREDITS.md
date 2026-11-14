# Credits

Everything in this game is original to OpenTacticsEngine (GPL-3.0, generated
by `tools/art/`), except the art of the "Detailed 3D" style, which comes
from public-domain packs (Creative Commons Zero, CC0 1.0: free for any use,
no credit required; credited here with thanks).

| Files | From | Licence |
|---|---|---|
| `models/detailed/*.glb` | KayKit Adventurers Character Pack 1.0 by Kay Lousberg (www.kaylousberg.com), https://github.com/KayKit-Game-Assets/KayKit-Character-Pack-Adventures-1.0 : Knight, Barbarian, Mage and Rogue_Hooded, cut down to the parts and four animations each unit uses | CC0 1.0 |
| `art/detailed_tiles.png` (grass, forest floor, rock, planks) | ambientCG (https://ambientcg.com): Grass004, Ground037, Rock030, Planks012, colour maps at 1K | CC0 1.0 |
| `art/detailed_tiles.png` (water) | drawn by `tools/art/make_detailed_style.py` | GPL-3.0 |

To rebuild the style: download the packs, then
`python3 tools/art/make_detailed_style.py --kaykit <folder with the .glb files> --textures <folder with the *_Color.jpg files>`.
