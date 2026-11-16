#include "server.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <functional>
#include <mutex>

#include "httplib.h"
#include "art.h"
#include "audio.h"
#include "models.h"
#include "core/data/database.h"
#include "learn.h"
#include "log.h"
#include "projects.h"
#include "static_files.h"

namespace ote_editor {

using ote::Json;

namespace {

constexpr size_t kMaxBody = 16u * 1024 * 1024;

std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
  return s;
}

// Host header -> bare host name ("localhost:8765" -> "localhost", "[::1]:80" -> "[::1]").
std::string hostName(const std::string& header) {
  std::string h = lower(header);
  if (!h.empty() && h[0] == '[') {
    size_t close = h.find(']');
    return close == std::string::npos ? h : h.substr(0, close + 1);
  }
  size_t colon = h.rfind(':');
  return colon == std::string::npos ? h : h.substr(0, colon);
}

bool isApiPath(const std::string& path) { return path.compare(0, 5, "/api/") == 0 || path == "/api"; }

thread_local std::chrono::steady_clock::time_point t_start;

}  // namespace

bool hostAllowed(const Config& cfg, const std::string& hostHeader) {
  std::string name = hostName(hostHeader);
  if (name.empty()) return false;
  if (name == "localhost" || name == "127.0.0.1" || name == "[::1]") return true;
  if (name == lower(cfg.host) || name == "[" + lower(cfg.host) + "]") return true;
  for (const std::string& a : cfg.allowHosts)
    if (name == lower(a)) return true;
  return false;
}

struct EditorServer::Impl {
  Config cfg;
  httplib::Server svr;
  // The engine core is not thread-safe as a whole (global log sink etc.), so
  // all /api work is serialized. Static files stay concurrent.
  std::mutex apiMutex;

  explicit Impl(const Config& c) : cfg(c) {}

  static void sendJson(httplib::Response& res, int status, const Json& body) {
    res.status = status;
    res.set_content(body.dump(0), "application/json");
  }
  static void sendError(httplib::Response& res, int status, const std::string& msg) {
    sendJson(res, status, apiError(status, msg).body);
  }

  static bool parseBody(const httplib::Request& req, Json& out, ApiResult& err) {
    std::string perr;
    if (req.body.empty()) { err = apiError(400, "request body is empty"); return false; }
    if (!Json::parse(req.body, out, perr)) { err = apiError(400, "invalid JSON body: " + perr); return false; }
    return true;
  }

  using ApiFn = std::function<ApiResult(const httplib::Request&)>;
  using Handler = std::function<void(const httplib::Request&, httplib::Response&)>;

  // Wraps a handler: catch-all so nothing can take the process down.
  Handler guarded(const Handler& fn, bool serialize) {
    return [this, fn, serialize](const httplib::Request& req, httplib::Response& res) {
      try {
        if (serialize) {
          std::lock_guard<std::mutex> lock(apiMutex);
          fn(req, res);
        } else {
          fn(req, res);
        }
      } catch (const std::exception& e) {
        elog("%s %s: unhandled exception: %s", req.method.c_str(), req.path.c_str(), e.what());
        sendError(res, 500, std::string("internal error: ") + e.what());
      } catch (...) {
        elog("%s %s: unhandled unknown exception", req.method.c_str(), req.path.c_str());
        sendError(res, 500, "internal error");
      }
    };
  }

  Handler api(const ApiFn& fn) {
    return guarded([fn](const httplib::Request& req, httplib::Response& res) {
      ApiResult r = fn(req);
      sendJson(res, r.status, r.body);
    }, true);
  }

  Handler apiWithBody(std::function<ApiResult(const httplib::Request&, const Json&)> fn) {
    return api([fn](const httplib::Request& req) {
      Json body;
      ApiResult err;
      if (!parseBody(req, body, err)) return err;
      return fn(req, body);
    });
  }

  static void sendStatic(httplib::Response& res, const StaticResponse& r) {
    res.status = r.status;
    res.set_content(r.body, r.mime);
  }

  void setup() {
    svr.set_payload_max_length(kMaxBody);
#ifndef _WIN32
    // httplib's default also sets SO_REUSEPORT, which would let a second helper
    // silently share the port. We want "port busy" to be a hard error.
    svr.set_socket_options([](socket_t sock) {
      int yes = 1;
      setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof yes);
    });
#endif
    svr.set_default_headers({{"Cache-Control", "no-store"}});

