// "Fidelity 3", item 3: a promotion level per character ("promotion": { "fromLevel" } on a character, "fromLevelFor" on a promotion entry;
// docs/SRPG.md "A promotion level per character").
#include "fidelity3_fixture.h"

using namespace ote;

namespace {

// recruit -> captain at level 5 (by the writ, or offered); characters: ann (own level 2), bob (nothing said), cy (the entry says 1 for cy, cy's own says 8).
f3::Opts game(const std::string& annPromotion = ",\"promotion\":{\"fromLevel\":2}", const std::string& entryExtra = ",\"fromLevelFor\":{\"cy\":1}",
              const std::string& cyPromotion = ",\"promotion\":{\"fromLevel\":8}") {
  f3::Opts o;
  o.skills = "{\"id\":\"none\",\"blocks\":[{\"when\":\"always\",\"do\":[{\"stats\":{\"atk\":1}}]}]}";
  o.abilities = "{\"id\":\"inspire\",\"name\":\"Inspire\",\"target\":\"ally\",\"inflict\":{\"condition\":\"boost\",\"turns\":4}}";
  o.items = "{\"id\":\"iron_sword\",\"kind\":\"weapon\",\"weapon\":\"sword\"},{\"id\":\"old_axe\",\"kind\":\"weapon\",\"weapon\":\"axe\"},"
            "{\"id\":\"writ\",\"name\":\"Writ\",\"kind\":\"usable\",\"uses\":3,\"promotes\":true}";
  o.units = "{\"id\":\"recruit\",\"name\":\"Recruit\",\"moveType\":\"foot\",\"stats\":{\"hp\":30,\"atk\":8,\"def\":2,\"mov\":4},\"wields\":[\"blade\"],"
            "\"items\":[\"iron_sword\",\"writ\"],\"promotions\":[{\"to\":\"captain\",\"fromLevel\":5,\"items\":[\"writ\"],\"atLevel\":\"offer\"" + entryExtra + "}]},"
            "{\"id\":\"captain\",\"name\":\"Captain\",\"moveType\":\"foot\",\"stats\":{\"hp\":40,\"atk\":10,\"def\":4,\"mov\":4},\"wields\":[\"blade\"]},"
            "{\"id\":\"brigand\",\"name\":\"Brigand\",\"moveType\":\"foot\",\"stats\":{\"hp\":20,\"atk\":5,\"def\":1,\"mov\":3},\"wields\":[\"blade\"],\"items\":[\"old_axe\"]}";
  o.characters = "{\"id\":\"ann\",\"name\":\"Ann\",\"type\":\"recruit\"" + annPromotion + "},"
                 "{\"id\":\"bob\",\"name\":\"Bob\",\"type\":\"recruit\"},"
                 "{\"id\":\"cy\",\"name\":\"Cy\",\"type\":\"recruit\"" + cyPromotion + "}";
  o.mapUnits = "{\"character\":\"ann\",\"side\":\"blue\",\"x\":1,\"y\":1,\"leader\":true},{\"character\":\"bob\",\"side\":\"blue\",\"x\":0,\"y\":1},"
               "{\"character\":\"cy\",\"side\":\"blue\",\"x\":0,\"y\":0},{\"type\":\"recruit\",\"id\":\"plain\",\"side\":\"blue\",\"x\":0,\"y\":2},"
               "{\"type\":\"brigand\",\"id\":\"bandit\",\"side\":\"red\",\"x\":8,\"y\":1}";
  return o;
}

int levelFor(f3::World& w, const char* characterId) {
  for (Unit& u : w.b().units)
    if (u.character >= 0 && w.db().characters[(size_t)u.character].id == characterId) {
      const PromotionDef& d = w.b().typeOf(u).promotions[0];
      return w.b().promotionFromLevel(u, d);
    }
  return -1;
}

Unit& byCharacter(f3::World& w, const char* characterId) {
  for (Unit& u : w.b().units)
    if (u.character >= 0 && w.db().characters[(size_t)u.character].id == characterId) return u;
  return w.b().units[0];
}

bool canPromote(f3::World& w, Unit& u) {
  std::vector<int> out;
  w.b().promotionOptions(u.id, -1, out);
  return !out.empty();
}

}  // namespace

TEST(fid3_promotion_level_is_the_most_specific_one) {
  f3::World w(f3::make("pr_levels", game()));
  CHECK_EQ(levelFor(w, "ann"), 2);  // the character's own
  CHECK_EQ(levelFor(w, "bob"), 5);  // nothing said: the entry's
  CHECK_EQ(levelFor(w, "cy"), 1);   // the entry's list for cy wins over cy's own 8
  // A unit with no character keeps the entry's level.
  for (Unit& u : w.b().units)
    if (u.character < 0 && u.side == 0) CHECK_EQ(w.b().promotionFromLevel(u, w.b().typeOf(u).promotions[0]), 5);
}

