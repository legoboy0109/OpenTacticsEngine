// The MCP protocol over newline-delimited JSON-RPC 2.0: one message per line in, one per line out. Dual-era: the handshake
// revisions (2024-11-05, 2025-03-26, 2025-06-18, 2025-11-25: `initialize` then requests) and the stateless revision
// 2026-07-28 (every request carries its version in params._meta; `server/discover`). docs/MCP.md "Protocol revisions".
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "context.h"
#include "tool.h"

namespace ote_mcp {

class Server {
 public:
  Server(Context& ctx, ToolRegistry& tools) : ctx_(ctx), tools_(tools) {}

  // Handles one line. Returns the line to write back ("" when there is nothing to answer: a notification).
  // Never throws, never writes anywhere itself.
  std::string handleLine(const std::string& line);

  // An error response for input that never became a request (an over-long line).
  static std::string errorLine(int code, const std::string& message);

  static constexpr const char* kServerName = "ote-mcp";
  static constexpr const char* kServerVersion = "1.0.0";

 private:
  struct Era {
    bool modern = false;
    std::string version;  // the revision this request is served under
  };
  Json handleOne(const Json& msg, bool& hasResponse);
  Json dispatch(const std::string& method, const Json& params, const Json& id, const Era& era, bool& isError);
  Json initialize(const Json& params);
  Json discover(const Era& era);
  Json callTool(const Json& params, const Era& era);
  Json listTools(const Json& params, const Era& era);
  Json capabilities(bool modern) const;
  std::string instructions() const;
  void finishResult(Json& result, const Era& era) const;

  Context& ctx_;
  ToolRegistry& tools_;
  std::string legacyVersion_ = "2025-11-25";  // what `initialize` negotiated (until then: the newest handshake revision)
};

}  // namespace ote_mcp