    // DNS-rebinding defence: a hostile web page can point its own domain at
    // 127.0.0.1, but the browser then sends that domain in Host.
    svr.set_pre_routing_handler([this](const httplib::Request& req, httplib::Response& res) {
      t_start = std::chrono::steady_clock::now();
      if (!hostAllowed(cfg, req.get_header_value("Host"))) {
        elog("rejected request with Host \"%s\" (use --allow-host to permit it)",
             req.get_header_value("Host").c_str());
        sendError(res, 403, "forbidden: unexpected Host header");
        return httplib::Server::HandlerResponse::Handled;
      }
      return httplib::Server::HandlerResponse::Unhandled;
    });

    svr.set_exception_handler([](const httplib::Request& req, httplib::Response& res, std::exception_ptr ep) {
      std::string what = "unknown error";
      try { if (ep) std::rethrow_exception(ep); } catch (const std::exception& e) { what = e.what(); } catch (...) {}
      elog("%s %s: exception in server: %s", req.method.c_str(), req.path.c_str(), what.c_str());
      sendError(res, 500, "internal error: " + what);
    });

    // Plain httplib errors (404 for unknown routes, 413 oversized body, ...)
    // get the same JSON shape under /api.
    svr.set_error_handler([](const httplib::Request& req, httplib::Response& res) {
      if (isApiPath(req.path) && res.body.empty()) {
        std::string msg = res.status == 413 ? "request body is larger than 16 MB"
                          : res.status == 404 ? "no such API endpoint"
                          : res.status == 405 ? "method not allowed"
                                              : "request failed";
        sendError(res, res.status, msg);
      }
      return httplib::Server::HandlerResponse::Handled;
    });

