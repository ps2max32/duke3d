/* ps2_float.h: included before Polymost's and the engine's sources: the R5900's
   FPU has no doubles (each is a call into soft float), and they need no more
   than float: double is float in them, the math.h names the float versions.
   Everything that shares such a variable (dxb1, gxyaspect, ...) has this too. */
#ifndef PS2_FLOAT_H
#define PS2_FLOAT_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include "ps2_math.h"
/* before the define: GL's GLdouble and GLclampd stay double, or the calls that
   take them (glDepthRange) would pass floats to libgl's doubles */
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
#undef isinf
#define isinf(x) __builtin_isinf(x)
#define double float
#endif
