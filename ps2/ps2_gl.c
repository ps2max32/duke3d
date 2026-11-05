/* ps2_gl.c: JFBuild's GL function table (glbuild.c's) on libgl: the fixed
   function calls polymost.c uses, by name from mxglGetProcAddress. No shaders:
   polymost.c draws with libgl's own vertex path under __PS2__. */

#include "build.h"
#include "baselayer_priv.h"
#include "glbuild_priv.h"
#include <mxgl.h>

struct glbuild_funcs glfunc;

static void ignore(void) {}

#define F(n) glfunc.n = (void *)(mxglGetProcAddress(#n) ? mxglGetProcAddress(#n) : ignore)

int glbuild_init(void) {
  static int done;
  if (done)
    return 0;
  done = 1;
  F(glClearColor); F(glClear); F(glColorMask); F(glBlendFunc); F(glCullFace); F(glFrontFace);
  F(glPolygonOffset); F(glPolygonMode); F(glEnable); F(glDisable); F(glGetFloatv);
  F(glGetIntegerv); F(glGetString); F(glGetError); F(glHint); F(glPixelStorei); F(glViewport);
  F(glScissor); F(glDepthFunc); F(glDepthMask); F(glDepthRange); F(glReadPixels);
  F(glGenTextures); F(glDeleteTextures); F(glBindTexture); F(glTexImage2D); F(glTexSubImage2D);
  F(glTexParameterf); F(glTexParameteri); F(glCompressedTexImage2D); F(glDrawElements);
  memset(&glinfo, 0, sizeof glinfo);
  glinfo.loaded = 1;
  glinfo.maxtexsize = 256;
  glinfo.clamptoedge = 1;
  glinfo.bgra = 0;
  glinfo.texnpot = 0;
  glinfo.multitex = 0;
  return 0;
}
int glbuild_loadfunctions(void) { return 0; }
void glbuild_unloadfunctions(void) {}
void glbuild_check_errors(const char *file, int line) {}
GLuint glbuild_compile_shader(GLuint type, const GLchar *source) { return 0; }
GLuint glbuild_link_program(int n, GLuint *shaders) { return 0; }

/* polymost.c's shader sources, never compiled here */
const char default_polymost_fs_glsl[] = "", default_polymost_vs_glsl[] = "";
const char default_polymostaux_fs_glsl[] = "", default_polymostaux_vs_glsl[] = "";
/* texture compression is off (glusetexcompr 0) */
int ptcompress_getstorage(int w, int h, int format) { return 0; }
int ptcompress_compress(void *b, int w, int h, int f, void *o) { return -1; }
