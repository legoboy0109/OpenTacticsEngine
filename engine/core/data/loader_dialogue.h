// The dialogue keys shared by game.json, map events and campaign scenes (docs/EVENTS.md "Speech bubbles", "Notices", "Portrait
// expressions", "Naming a character", "Loops"): read here so both story loaders accept exactly the same shapes and say the same
// words when they refuse something.
#pragma once
#include <string>

#include "core/data/database.h"
#include "core/data/loader_util.h"
#include "core/data/scene_fx.h"

namespace ote {

// game.json "dialogue": { "style": "box" | "bubble", "portrait": "beside" | "inside" | "none", "names": true }.
bool readDialogueLook(const LoadCtx& c, const Json& root, Database& db);
// A say / choice line's "face": N (1..kMaxFaces, and no more than the longest portrait "expressions" list).
bool readFace(const LoadCtx& c, const Database& db, const Json& e, const std::string& at, SceneFx& fx);
// A choice's "notice": true (the question in the notice window: no speaker, portrait or face) and "cursor": N (1..answers written).
bool readChoiceLook(const LoadCtx& c, const Database& db, const Json& e, const std::string& at, size_t answers, SceneFx& fx);
// Refuses a malformed "{name:...}" in a piece of text the player will show.
bool checkNameTokens(const LoadCtx& c, const std::string& text, const std::string& at);
// { "notice": "text", "auto": ms }: the text (checked) into `text`, the keys into `fx` (fx.notice set).
bool readNotice(const LoadCtx& c, const Database& db, bool mapEvent, const Json& e, const std::string& at, std::string& text, SceneFx& fx);
// { "loop": [ ... ], "max": N }: the most rounds (default kDefaultLoopMax, 1..kMaxLoopMax). The block itself is read by the caller.
bool readLoopHead(const LoadCtx& c, const Json& e, const std::string& at, int& max);
// True when `key` (notice, loop, break, askName) is a command of its own in `e`. A "notice": true / false is a choice's modifier. A "loop": true / false is not the loop command (it
// reads as a music option somebody guessed, and gets the "unknown key" message of the command it sits on).
bool isDialogueCommandIn(const Json& e, const char* key);
// { "break": true } (only inside a loop: `inLoop`).
bool readBreak(const LoadCtx& c, const Json& e, const std::string& at, bool inLoop);

}  // namespace ote
