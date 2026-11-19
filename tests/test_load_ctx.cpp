// LoadCtx::integerIn and integerFrom (engine/core/data/loader_util.h): the two "whole number in a range" readers
// that many loaders share. The messages are mirrored by the editor, so each is asserted exactly.
#include <string>

#include "core/base/json.h"
#include "core/data/loader_util.h"
#include "test.h"

using namespace ote;

namespace {

Json parse(const char* text) {
  Json j;
  std::string err;
  Json::parse(text, j, err);
  return j;
}

// Runs the reader against `json`; returns "ok:<value>" or the whole error text.
template <class Read>
std::string run(const char* json, Read read) {
  std::string err;
  LoadCtx c(err, "data/x.json");
  const Json j = parse(json);
  int out = -7;
  if (!read(c, j, out)) return err;
  return "ok:" + std::to_string(out);
}

}  // namespace

TEST(load_ctx_integer_in_words_every_refusal_the_same_way) {
  const auto read = [](const LoadCtx& c, const Json& j, int& out) { return c.integerIn(j, "k", "w", 1, 9, out); };
  const std::string bad = "data/x.json: w: \"k\" must be a whole number from 1 to 9";
  CHECK(run("{\"k\":5}", read) == "ok:5");
  CHECK(run("{}", read) == "ok:-7");  // absent: untouched, no error
  CHECK(run("{\"k\":0}", read) == bad);
  CHECK(run("{\"k\":10}", read) == bad);
  CHECK(run("{\"k\":2.5}", read) == bad);
  CHECK(run("{\"k\":\"5\"}", read) == bad);
  const auto required = [](const LoadCtx& c, const Json& j, int& out) { return c.integerIn(j, "k", "w", -3, 9, out, true); };
  CHECK(run("{}", required) == "data/x.json: w: missing required \"k\"");
  CHECK(run("{\"k\":-4}", required) == "data/x.json: w: \"k\" must be a whole number from -3 to 9");
}

TEST(load_ctx_integer_from_keeps_integers_own_message_for_a_wrong_type) {
  const auto read = [](const LoadCtx& c, const Json& j, int& out) { return c.integerFrom(j, "k", "w", 0, 4, out); };
  CHECK(run("{\"k\":4}", read) == "ok:4");
  CHECK(run("{}", read) == "ok:-7");
  CHECK(run("{\"k\":5}", read) == "data/x.json: w: \"k\" must be from 0 to 4");
  CHECK(run("{\"k\":-1}", read) == "data/x.json: w: \"k\" must be from 0 to 4");
  CHECK(run("{\"k\":\"4\"}", read) == "data/x.json: w: \"k\" must be a whole number");
  CHECK(run("{\"k\":1.5}", read) == "data/x.json: w: \"k\" must be a whole number");
}
