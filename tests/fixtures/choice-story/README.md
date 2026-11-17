A tiny story game for the dialogue-choice tests (docs/EVENTS.md "Dialogue choices"): `tests/choice_player_test.py` plays it
through the player. Original neutral names; the rules are as small as possible (nobody can hurt anybody: the damage
formula is 0). Not a demo: do not grow it.

- `ridge` (chapter 1): a question when the battle starts (the stranger), a question in the middle of the battle (the
  gate, when Mara steps on the marked square, with a hidden answer and a nested question), a win at the start of round 2.
- `vale`, `summit`: empty battles won at round 2.
- Campaign: `ch1` has a question in its scene before the battle (which pack) and one after it (which road), whose
  answers jump to `ch2` or `ch3`; both lead to `ch4`, whose scenes read the earlier answers and ask one more question.
