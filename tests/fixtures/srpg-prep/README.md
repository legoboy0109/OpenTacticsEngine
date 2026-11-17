A small three-chapter game for the preparation screen (docs/EVENTS.md "Preparation"): `tests/test_prep_*.cpp` and
`tests/srpg_player_prep_test.py` use it (copied to a temp folder, never changed). Original neutral names; battles are won
at the start of round 2.

- `meadow` (chapter 1): no preparation. Four named units (sera, tomas, wren, bram), a potion in the storage, 60 gold.
- `orchard` (chapter 2): preparation with 4 slots (slot 4 is a pond: only the glider can stand there), limit 3, sera
  required on slot 1, a fixed unit `guard`, a map character `ilsa` the roster does not know yet, a shop (potion, tonic),
  25 gold from the map.
- `hilltop` (chapter 3): preparation with 4 slots, limit 4, `wren` forbidden, no shop.
