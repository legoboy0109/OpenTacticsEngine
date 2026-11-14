// The handful of OpenGL calls the hardware 3D backend uses, as a table of
// function pointers the platform fills in. The engine ships no GL headers and
// links no GL library: the same code runs on OpenGL ES 3.0, WebGL 2 and
// desktop OpenGL 3.3 core, whichever the platform's GlHost provides.
//
// Only names that exist, with the same meaning, in ALL three are listed (so
// no glClearDepth/glClearDepthf, no glPolygonMode, ...).
#pragma once
#include <cstddef>

namespace ote {

// Where the platform gives the backend a GL context (docs/RENDER3D.md
// "OpenGL backend"). The context is the backend's own:
// nothing else draws with it, so the backend never has to restore GL state.
class GlHost {
 public:
  virtual ~GlHost() {}
  // Makes the context current, remembering what was current before. False
  // when the context is gone (the platform shut down): draw nothing then.
  virtual bool enter() = 0;
  // Puts back what was current before enter(), so the 2D renderer never
  // finds a foreign context.
  virtual void leave() = 0;
  // Address of a GL function by name ("glClear"), null when missing.
  virtual void* proc(const char* name) = 0;
  // True: OpenGL ES 3 / WebGL 2 shaders (#version 300 es). False: desktop
  // OpenGL 3.3 core (#version 330 core).
  virtual bool gles() const = 0;
};

#if defined(_WIN32) && !defined(_WIN64)
#define OTE_GL_CALL __stdcall
#else
#define OTE_GL_CALL
#endif

typedef unsigned int GLenum_;
typedef unsigned int GLuint_;
typedef int GLint_;
typedef int GLsizei_;
typedef unsigned char GLboolean_;
typedef unsigned int GLbitfield_;
typedef float GLfloat_;
typedef char GLchar_;
typedef ptrdiff_t GLsizeiptr_;

enum : unsigned int {
  GL_DEPTH_BUFFER_BIT_ = 0x0100, GL_COLOR_BUFFER_BIT_ = 0x4000,
  GL_TRIANGLES_ = 0x0004,
  GL_LESS_ = 0x0201, GL_ALWAYS_ = 0x0207,
  GL_SRC_ALPHA_ = 0x0302, GL_ONE_MINUS_SRC_ALPHA_ = 0x0303, GL_ONE_ = 1,
  GL_FRONT_ = 0x0404, GL_BACK_ = 0x0405, GL_CW_ = 0x0900,
  GL_CULL_FACE_ = 0x0B44, GL_DEPTH_TEST_ = 0x0B71, GL_DITHER_ = 0x0BD0, GL_BLEND_ = 0x0BE2,
  GL_UNPACK_ALIGNMENT_ = 0x0CF5, GL_PACK_ALIGNMENT_ = 0x0D05,
  GL_TEXTURE_2D_ = 0x0DE1,
  GL_UNSIGNED_BYTE_ = 0x1401, GL_UNSIGNED_SHORT_ = 0x1403, GL_FLOAT_ = 0x1406,
  GL_RGBA_ = 0x1908, GL_RGBA8_ = 0x8058,
  GL_NEAREST_ = 0x2600, GL_LINEAR_ = 0x2601,
  GL_TEXTURE_MAG_FILTER_ = 0x2800, GL_TEXTURE_MIN_FILTER_ = 0x2801,
  GL_TEXTURE_WRAP_S_ = 0x2802, GL_TEXTURE_WRAP_T_ = 0x2803,
  GL_REPEAT_ = 0x2901, GL_CLAMP_TO_EDGE_ = 0x812F,
  GL_TEXTURE0_ = 0x84C0,
  GL_ARRAY_BUFFER_ = 0x8892, GL_ELEMENT_ARRAY_BUFFER_ = 0x8893,
  GL_STATIC_DRAW_ = 0x88E4, GL_DYNAMIC_DRAW_ = 0x88E8,
  GL_FRAGMENT_SHADER_ = 0x8B30, GL_VERTEX_SHADER_ = 0x8B31,
  GL_COMPILE_STATUS_ = 0x8B81, GL_LINK_STATUS_ = 0x8B82,
  GL_FRAMEBUFFER_ = 0x8D40, GL_RENDERBUFFER_ = 0x8D41,
  GL_COLOR_ATTACHMENT0_ = 0x8CE0, GL_DEPTH_ATTACHMENT_ = 0x8D00,
  GL_FRAMEBUFFER_COMPLETE_ = 0x8CD5, GL_DEPTH_COMPONENT24_ = 0x81A6,
  GL_VERSION_ = 0x1F02, GL_RENDERER_ = 0x1F01,
};

// X(return type, name, parameters)
#define OTE_GL_FUNCTIONS(X) \
  X(GLenum_, GetError, (void)) \
  X(const unsigned char*, GetString, (GLenum_)) \
  X(void, Viewport, (GLint_, GLint_, GLsizei_, GLsizei_)) \
  X(void, ClearColor, (GLfloat_, GLfloat_, GLfloat_, GLfloat_)) \
  X(void, Clear, (GLbitfield_)) \
  X(void, Enable, (GLenum_)) \
  X(void, Disable, (GLenum_)) \
  X(void, DepthFunc, (GLenum_)) \
  X(void, DepthMask, (GLboolean_)) \
  X(void, CullFace, (GLenum_)) \
  X(void, FrontFace, (GLenum_)) \
  X(void, BlendFuncSeparate, (GLenum_, GLenum_, GLenum_, GLenum_)) \
  X(void, PixelStorei, (GLenum_, GLint_)) \
  X(void, GenBuffers, (GLsizei_, GLuint_*)) \
  X(void, BindBuffer, (GLenum_, GLuint_)) \
  X(void, BufferData, (GLenum_, GLsizeiptr_, const void*, GLenum_)) \
  X(void, DeleteBuffers, (GLsizei_, const GLuint_*)) \
  X(void, GenVertexArrays, (GLsizei_, GLuint_*)) \
  X(void, BindVertexArray, (GLuint_)) \
  X(void, DeleteVertexArrays, (GLsizei_, const GLuint_*)) \
  X(void, EnableVertexAttribArray, (GLuint_)) \
  X(void, VertexAttribPointer, (GLuint_, GLint_, GLenum_, GLboolean_, GLsizei_, const void*)) \
  X(void, GenTextures, (GLsizei_, GLuint_*)) \
  X(void, BindTexture, (GLenum_, GLuint_)) \
  X(void, TexImage2D, (GLenum_, GLint_, GLint_, GLsizei_, GLsizei_, GLint_, GLenum_, GLenum_, const void*)) \
  X(void, TexParameteri, (GLenum_, GLenum_, GLint_)) \
  X(void, DeleteTextures, (GLsizei_, const GLuint_*)) \
  X(void, ActiveTexture, (GLenum_)) \
  X(void, GenSamplers, (GLsizei_, GLuint_*)) \
  X(void, BindSampler, (GLuint_, GLuint_)) \
  X(void, SamplerParameteri, (GLuint_, GLenum_, GLint_)) \
  X(void, DeleteSamplers, (GLsizei_, const GLuint_*)) \
  X(void, GenFramebuffers, (GLsizei_, GLuint_*)) \
  X(void, BindFramebuffer, (GLenum_, GLuint_)) \
  X(void, FramebufferTexture2D, (GLenum_, GLenum_, GLenum_, GLuint_, GLint_)) \
  X(void, FramebufferRenderbuffer, (GLenum_, GLenum_, GLenum_, GLuint_)) \
  X(GLenum_, CheckFramebufferStatus, (GLenum_)) \
  X(void, DeleteFramebuffers, (GLsizei_, const GLuint_*)) \
  X(void, GenRenderbuffers, (GLsizei_, GLuint_*)) \
  X(void, BindRenderbuffer, (GLenum_, GLuint_)) \
  X(void, RenderbufferStorage, (GLenum_, GLenum_, GLsizei_, GLsizei_)) \
  X(void, DeleteRenderbuffers, (GLsizei_, const GLuint_*)) \
  X(GLuint_, CreateShader, (GLenum_)) \
  X(void, ShaderSource, (GLuint_, GLsizei_, const GLchar_* const*, const GLint_*)) \
  X(void, CompileShader, (GLuint_)) \
  X(void, GetShaderiv, (GLuint_, GLenum_, GLint_*)) \
  X(void, GetShaderInfoLog, (GLuint_, GLsizei_, GLsizei_*, GLchar_*)) \
  X(void, DeleteShader, (GLuint_)) \
  X(GLuint_, CreateProgram, (void)) \
  X(void, AttachShader, (GLuint_, GLuint_)) \
  X(void, BindAttribLocation, (GLuint_, GLuint_, const GLchar_*)) \
  X(void, LinkProgram, (GLuint_)) \
  X(void, GetProgramiv, (GLuint_, GLenum_, GLint_*)) \
  X(void, GetProgramInfoLog, (GLuint_, GLsizei_, GLsizei_*, GLchar_*)) \
  X(void, UseProgram, (GLuint_)) \
  X(void, DeleteProgram, (GLuint_)) \
  X(GLint_, GetUniformLocation, (GLuint_, const GLchar_*)) \
  X(void, UniformMatrix4fv, (GLint_, GLsizei_, GLboolean_, const GLfloat_*)) \
  X(void, UniformMatrix3fv, (GLint_, GLsizei_, GLboolean_, const GLfloat_*)) \
  X(void, Uniform4f, (GLint_, GLfloat_, GLfloat_, GLfloat_, GLfloat_)) \
  X(void, Uniform3f, (GLint_, GLfloat_, GLfloat_, GLfloat_)) \
  X(void, Uniform1i, (GLint_, GLint_)) \
  X(void, DrawElements, (GLenum_, GLsizei_, GLenum_, const void*)) \
  X(void, ReadPixels, (GLint_, GLint_, GLsizei_, GLsizei_, GLenum_, GLenum_, void*))

struct GlApi {
#define OTE_GL_FIELD(ret, name, params) ret(OTE_GL_CALL* name) params = nullptr;
  OTE_GL_FUNCTIONS(OTE_GL_FIELD)
#undef OTE_GL_FIELD
};

// Fills the table from the host (its context must be current). False, with
// the first missing name in `missing`, when a function is not there.
bool loadGlApi(GlApi& gl, GlHost& host, const char*& missing);

}  // namespace ote