    svr.set_logger([](const httplib::Request& req, const httplib::Response& res) {
      if (!isApiPath(req.path) && res.status < 400) return;  // skip static-file noise
      long ms = (long)std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - t_start).count();
      elog("%s %s %d %ldms", req.method.c_str(), req.path.c_str(), res.status, ms);
    });

    svr.Get("/api/info", api([](const httplib::Request&) {
      Json o = Json::makeObject();
      o.set("engine", Json::makeString("OpenTacticsEngine"));
      o.set("apiVersion", Json::makeNumber(1));
      o.set("formatVersion", Json::makeNumber(1));
      // Which kinds of view ("view" in game.json styles) a hexagon game may use: what this build's loader accepts.
      Json hexViews = Json::makeArray();
      using ote::StyleView;
      const std::pair<StyleView, const char*> kinds[] = {{StyleView::TopDown, "top-down"}, {StyleView::Isometric, "isometric"},
                                                         {StyleView::Isometric3D, "isometric-3d"}, {StyleView::Full3D, "3d"}};
      for (const auto& k : kinds)
        if (ote::hexGameAllowsView(k.first)) hexViews.push(Json::makeString(k.second));
      o.set("hexViews", hexViews);
      return apiOk(o);
    }));
    svr.Get("/api/learn", api([this](const httplib::Request&) { return getLearn(cfg); }));
    svr.Put("/api/learn", api([this](const httplib::Request& req) { return putLearn(cfg, req.body); }));
    svr.Get("/api/projects", api([this](const httplib::Request&) { return listProjects(cfg); }));
    svr.Post("/api/projects", apiWithBody([this](const httplib::Request&, const Json& body) {
      return createProject(cfg, body);
    }));
    svr.Get("/api/templates", api([this](const httplib::Request&) { return listTemplates(cfg); }));
    svr.Get("/api/fonts", api([this](const httplib::Request&) { return listFonts(cfg); }));
    svr.Get("/api/projects/:id", api([this](const httplib::Request& req) {
      return getProject(cfg, req.path_params.at("id"));
    }));
    svr.Put("/api/projects/:id", apiWithBody([this](const httplib::Request& req, const Json& body) {
      return putProject(cfg, req.path_params.at("id"), body);
    }));
    svr.Get("/api/projects/:id/problems", api([this](const httplib::Request& req) {
      return getProblems(cfg, req.path_params.at("id"));
    }));
    svr.Get("/api/projects/:id/files", api([this](const httplib::Request& req) {
      return getProjectFiles(cfg, req.path_params.at("id"));
    }));
    svr.Get(R"(/api/projects/([^/]+)/file/(.+))", guarded([this](const httplib::Request& req, httplib::Response& res) {
      RawFile f = getProjectFile(cfg, req.matches[1].str(), req.matches[2].str());
      if (f.status != 200) sendError(res, f.status, f.error);
      else {
        std::string rel = lower(req.matches[2].str());
        bool png = rel.size() > 4 && rel.compare(rel.size() - 4, 4, ".png") == 0;
        std::string audio = audioContentType(rel);  // audio/wav, audio/ogg: the Sound screen's preview plays these
        res.status = 200;
        res.set_content(f.bytes, png ? "image/png" : !audio.empty() ? audio : "application/octet-stream");
      }
    }, true));
    svr.Get("/api/projects/:id/art", api([this](const httplib::Request& req) {
      return listArt(cfg, req.path_params.at("id"));
    }));
    svr.Put(R"(/api/projects/([^/]+)/art/(.+))", api([this](const httplib::Request& req) {
      return putArt(cfg, req.matches[1].str(), req.matches[2].str(), req.body,
                    req.get_param_value("overwrite") == "1");
    }));
    svr.Delete(R"(/api/projects/([^/]+)/art/(.+))", api([this](const httplib::Request& req) {
      return deleteArt(cfg, req.matches[1].str(), req.matches[2].str());
    }));
    svr.Get("/api/projects/:id/audio", api([this](const httplib::Request& req) {
      return listAudioFiles(cfg, req.path_params.at("id"));
    }));
    svr.Put(R"(/api/projects/([^/]+)/audio/(.+))", api([this](const httplib::Request& req) {
      return putAudioFile(cfg, req.matches[1].str(), req.matches[2].str(), req.body,
                          req.get_param_value("overwrite") == "1");
    }));
    svr.Delete(R"(/api/projects/([^/]+)/audio/(.+))", api([this](const httplib::Request& req) {
      return deleteAudioFile(cfg, req.matches[1].str(), req.matches[2].str());
    }));
    svr.Get("/api/projects/:id/models", api([this](const httplib::Request& req) {
      return listModels(cfg, req.path_params.at("id"));
    }));
    svr.Put(R"(/api/projects/([^/]+)/models/(.+))", api([this](const httplib::Request& req) {
      return putModel(cfg, req.matches[1].str(), req.matches[2].str(), req.body,
                      req.get_param_value("overwrite") == "1");
    }));
    svr.Delete(R"(/api/projects/([^/]+)/models/(.+))", api([this](const httplib::Request& req) {
      return deleteModel(cfg, req.matches[1].str(), req.matches[2].str());
    }));
    // Any other /api path: JSON 404 (must be registered before the web catch-all).
    svr.Get(R"(/api(/.*)?)", guarded([](const httplib::Request&, httplib::Response& res) {
      sendError(res, 404, "no such API endpoint");
    }, false));

    svr.Get(R"(/assets/(.*))", guarded([this](const httplib::Request& req, httplib::Response& res) {
      sendStatic(res, serveFromDir(cfg.assetsDir, req.matches[1].str(), false));
    }, false));
    svr.Get(R"(/player/(.*))", guarded([this](const httplib::Request& req, httplib::Response& res) {
      sendStatic(res, serveFromDir(cfg.playerDir, req.matches[1].str(), false));
    }, false));
    svr.Get(R"(/(.*))", guarded([this](const httplib::Request& req, httplib::Response& res) {
      std::string rel = req.matches[1].str();
      StaticResponse r = serveFromDir(cfg.webDir, rel, true);
      // UI not built yet: a friendly page instead of a 404 at the root.
      if (r.status == 404 && (rel.empty() || rel == "index.html")) r = builtinIndexPage(cfg);
      sendStatic(res, r);
    }, false));
  }
};

EditorServer::EditorServer(const Config& cfg) : impl_(new Impl(cfg)) { impl_->setup(); }
EditorServer::~EditorServer() = default;

bool EditorServer::bind(std::string& err) {
  if (!impl_->svr.bind_to_port(impl_->cfg.host, impl_->cfg.port)) {
    err = "cannot listen on " + impl_->cfg.host + ":" + std::to_string(impl_->cfg.port) +
          " - the port is probably in use (is another ote-editor running?). Pick another with --port.";
    return false;
  }
  return true;
}

void EditorServer::serve() { impl_->svr.listen_after_bind(); }
void EditorServer::stop() { impl_->svr.stop(); }

}  // namespace ote_editor
