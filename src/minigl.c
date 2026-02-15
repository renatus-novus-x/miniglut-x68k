#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef __human68k__
  #include <x68k/iocs.h>
  #include <x68k/dos.h>
#else
  #include "host_shim.h"
#endif

#include "GL/gl.h"
#include "GL/glu.h"

/*
  Minimal software renderer to GVRAM (X68000) for wireframe only.

  Video mode: 512x512, 16-bit color (IOCS _CRTMOD(12)).
  Pixel format: GRB + intensity bit (GRBI), 5 bits per channel.
*/

#define CRT_MODE   12
#define GFX_W      512
#define GFX_H      512
#define GVRAM_BASE 0xC00000

/* Macro style taken from common X68000 examples. */
#define RGB2GRB(r, g, b) ( ((uint16_t)((b)&0xF8) >> 2) | ((uint16_t)((g)&0xF8) << 8) | ((uint16_t)((r)&0xF8) << 3) | 1 )

typedef struct {
  float m[16];
} Mat4;

static Mat4 g_proj_stack[16];
static Mat4 g_mv_stack[16];
static int g_proj_top = 0;
static int g_mv_top = 0;
static GLenum g_matrix_mode = GL_MODELVIEW;

static uint16_t g_clear_color = 0;
static uint16_t g_draw_color = 0xFFFF;

static GLenum g_begin_mode = 0;
static int g_in_begin = 0;

/* Small vertex buffering to draw wireframe primitives without extra storage. */
typedef struct {
  int x;
  int y;
  int valid;
} ScreenPt;

static ScreenPt g_first_pt;
static ScreenPt g_prev_pt;
static int g_tri_count = 0;
static ScreenPt g_tri_pts[3];

/* Supervisor mode token for direct VRAM access. */
static uint16_t g_super_token = 0;
static int g_platform_inited = 0;

/* Saved state to restore on exit. */
static int g_saved_crtmod = 0;
static int g_saved_crtmod_valid = 0;

/* Double buffer (RAM) - enabled only if GLUT_DOUBLE was requested. */
static int g_double_enabled = 0;
static uint16_t *g_front_buf = NULL;
static uint16_t *g_back_buf = NULL;

static volatile uint16_t *gvramPtr(int x, int y)
{
  if (g_double_enabled && g_back_buf) {
    return (volatile uint16_t *)&g_back_buf[y * GFX_W + x];
  }
  return (volatile uint16_t *)(GVRAM_BASE + (y * GFX_W + x) * 2);
}
static void putPixel(int x, int y, uint16_t c)
{
  if ((unsigned)x >= GFX_W || (unsigned)y >= GFX_H) return;
  *gvramPtr(x, y) = c;
}

