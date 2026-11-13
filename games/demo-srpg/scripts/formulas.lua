-- Example of a scripted formula: game.json's "damage" formula calls this
-- function, so a creator can put longer logic here instead of in a one-line
-- expression. Arguments are the same variables formulas always get:
--   a, b  attacker / defender stats      w   weapon props
--   tb    terrain props under defender   vs  weapon "versus" multiplier
function srpg_damage(a, b, w, tb, vs)
  return (a.str + w.might * vs) - b.def - (tb.defense or 0)
end