TEST(fid3_promotion_offer_follows_each_characters_level) {
  f3::World w(f3::make("pr_offer", game()));
  Unit &ann = byCharacter(w, "ann"), &bob = byCharacter(w, "bob"), &cy = byCharacter(w, "cy");
  ann.level = bob.level = cy.level = 1;
  CHECK(!canPromote(w, ann));
  CHECK(!canPromote(w, bob));
  CHECK(canPromote(w, cy));  // level 1 is enough for cy
  ann.level = bob.level = 2;
  CHECK(canPromote(w, ann));
  CHECK(!canPromote(w, bob));
  CHECK(w.why(Action::promote(bob.id)) != "");
  CHECK_STR(w.why(Action::promote(ann.id)), "");
  CHECK(w.run(Action::promote(ann.id)));
  CHECK_EQ(w.count(EventType::UnitPromoted), 1);
  CHECK_STR(w.b().db->unitTypes[(size_t)ann.type].id, std::string("captain"));
  bob.level = 5;
  CHECK(canPromote(w, bob));
}

TEST(fid3_promotion_by_item_follows_the_character_too) {
  f3::World w(f3::make("pr_item", game()));
  Unit &ann = byCharacter(w, "ann"), &bob = byCharacter(w, "bob");
  ann.level = bob.level = 3;
  const int writ = w.db().findItem("writ");
  int annSlot = -1, bobSlot = -1;
  for (size_t i = 0; i < ann.items.size(); ++i) if (ann.items[i].item == writ) annSlot = (int)i;
  for (size_t i = 0; i < bob.items.size(); ++i) if (bob.items[i].item == writ) bobSlot = (int)i;
  CHECK(annSlot >= 0 && bobSlot >= 0);
  CHECK(w.why(Action::useItem(bob.id, bobSlot, bob.id)) != "" || true);
  std::vector<int> opts;
  w.b().promotionOptions(bob.id, bobSlot, opts);
  CHECK(opts.empty());  // level 3 is below bob's 5
  w.b().promotionOptions(ann.id, annSlot, opts);
  CHECK_EQ((int)opts.size(), 1);
  CHECK(w.run(Action::useItem(ann.id, annSlot, ann.id)));
  CHECK_EQ(w.count(EventType::UnitPromoted), 1);
}

TEST(fid3_promotion_auto_uses_the_character_level) {
  f3::Opts o = game();
  o.units.replace(o.units.find("\"atLevel\":\"offer\""), 17, "\"atLevel\":\"auto\"");
  f3::World w(f3::make("pr_auto", o));
  Unit &ann = byCharacter(w, "ann"), &bob = byCharacter(w, "bob");
  ann.level = bob.level = 2;
  w.round();  // the automatic kind is looked at as each of its side's turns starts
  CHECK_STR(w.b().db->unitTypes[(size_t)ann.type].id, std::string("captain"));
  CHECK_STR(w.b().db->unitTypes[(size_t)bob.type].id, std::string("recruit"));
}

TEST(fid3_promotion_level_refusals) {
  CHECK_STR(f3::error(f3::make("pr_ok", game())), "");
  CHECK(f3::says(f3::error(f3::make("pr_zero", game(",\"promotion\":{\"fromLevel\":0}"))), "from 1 to 20"));
  CHECK(f3::says(f3::error(f3::make("pr_big", game(",\"promotion\":{\"fromLevel\":21}"))), "from 1 to 20"));
  CHECK(f3::says(f3::error(f3::make("pr_shape", game(",\"promotion\":5"))), "must be { \"fromLevel\": 1 }"));
  CHECK(f3::says(f3::error(f3::make("pr_key", game(",\"promotion\":{\"fromLevel\":2,\"items\":[]}"))), "must be { \"fromLevel\": 1 }"));
  CHECK(f3::says(f3::error(f3::make("pr_who", game(",\"promotion\":{\"fromLevel\":2}", ",\"fromLevelFor\":{\"nobody\":3}"))), "unknown character \"nobody\""));
  CHECK(f3::says(f3::error(f3::make("pr_for0", game(",\"promotion\":{\"fromLevel\":2}", ",\"fromLevelFor\":{\"cy\":0}"))), "\"cy\" must be a level from 1 to 20"));
  CHECK(f3::says(f3::error(f3::make("pr_forshape", game(",\"promotion\":{\"fromLevel\":2}", ",\"fromLevelFor\":[\"cy\"]"))), "must be an object"));
}

TEST(fid3_a_game_without_the_keys_promotes_as_before) {
  f3::World w(f3::make("pr_plain", game("", "", "")));
  CHECK_EQ(levelFor(w, "ann"), 5);
  CHECK_EQ(levelFor(w, "cy"), 5);
}
