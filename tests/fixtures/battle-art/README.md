# battle-art

A copy of `srpg-rename` whose `data/graphics.json` carries a battle scene with every key of the loader's `"battle"` section
(docs/BATTLE_SCENE.md): sets with all twelve animation slots, effects of every kind, assignments, backgrounds, platforms, a fixed
stage with range bands, and the scene timing. The native editor's tests read it: `native_test_conformance` checks that the schema
knows every key it holds, `native_test_form_battle` that the form shows all of it and leaves nothing under "Other keys". Not a demo.