static void drawLine(int x0, int y0, int x1, int y1, uint16_t c)
{
  int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
  int sx = (x0 < x1) ? 1 : -1;
  int dy = (y1 > y0) ? (y0 - y1) : (y1 - y0); /* negative */
  int sy = (y0 < y1) ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    putPixel(x0, y0, c);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

static void matIdentity(Mat4 *o)
{
  memset(o->m, 0, sizeof(o->m));
  o->m[0] = o->m[5] = o->m[10] = o->m[15] = 1.0f;
}

static Mat4 matMul(const Mat4 *a, const Mat4 *b)
{
  Mat4 r;
  for (int col = 0; col < 4; col++) {
    for (int row = 0; row < 4; row++) {
      float s = 0.0f;
      for (int k = 0; k < 4; k++) {
        s += a->m[k * 4 + row] * b->m[col * 4 + k];
      }
      r.m[col * 4 + row] = s;
    }
  }
  return r;
}

static Mat4 *curMat(void)
{
  if (g_matrix_mode == GL_PROJECTION) return &g_proj_stack[g_proj_top];
  return &g_mv_stack[g_mv_top];
}

static void matTranslate(Mat4 *m, float x, float y, float z)
{
  Mat4 t;
  matIdentity(&t);
  t.m[12] = x;
  t.m[13] = y;
  t.m[14] = z;
  *m = matMul(m, &t);
}

static void matScale(Mat4 *m, float x, float y, float z)
{
  Mat4 s;
  matIdentity(&s);
  s.m[0] = x;
  s.m[5] = y;
  s.m[10] = z;
  *m = matMul(m, &s);
}

static void matRotateAxis(Mat4 *m, float angleDeg, float ax, float ay, float az)
{
  float len = sqrtf(ax * ax + ay * ay + az * az);
  if (len <= 1e-8f) return;
  ax /= len; ay /= len; az /= len;

  float a = angleDeg * (float)M_PI / 180.0f;
  float c = cosf(a);
  float s = sinf(a);
  float ic = 1.0f - c;

  Mat4 r;
  matIdentity(&r);

  r.m[0]  = ax * ax * ic + c;
  r.m[1]  = ay * ax * ic + az * s;
  r.m[2]  = az * ax * ic - ay * s;

  r.m[4]  = ax * ay * ic - az * s;
  r.m[5]  = ay * ay * ic + c;
  r.m[6]  = az * ay * ic + ax * s;

  r.m[8]  = ax * az * ic + ay * s;
  r.m[9]  = ay * az * ic - ax * s;
  r.m[10] = az * az * ic + c;

  *m = matMul(m, &r);
}

static void matOrtho(Mat4 *m, float l, float r, float b, float t, float n, float f)
{
  Mat4 o;
  matIdentity(&o);
  o.m[0] = 2.0f / (r - l);
  o.m[5] = 2.0f / (t - b);
  o.m[10] = -2.0f / (f - n);
  o.m[12] = -(r + l) / (r - l);
  o.m[13] = -(t + b) / (t - b);
  o.m[14] = -(f + n) / (f - n);
  *m = matMul(m, &o);
}

static void matPerspective(Mat4 *m, float fovyDeg, float aspect, float zNear, float zFar)
{
  float f = 1.0f / tanf((fovyDeg * (float)M_PI / 180.0f) * 0.5f);

  Mat4 p;
  memset(p.m, 0, sizeof(p.m));
  p.m[0] = f / aspect;
  p.m[5] = f;
  p.m[10] = (zFar + zNear) / (zNear - zFar);
  p.m[11] = -1.0f;
  p.m[14] = (2.0f * zFar * zNear) / (zNear - zFar);
  *m = matMul(m, &p);
}

static ScreenPt projectToScreen(float x, float y, float z)
{
  /* Transform: clip = P * MV * [x y z 1] */
  Mat4 mv = g_mv_stack[g_mv_top];
  Mat4 pr = g_proj_stack[g_proj_top];
  Mat4 pmv = matMul(&pr, &mv);

  float vx = x, vy = y, vz = z, vw = 1.0f;

  float cx =
    pmv.m[0] * vx + pmv.m[4] * vy + pmv.m[8]  * vz + pmv.m[12] * vw;
  float cy =
    pmv.m[1] * vx + pmv.m[5] * vy + pmv.m[9]  * vz + pmv.m[13] * vw;
  float cz =
    pmv.m[2] * vx + pmv.m[6] * vy + pmv.m[10] * vz + pmv.m[14] * vw;
  float cw =
    pmv.m[3] * vx + pmv.m[7] * vy + pmv.m[11] * vz + pmv.m[15] * vw;

  ScreenPt out;
  out.valid = 0;

  if (fabsf(cw) < 1e-8f) return out;

  float ndc_x = cx / cw;
  float ndc_y = cy / cw;

  int sx = (int)((ndc_x * 0.5f + 0.5f) * (float)(GFX_W - 1));
  int sy = (int)((1.0f - (ndc_y * 0.5f + 0.5f)) * (float)(GFX_H - 1));

  out.x = sx;
  out.y = sy;
  out.valid = 1;
  return out;
}

static void emitLine(ScreenPt a, ScreenPt b)
{
  if (!a.valid || !b.valid) return;
  drawLine(a.x, a.y, b.x, b.y, g_draw_color);
}

void miniglSetDoubleBuffered(int enabled)
{
  g_double_enabled = enabled ? 1 : 0;
}

void miniglSwapBuffers(void)
{
  if (!g_double_enabled || !g_front_buf || !g_back_buf) return;
  /* Swap front/back, then present the new front to VRAM. */
  uint16_t *tmp = g_front_buf;
  g_front_buf = g_back_buf;
  g_back_buf = tmp;
  /* Copy to VRAM (512x512x16bpp = 512 KB). */
#ifdef __human68k__
  void *dst = (void *)(uintptr_t)GVRAM_BASE;
  memcpy(dst, (const void *)g_front_buf, (size_t)GFX_W * (size_t)GFX_H * 2u);
#endif
}

void miniglPlatformInit(void)
{
  if (g_platform_inited) return;
  g_platform_inited = 1;

  /* Allocate RAM buffers for double buffering if requested. */
  if (g_double_enabled) {
    size_t n = (size_t)GFX_W * (size_t)GFX_H;
    g_front_buf = (uint16_t *)malloc(n * 2u);
    g_back_buf  = (uint16_t *)malloc(n * 2u);
    if (!g_front_buf || !g_back_buf) {
      /* Fallback to single buffer if allocation failed. */
      free(g_front_buf);
      free(g_back_buf);
      g_front_buf = NULL;
      g_back_buf = NULL;
      g_double_enabled = 0;
    } else {
      /* Initialize both buffers to black. */
      memset(g_front_buf, 0, n * 2u);
      memset(g_back_buf, 0, n * 2u);
    }
  }

#ifdef __human68k__
  if (!g_saved_crtmod_valid) {
    /* IOCS _CRTMOD(-1) returns current CRT mode. */
    g_saved_crtmod = _iocs_crtmod(-1);
    g_saved_crtmod_valid = 1;
  }

  _iocs_crtmod(CRT_MODE);

  /* Hide text cursor (B_CUROFF only stops blinking; OS_CUROF actually hides it). */
  _iocs_os_curof();
  _iocs_b_curoff();

  _iocs_vpage(0);
  _iocs_g_clr_on();
  g_super_token = _dos_super(0);
#endif

  matIdentity(&g_proj_stack[0]);
  matIdentity(&g_mv_stack[0]);
  g_proj_top = 0;
  g_mv_top = 0;
}

void miniglPlatformShutdown(void)
{
  if (!g_platform_inited) return;
  g_platform_inited = 0;

#ifdef __human68k__
  _dos_super(g_super_token);
  if (g_saved_crtmod_valid) {
    _iocs_crtmod(g_saved_crtmod);
  }
  _iocs_os_curon();
  _iocs_b_curon();
#endif
}

int miniglGetWidth(void) { return GFX_W; }
int miniglGetHeight(void) { return GFX_H; }

/* OpenGL-like API */
void glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
  (void)a;
  int ri = (int)(r * 255.0f); if (ri < 0) ri = 0; if (ri > 255) ri = 255;
  int gi = (int)(g * 255.0f); if (gi < 0) gi = 0; if (gi > 255) gi = 255;
  int bi = (int)(b * 255.0f); if (bi < 0) bi = 0; if (bi > 255) bi = 255;
  g_clear_color = RGB2GRB((uint8_t)ri, (uint8_t)gi, (uint8_t)bi);
}

