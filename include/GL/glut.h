\
#pragma once
/*
  Minimal GLUT-compatible header for elf2x68k.

  This is intentionally tiny: it provides only a subset of GLUT APIs.
  It is designed so that simple GLUT samples can be adapted with minimal edits.
*/
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Display mode flags (accepted by glutInitDisplayMode, currently ignored). */
#define GLUT_RGB     0
#define GLUT_RGBA    0
#define GLUT_SINGLE  0
#define GLUT_DOUBLE  2

/* Callback typedefs (matching common GLUT signatures). */
typedef void (*GLUTdisplayCB)(void);
typedef void (*GLUTidleCB)(void);
typedef void (*GLUTkeyboardCB)(unsigned char key, int x, int y);

/* Core subset. */
void glutInit(int *argc, char **argv);
int  glutCreateWindow(const char *title);
void glutDisplayFunc(GLUTdisplayCB cb);
void glutKeyboardFunc(GLUTkeyboardCB cb);
void glutIdleFunc(GLUTidleCB cb);
void glutMainLoop(void);

/* Extra helpers for compatibility. */
void glutInitDisplayMode(unsigned int mode);
void glutInitWindowSize(int width, int height);
void glutSwapBuffers(void);
void glutPostRedisplay(void);
void glutLeaveMainLoop(void);

#ifdef __cplusplus
}
#endif