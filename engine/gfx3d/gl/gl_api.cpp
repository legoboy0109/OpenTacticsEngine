#include "gfx3d/gl/gl_api.h"

namespace ote {

bool loadGlApi(GlApi& gl, GlHost& host, const char*& missing) {
  missing = nullptr;
#define OTE_GL_LOAD(ret, name, params)                          \
  gl.name = (ret(OTE_GL_CALL*) params)host.proc("gl" #name);   \
  if (!gl.name && !missing) missing = "gl" #name;
  OTE_GL_FUNCTIONS(OTE_GL_LOAD)
#undef OTE_GL_LOAD
  return missing == nullptr;
}

}  // namespace ote
