\
#pragma once
/*
  Minimal GLU-like header: gluPerspective only.
*/
#include "gl.h"

#ifdef __cplusplus
extern "C" {
#endif

void gluPerspective(GLfloat fovyDeg, GLfloat aspect, GLfloat zNear, GLfloat zFar);

#ifdef __cplusplus
}
#endif
