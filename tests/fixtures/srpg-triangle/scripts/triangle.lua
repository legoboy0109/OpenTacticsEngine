-- The weapon triangle of the fixture: props.kind is 1 = sword, 2 = axe, 3 = lance; sword beats axe beats lance
-- beats sword. Returns +1 (my weapon has the advantage), -1 (disadvantage) or 0. `fw.none` is set when the other
-- fighter has no usable weapon (docs/SRPG.md "Foe's weapon").
function tri(w, fw)
  if fw.none or not w.kind then return 0 end
  local d = (fw.kind - w.kind) % 3
  if d == 1 then return 1 elseif d == 2 then return -1 end
  return 0
end
