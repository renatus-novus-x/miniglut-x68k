\
#pragma once
/*
  Minimal OpenGL-like header (fixed pipeline-ish) for wireframe-only rendering.

  IMPORTANT:
  - This is NOT real OpenGL.
  - It is a small immediate-mode API designed to compile a tiny subset of samples.
*/
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Basic types */
typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef float GLfloat;
typedef unsigned char GLubyte;

/* Constants */
#define GL_COLOR_BUFFER_BIT 0x00004000

#define GL_MODELVIEW  0x1700
#define GL_PROJECTION 0x1701

#define GL_LINES      0x0001
#define GL_LINE_STRIP 0x0003
#define GL_LINE_LOOP  0x0002
#define GL_TRIANGLES  0x0004

/* API subset */
void glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void glClear(GLbitfield mask);

void glMatrixMode(GLenum mode);
void glLoadIdentity(void);
void glPushMatrix(void);
void glPopMatrix(void);

void glTranslatef(GLfloat x, GLfloat y, GLfloat z);
void glRotatef(GLfloat angleDeg, GLfloat x, GLfloat y, GLfloat z);
void glScalef(GLfloat x, GLfloat y, GLfloat z);
void glOrtho(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f);

void glColor3ub(GLubyte r, GLubyte g, GLubyte b);

void glBegin(GLenum mode);
void glVertex3f(GLfloat x, GLfloat y, GLfloat z);
void glEnd(void);

void glFlush(void);

/* Non-GL functions (tiny bridge for platform init) */
int  miniglGetWidth(void);
int  miniglGetHeight(void);

#ifdef __cplusplus
}
#endif
