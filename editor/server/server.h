// The HTTP server: routing, Host check, JSON helpers. Hides httplib.
#pragma once
#include <memory>
#include <string>

#include "config.h"

namespace ote_editor {

class EditorServer {
 public:
  explicit EditorServer(const Config& cfg);
  ~EditorServer();

  // Binds the listening socket; false + message when the port is busy etc.
  bool bind(std::string& err);
  // Serves until stop() is called (blocking).
  void serve();
  // Safe to call from any thread.
  void stop();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// True when a Host header value (with optional :port) is allowed.
bool hostAllowed(const Config& cfg, const std::string& hostHeader);

}  // namespace ote_editor
