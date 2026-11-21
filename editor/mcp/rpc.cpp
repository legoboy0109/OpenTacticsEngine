#include "rpc.h"

#include <algorithm>
#include <cstdlib>
#include <exception>

#include "resources.h"

namespace ote_mcp {

namespace {

constexpr const char* kModern = "2026-07-28";
const char* const kLegacy[] = {"2025-11-25", "2025-06-18", "2025-03-26", "2024-11-05"};
constexpr size_t kMaxResultChars = 400000;  // a tool result bigger than this is replaced by advice to ask for less

// JSON-RPC and MCP error codes.
constexpr int kParseError = -32700, kInvalidRequest = -32600, kMethodNotFound = -32601, kInvalidParams = -32602, kInternal = -32603;
constexpr int kUnsupportedVersion = -32022;

bool isLegacyVersion(const std::string& v) {
  for (const char* l : kLegacy) if (v == l) return true;
  return false;
}

Json supportedVersions() {
  Json a = jarr();
  a.push(jstr(kModern));
  for (const char* l : kLegacy) a.push(jstr(l));
  return a;
}

Json errorObject(const Json& id, int code, const std::string& message, const Json& data = Json()) {
  Json r = jobj();
  r.set("jsonrpc", jstr("2.0"));
  r.set("id", id);  // null when the request id could not be read
  Json e = jobj();
  e.set("code", jnum(code));
  e.set("message", jstr(message));
  if (!data.isNull()) e.set("data", data);
  r.set("error", e);
  return r;
}

Json resultObject(const Json& id, const Json& result) {
  Json r = jobj();
  r.set("jsonrpc", jstr("2.0"));
  r.set("id", id);
  r.set("result", result);
  return r;
}

// Dates as YYYY-MM-DD compare correctly as text.
bool atLeast(const std::string& version, const char* than) { return version >= than; }

}  // namespace

std::string Server::errorLine(int code, const std::string& message) { return sanitizeUtf8(errorObject(Json(), code, message).dump(0)); }

std::string Server::handleLine(const std::string& line) {
  try {
    Json msg;
    std::string perr;
    if (!Json::parse(line, msg, perr)) return sanitizeUtf8(errorObject(Json(), kParseError, "Parse error: " + perr).dump(0));
    if (msg.isArray()) {
      // A batch (allowed up to 2025-03-26): answer each request, in order.
      if (msg.size() == 0) return sanitizeUtf8(errorObject(Json(), kInvalidRequest, "Invalid Request: empty batch").dump(0));
      Json out = jarr();
      for (const Json& m : msg.items()) {
        bool has = false;
        Json r = handleOne(m, has);
        if (has) out.push(r);
      }
      return out.size() == 0 ? std::string() : sanitizeUtf8(out.dump(0));
    }
    bool has = false;
    const Json r = handleOne(msg, has);
    return has ? sanitizeUtf8(r.dump(0)) : std::string();
  } catch (const std::exception& e) {
    logError(std::string("internal error: ") + e.what());
    return sanitizeUtf8(errorObject(Json(), kInternal, "Internal error").dump(0));
  } catch (...) {
    logError("internal error (unknown exception)");
    return sanitizeUtf8(errorObject(Json(), kInternal, "Internal error").dump(0));
  }
}

Json Server::handleOne(const Json& msg, bool& hasResponse) {
  hasResponse = true;
  if (!msg.isObject()) return errorObject(Json(), kInvalidRequest, "Invalid Request: a message must be a JSON object");
  const bool hasMethod = msg["method"].isString();
  if (!hasMethod) {
    // A response from the client (we send no requests, so there is nothing to match): ignore it.
    if (msg.has("result") || msg.has("error")) { hasResponse = false; return Json(); }
    return errorObject(msg["id"].isString() || msg["id"].isNumber() ? msg["id"] : Json(), kInvalidRequest, "Invalid Request: \"method\" must be a string");
  }
  const std::string method = msg["method"].asString();
  const bool isNotification = !msg.has("id");
  if (!isNotification && !msg["id"].isString() && !msg["id"].isNumber()) return errorObject(Json(), kInvalidRequest, "Invalid Request: \"id\" must be a string or a number");
  const Json id = isNotification ? Json() : msg["id"];
  if (msg.has("params") && !msg["params"].isObject() && !msg["params"].isNull()) {
    if (isNotification) { hasResponse = false; return Json(); }
    return errorObject(id, kInvalidParams, "Invalid params: params must be an object");
  }
  const Json& params = msg["params"];
  if (isNotification) {
    hasResponse = false;  // notifications/initialized, notifications/cancelled and the rest need no action: every call is synchronous
    return Json();
  }

  // Which revision does this request speak?
  Era era;
  era.version = legacyVersion_;
  const Json& meta = params["_meta"];
  if (method != "initialize" && meta.isObject() && meta.has("io.modelcontextprotocol/protocolVersion")) {
    const Json& pv = meta["io.modelcontextprotocol/protocolVersion"];
    if (!pv.isString()) return errorObject(id, kInvalidParams, "Invalid params: _meta io.modelcontextprotocol/protocolVersion must be a string");
    const std::string v = pv.asString();
    if (v == kModern) {
      if (!meta["io.modelcontextprotocol/clientCapabilities"].isObject())
        return errorObject(id, kInvalidParams, "Invalid params: _meta io.modelcontextprotocol/clientCapabilities is required in revision 2026-07-28");
      era.modern = true;
      era.version = v;
    } else if (isLegacyVersion(v)) {
      era.version = v;
    } else {
      Json data = jobj();
      data.set("supported", supportedVersions());
      data.set("requested", jstr(v));
      return errorObject(id, kUnsupportedVersion, "Unsupported protocol version", data);
    }
  }

  bool isError = false;
  const Json result = dispatch(method, params, id, era, isError);
  return result;
}

Json Server::capabilities(bool modern) const {
  Json c = jobj();
  Json tools = jobj();
  tools.set("listChanged", jbool(false));
  c.set("tools", tools);
  Json res = jobj();
  res.set("subscribe", jbool(false));
  res.set("listChanged", jbool(false));
  c.set("resources", res);
  Json pr = jobj();
  pr.set("listChanged", jbool(false));
  c.set("prompts", pr);
  if (!modern) c.set("logging", jobj());  // logging/setLevel is accepted; messages go to stderr (deprecated in 2026-07-28)
  return c;
}

std::string Server::instructions() const {
  return "ote-mcp edits OpenTacticsEngine games (folders of JSON data files for turn-based tactics games) through the editor's own rules. "
         "Start with list_games (or list_templates + create_game), then get_overview. Use describe before writing: it lists the allowed keys, ranges and the ids that exist. "
         "Write with set / add_entry / rename_id / set_tiles; every write is checked against the schema and followed by the engine's project check. Use dryRun=true to preview a diff. "
         "validate re-checks; render_map shows a map; simulate gives balance numbers. Results are paged: follow nextOffset.";
}

void Server::finishResult(Json& result, const Era& era) const {
  if (!era.modern) return;
  result.set("resultType", jstr("complete"));
  Json meta = result["_meta"].isObject() ? result["_meta"] : jobj();
  Json info = jobj();
  info.set("name", jstr(kServerName));
  info.set("version", jstr(kServerVersion));
  meta.set("io.modelcontextprotocol/serverInfo", info);
  result.set("_meta", meta);
}

Json Server::initialize(const Json& params) {
  // Handshake revisions: the client offers its newest; we answer the same if we know it, else our newest handshake revision.
  const std::string asked = params["protocolVersion"].asString();
  legacyVersion_ = isLegacyVersion(asked) ? asked : kLegacy[0];
  Json r = jobj();
  r.set("protocolVersion", jstr(legacyVersion_));
  r.set("capabilities", capabilities(false));
  Json info = jobj();
  info.set("name", jstr(kServerName));
  info.set("version", jstr(kServerVersion));
  if (atLeast(legacyVersion_, "2025-06-18")) info.set("title", jstr("OpenTacticsEngine game editor"));
  r.set("serverInfo", info);
  r.set("instructions", jstr(instructions()));
  logInfo("client initialised, protocol " + legacyVersion_ + (params["clientInfo"]["name"].isString() ? ", client " + params["clientInfo"]["name"].asString() : ""));
  return r;
}

Json Server::discover(const Era& era) {
  Json r = jobj();
  r.set("supportedVersions", supportedVersions());
  r.set("capabilities", capabilities(true));
  r.set("instructions", jstr(instructions()));
  r.set("ttlMs", jnum(3600000));
  r.set("cacheScope", jstr("public"));
  finishResult(r, era);
  return r;
}

Json Server::listTools(const Json& params, const Era& era) {
  Json arr = jarr();
  for (const Tool& t : tools_.all()) {
    if (ctx_.cfg.readOnly && !t.readOnly) continue;  // --read-only: the writing tools are not offered
    Json o = jobj();
    o.set("name", jstr(t.name));
    if (atLeast(era.version, "2025-06-18")) o.set("title", jstr(t.title));
    o.set("description", jstr(t.description));
    o.set("inputSchema", t.inputSchema);
    Json ann = jobj();
    ann.set("title", jstr(t.title));
    ann.set("readOnlyHint", jbool(t.readOnly));
    if (!t.readOnly) ann.set("destructiveHint", jbool(t.destructive));
    ann.set("openWorldHint", jbool(false));
    o.set("annotations", ann);
    arr.push(o);
  }
  (void)params;
  Json r = jobj();
  r.set("tools", arr);
  if (era.modern) { r.set("ttlMs", jnum(3600000)); r.set("cacheScope", jstr("public")); }
  finishResult(r, era);
  return r;
}

Json Server::callTool(const Json& params, const Era& era) {
  const std::string name = params["name"].asString();
  const Tool* tool = tools_.find(name);
  const Json args = params["arguments"].isObject() ? params["arguments"] : jobj();
  ToolResult res;
  if (!tool) {
    res = toolFail("unknown tool \"" + name + "\"", "tools/list shows the tools");
  } else if (ctx_.cfg.readOnly && !tool->readOnly) {
    res = toolFail("the tool " + name + " is disabled: ote-mcp was started with --read-only", "restart without --read-only to allow writing");
  } else {
    const std::string bad = checkAgainstSchema(tool->inputSchema, args);
    if (!bad.empty()) {
      res = toolFail("invalid arguments: " + bad, "tools/list shows each tool's input schema");
    } else {
      try {
        res = tool->run(args);
      } catch (const std::exception& e) {
        logError("tool " + name + " failed: " + e.what());
        res = toolFail(std::string("internal error in ") + name + ": " + e.what());
      } catch (...) {
        logError("tool " + name + " failed (unknown exception)");
        res = toolFail("internal error in " + name);
      }
    }
  }
  std::string text = res.data.dump(0);
  if (text.size() > kMaxResultChars) {
    res = toolFail("the result is too large (" + std::to_string(text.size()) + " bytes); ask for less", "use limit / offset, mode=summary, or a narrower path");
    text = res.data.dump(0);
  }
  Json content = jarr();
  Json block = jobj();
  block.set("type", jstr("text"));
  block.set("text", jstr(text));
  content.push(block);
  for (const Json& extra : res.extraContent) content.push(extra);
  Json r = jobj();
  r.set("content", content);
  if (res.data.isObject() && (era.modern || atLeast(era.version, "2025-06-18"))) r.set("structuredContent", res.data);
  r.set("isError", jbool(res.isError));
  finishResult(r, era);
  return r;
}

Json Server::dispatch(const std::string& method, const Json& params, const Json& id, const Era& era, bool& isError) {
  isError = false;
  auto ok = [&](Json result) { finishResult(result, era); return resultObject(id, result); };
  if (method == "initialize") return resultObject(id, initialize(params));
  if (method == "ping") return ok(jobj());
  if (method == "server/discover") {
    Era modernEra;  // discover is a revision 2026-07-28 call whatever _meta said
    modernEra.modern = true;
    modernEra.version = kModern;
    return resultObject(id, discover(modernEra));
  }
  if (method == "tools/list") {
    const Json& cur = params["cursor"];
    if (!cur.isNull() && !cur.isString()) return errorObject(id, kInvalidParams, "Invalid params: cursor must be a string");
    return resultObject(id, listTools(params, era));
  }
  if (method == "tools/call") {
    if (!params["name"].isString()) return errorObject(id, kInvalidParams, "Invalid params: tools/call needs a string \"name\"");
    if (params.has("arguments") && !params["arguments"].isObject() && !params["arguments"].isNull()) return errorObject(id, kInvalidParams, "Invalid params: \"arguments\" must be an object");
    if (!tools_.find(params["name"].asString())) return errorObject(id, kInvalidParams, "Unknown tool: " + params["name"].asString() + " (tools/list shows the tools)");
    return resultObject(id, callTool(params, era));
  }
  if (method == "resources/list") {
    const Json all = listAllResources(ctx_);
    size_t start = 0;
    if (params["cursor"].isString() && !params["cursor"].asString().empty()) {
      char* end = nullptr;
      start = (size_t)std::strtoull(params["cursor"].asString().c_str(), &end, 10);
      if (!end || *end != 0 || start > all.size()) return errorObject(id, kInvalidParams, "Invalid params: unknown cursor");
    }
    const size_t pageSize = 100;
    Json page = jarr();
    for (size_t i = start; i < all.size() && i < start + pageSize; ++i) page.push(all.items()[i]);
    Json r = jobj();
    r.set("resources", page);
    if (start + pageSize < all.size()) r.set("nextCursor", jstr(std::to_string(start + pageSize)));
    return ok(r);
  }
  if (method == "resources/templates/list") {
    Json r = jobj();
    r.set("resourceTemplates", resourceTemplates());
    return ok(r);
  }
  if (method == "resources/read") {
    if (!params["uri"].isString()) return errorObject(id, kInvalidParams, "Invalid params: resources/read needs a string \"uri\"");
    Json contents;
    std::string message;
    if (!readResourceUri(ctx_, params["uri"].asString(), contents, message)) return errorObject(id, kInvalidParams, message);
    return ok(contents);
  }
  if (method == "prompts/list") {
    Json r = jobj();
    r.set("prompts", listPromptDefs());
    return ok(r);
  }
  if (method == "prompts/get") {
    if (!params["name"].isString()) return errorObject(id, kInvalidParams, "Invalid params: prompts/get needs a string \"name\"");
    Json result;
    std::string message;
    if (!getPromptByName(ctx_, params["name"].asString(), params["arguments"], result, message)) return errorObject(id, kInvalidParams, message);
    return ok(result);
  }
  if (method == "logging/setLevel") return ok(jobj());  // messages go to stderr whatever the level
  return errorObject(id, kMethodNotFound, "Method not found: " + method);
}

}  // namespace ote_mcp