void glClear(GLbitfield mask)
{
  if (!(mask & GL_COLOR_BUFFER_BIT)) return;

#ifdef __human68k__
  /* Fast clear if black (single-buffer only). */
  if (!g_double_enabled && g_clear_color == 0) {
    _iocs_g_clr_on();
    return;
  }
#endif

  for (int y = 0; y < GFX_H; y++) {
    for (int x = 0; x < GFX_W; x++) {
      putPixel(x, y, g_clear_color);
    }
  }
}

void glMatrixMode(GLenum mode)
{
  if (mode == GL_MODELVIEW || mode == GL_PROJECTION) {
    g_matrix_mode = mode;
  }
}

void glLoadIdentity(void)
{
  matIdentity(curMat());
}

void glPushMatrix(void)
{
  if (g_matrix_mode == GL_PROJECTION) {
    if (g_proj_top < 15) {
      g_proj_stack[g_proj_top + 1] = g_proj_stack[g_proj_top];
      g_proj_top++;
    }
  } else {
    if (g_mv_top < 15) {
      g_mv_stack[g_mv_top + 1] = g_mv_stack[g_mv_top];
      g_mv_top++;
    }
  }
}

void glPopMatrix(void)
{
  if (g_matrix_mode == GL_PROJECTION) {
    if (g_proj_top > 0) g_proj_top--;
  } else {
    if (g_mv_top > 0) g_mv_top--;
  }
}

void glTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
  matTranslate(curMat(), x, y, z);
}

void glRotatef(GLfloat angleDeg, GLfloat x, GLfloat y, GLfloat z)
{
  matRotateAxis(curMat(), angleDeg, x, y, z);
}

void glScalef(GLfloat x, GLfloat y, GLfloat z)
{
  matScale(curMat(), x, y, z);
}

void glOrtho(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f)
{
  matOrtho(curMat(), l, r, b, t, n, f);
}

void gluPerspective(GLfloat fovyDeg, GLfloat aspect, GLfloat zNear, GLfloat zFar)
{
  matPerspective(curMat(), fovyDeg, aspect, zNear, zFar);
}

void glColor3ub(GLubyte r, GLubyte g, GLubyte b)
{
  g_draw_color = RGB2GRB(r, g, b);
}

void glBegin(GLenum mode)
{
  g_begin_mode = mode;
  g_in_begin = 1;
  g_first_pt.valid = 0;
  g_prev_pt.valid = 0;
  g_tri_count = 0;
}

void glVertex3f(GLfloat x, GLfloat y, GLfloat z)
{
  if (!g_in_begin) return;

  ScreenPt p = projectToScreen(x, y, z);

  switch (g_begin_mode) {
    case GL_LINES:
      if (!g_prev_pt.valid) {
        g_prev_pt = p;
      } else {
        emitLine(g_prev_pt, p);
        g_prev_pt.valid = 0;
      }
      break;

    case GL_LINE_STRIP:
      if (g_prev_pt.valid) {
        emitLine(g_prev_pt, p);
      }
      g_prev_pt = p;
      break;

    case GL_LINE_LOOP:
      if (!g_first_pt.valid) {
        g_first_pt = p;
        g_prev_pt = p;
      } else {
        emitLine(g_prev_pt, p);
        g_prev_pt = p;
      }
      break;

    case GL_TRIANGLES:
      if (g_tri_count < 3) {
        g_tri_pts[g_tri_count++] = p;
      }
      if (g_tri_count == 3) {
        emitLine(g_tri_pts[0], g_tri_pts[1]);
        emitLine(g_tri_pts[1], g_tri_pts[2]);
        emitLine(g_tri_pts[2], g_tri_pts[0]);
        g_tri_count = 0;
      }
      break;

    default:
      /* Unsupported modes are ignored. */
      break;
  }
}

void glEnd(void)
{
  if (!g_in_begin) return;

  if (g_begin_mode == GL_LINE_LOOP) {
    if (g_first_pt.valid && g_prev_pt.valid) {
      emitLine(g_prev_pt, g_first_pt);
    }
  }

  g_in_begin = 0;
  g_begin_mode = 0;
}

void glFlush(void)
{
  /* No-op in this prototype. */
}
