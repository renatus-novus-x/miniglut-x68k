\
#include <stdlib.h>
#include <stdint.h>

#ifdef __human68k__
  #include <x68k/iocs.h>
  #include <x68k/dos.h>
#else
  #include "host_shim.h"
#endif

#include "GL/glut.h"

/* Forward: the renderer uses fixed 512x512 for now. */
extern void miniglPlatformInit(void);
extern void miniglPlatformShutdown(void);

/* Callback storage */
static GLUTdisplayCB g_display_cb = NULL;
static GLUTkeyboardCB g_keyboard_cb = NULL;
static GLUTidleCB g_idle_cb = NULL;

static volatile int g_running = 1;
static volatile int g_redisplay = 1;

void glutInit(int *argc, char **argv)
{
  (void)argc;
  (void)argv;
}

int glutCreateWindow(const char *title)
{
  (void)title;
  miniglPlatformInit();
  return 1;
}

void glutDisplayFunc(GLUTdisplayCB cb)
{
  g_display_cb = cb;
  g_redisplay = 1;
}

void glutKeyboardFunc(GLUTkeyboardCB cb)
{
  g_keyboard_cb = cb;
}

void glutIdleFunc(GLUTidleCB cb)
{
  g_idle_cb = cb;
}

void glutInitDisplayMode(unsigned int mode)
{
  (void)mode;
}

void glutSwapBuffers(void)
{
  /* No real double buffering in this minimal prototype. */
}

void glutPostRedisplay(void)
{
  g_redisplay = 1;
}

void glutLeaveMainLoop(void)
{
  g_running = 0;
}

/* Poll keyboard using IOCS B_KEYSNS + B_KEYINP.
   - B_KEYINP blocks if no key is available, so always gate with B_KEYSNS. */
static void miniglutPollKeyboard(void)
{
#ifdef __human68k__
  if (_iocs_b_keysns() != 0) {
    int v = _iocs_b_keyinp();
    unsigned char key = (unsigned char)(v & 0xFF);
    if (g_keyboard_cb) {
      g_keyboard_cb(key, 0, 0);
    }
    /* Default quit on ESC if user didn't. */
    if (key == 27) {
      g_running = 0;
    }
  }
#else
  (void)g_keyboard_cb;
#endif
}

void glutMainLoop(void)
{
  while (g_running) {
    miniglutPollKeyboard();

    if (g_idle_cb) {
      g_idle_cb();
    }

    if (g_display_cb) {
      if (g_idle_cb) {
        /* With idle callback, render only when requested. */
        if (g_redisplay) {
          g_redisplay = 0;
          g_display_cb();
        }
      } else {
        /* Without idle callback, render continuously. */
        g_display_cb();
      }
    }
  }

  miniglPlatformShutdown();
  exit(0);
}
