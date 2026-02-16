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

/*
  Software wireframe renderer to GVRAM (X68000).

  Supported modes (selected at startup via glutInitWindowSize):
    - 512x512, 16-bit color: CRTMOD 12
    - 256x256, 16-bit color: CRTMOD 14 (virtual 512x512, we can flip via HOME)
*/

#define VRAM_STRIDE_W 512
#define VRAM_H        512
#define GVRAM_BASE    0xC00000

static int g_crt_mode = 12;
static int g_view_w = 512;
static int g_view_h = 512;
static int g_req_w = 512;
static int g_req_h = 512;

#define GFX_W (g_view_w)
#define GFX_H (g_view_h)

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

/* Minimal state for GLUT-like toggles. */
static GLenum g_shade_model = GL_SMOOTH;
static GLenum g_polygon_mode = GL_FILL;

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

#ifdef __human68k__
/* "Tiled" VRAM double-buffering for CRT_MODE 14 (256x256 shown inside a 512x512 16bpp plane).
   We flip by changing the HOME (display origin) between (0,0) and (256,0) and drawing into the other half.
   This avoids RAM->VRAM memcpy and touches ~1/4 pixels per frame.
*/
static int g_vram_tiled_flip = 0;
static int g_view_off_x = 0;
static int g_view_off_y = 0;
static int g_draw_off_x = 0;
static int g_draw_off_y = 0;
#endif

static volatile uint16_t *gvramPtr(int x, int y)
{
  if (g_double_enabled && g_back_buf) {
    return (volatile uint16_t *)&g_back_buf[y * GFX_W + x];
  }

#ifdef __human68k__
  /* Direct VRAM access (optionally into the "back" tile). */
  int gx = x;
  int gy = y;
  if (g_vram_tiled_flip) {
    gx += g_draw_off_x;
    gy += g_draw_off_y;
  }
  return (volatile uint16_t *)(uintptr_t)(GVRAM_BASE + (gy * VRAM_STRIDE_W + gx) * 2);
#else
  /* Host build: no real VRAM. */
  return (volatile uint16_t *)(uintptr_t)0;
#endif
}

static void putPixel(int x, int y, uint16_t c)
{
  if ((unsigned)x >= GFX_W || (unsigned)y >= GFX_H) return;
  *gvramPtr(x, y) = c;
}

static void drawLine(int x0, int y0, int x1, int y1, uint16_t c)
{
/*
  Line rasterizer: Default is fixed-point DDA (single divide, then add/shift).
  Define MINIGL_USE_BRESENHAM to build the previous integer Bresenham version
  for A/B comparisons.

  Note:
    - Classic float DDA can be slower on 68000 (no FPU on many models).
    - This implementation uses 16.16 fixed-point to keep it fast-ish.
*/
#ifdef MINIGL_USE_BRESENHAM
  /* Previous implementation (integer Bresenham). */
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
#else
  /* Fixed-point DDA. */
  int dx = x1 - x0;
  int dy = y1 - y0;
  int adx = (dx < 0) ? -dx : dx;
  int ady = (dy < 0) ? -dy : dy;

  /* Fast paths. */
  if (dy == 0) {
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    for (int x = x0; x <= x1; ++x) putPixel(x, y0, c);
    return;
  }
  if (dx == 0) {
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    for (int y = y0; y <= y1; ++y) putPixel(x0, y, c);
    return;
  }

  int steps = (adx > ady) ? adx : ady;
  if (steps <= 0) {
    putPixel(x0, y0, c);
    return;
  }

  /* 16.16 fixed-point increments (one division each). */
  int32_t x = ((int32_t)x0) << 16;
  int32_t y = ((int32_t)y0) << 16;
  int32_t x_inc = (((int32_t)dx) << 16) / (int32_t)steps;
  int32_t y_inc = (((int32_t)dy) << 16) / (int32_t)steps;

  for (int i = 0; i <= steps; ++i) {
    putPixel((int)(x >> 16), (int)(y >> 16), c);
    x += x_inc;
    y += y_inc;
  }
#endif
}

/* Fill a horizontal span [x0, x1] at scanline y with color c (inclusive). */
static void fillSpan(int y, int x0, int x1, uint16_t c)
{
  if ((unsigned)y >= (unsigned)GFX_H) return;
  if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
  if (x1 < 0 || x0 >= GFX_W) return;
  if (x0 < 0) x0 = 0;
  if (x1 >= GFX_W) x1 = GFX_W - 1;

  int n = x1 - x0 + 1;
  volatile uint16_t *p = gvramPtr(x0, y);
  while (n--) *p++ = c;
}

/* Flat-color triangle fill (no Z, no clipping, no texture). */
static void fillTriangleFlat(ScreenPt a, ScreenPt b, ScreenPt c, uint16_t col)
{
  if (!a.valid || !b.valid || !c.valid) return;

  /* Sort vertices by y (ascending). */
  ScreenPt v0 = a, v1 = b, v2 = c;
  if (v1.y < v0.y) { ScreenPt t=v0; v0=v1; v1=t; }
  if (v2.y < v0.y) { ScreenPt t=v0; v0=v2; v2=t; }
  if (v2.y < v1.y) { ScreenPt t=v1; v1=v2; v2=t; }

  int y0 = v0.y, y1 = v1.y, y2 = v2.y;
  int x0 = v0.x, x1 = v1.x, x2 = v2.x;

  /* Fully horizontal -> nothing meaningful to fill. */
  if (y0 == y2) return;

  /* 16.16 fixed-point edge walker for v0->v2. */
  int dy02 = y2 - y0;
  int dx02 = x2 - x0;
  int32_t sx02 = ((int32_t)dx02 << 16) / dy02;
  int32_t x02 = ((int32_t)x0 << 16);

  /* Upper part v0->v1 (if any). */
  if (y1 > y0) {
    int dy01 = y1 - y0;
    int dx01 = x1 - x0;
    int32_t sx01 = ((int32_t)dx01 << 16) / dy01;
    int32_t x01 = ((int32_t)x0 << 16);

    for (int y = y0; y < y1; y++) {
      int xa = (int)(x02 >> 16);
      int xb = (int)(x01 >> 16);
      fillSpan(y, xa, xb, col);
      x02 += sx02;
      x01 += sx01;
    }
  } else {
    /* Flat-top: advance x02 to y1. */
    x02 += sx02 * (y1 - y0);
  }

  /* Lower part v1->v2 (if any). */
  if (y2 > y1) {
    int dy12 = y2 - y1;
    int dx12 = x2 - x1;
    int32_t sx12 = ((int32_t)dx12 << 16) / dy12;
    int32_t x12 = ((int32_t)x1 << 16);

    for (int y = y1; y <= y2; y++) {
      int xa = (int)(x02 >> 16);
      int xb = (int)(x12 >> 16);
      fillSpan(y, xa, xb, col);
      x02 += sx02;
      x12 += sx12;
    }
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

void miniglSetRequestedSize(int w, int h)
{
  if (w > 0) g_req_w = w;
  if (h > 0) g_req_h = h;
}

void miniglSwapBuffers(void)
{
#ifdef __human68k__
  if (g_vram_tiled_flip) {
    /* Present the region we just rendered by moving the display origin (HOME). */
    g_view_off_x = g_draw_off_x;
    g_view_off_y = g_draw_off_y;

    /* Toggle draw target to the other 256x256 tile. */
    g_draw_off_x = (g_view_off_x == 0) ? 256 : 0;
    g_draw_off_y = 0;

    /* Page argument is kept as 0 to match existing vpage(0) usage in this codebase. */
    _iocs_home(0, g_view_off_x, g_view_off_y);
    return;
  }
#endif

  if (!g_double_enabled || !g_front_buf || !g_back_buf) return;

  /* Swap front/back, then present the new front to VRAM. */
  uint16_t *tmp = g_front_buf;
  g_front_buf = g_back_buf;
  g_back_buf = tmp;

#ifdef __human68k__
  /* Copy only the visible 256x256 area into VRAM (stride is 512 in 16bpp modes). */
  volatile uint16_t *dst = (volatile uint16_t *)(uintptr_t)GVRAM_BASE;
  const uint16_t *src = (const uint16_t *)g_front_buf;
  for (int y = 0; y < GFX_H; y++) {
    memcpy((void *)(dst + (size_t)y * (size_t)VRAM_STRIDE_W),
           (const void *)(src + (size_t)y * (size_t)GFX_W),
           (size_t)GFX_W * 2u);
  }
#endif
}


void miniglPlatformInit(void)
{
  if (g_platform_inited) return;
  g_platform_inited = 1;

  /* Choose 512x512 (CRTMOD 12) or 256x256 (CRTMOD 14) based on the requested window size. */
  {
    int d256 = abs(g_req_w - 256) + abs(g_req_h - 256);
    int d512 = abs(g_req_w - 512) + abs(g_req_h - 512);
    if (d256 < d512) {
      g_crt_mode = 14;
      g_view_w = 256;
      g_view_h = 256;
    } else {
      g_crt_mode = 12;
      g_view_w = 512;
      g_view_h = 512;
    }
  }

#ifdef __human68k__
  /* Reset flip state; re-initialized below if supported by the selected mode. */
  g_vram_tiled_flip = 0;
  g_view_off_x = 0;  g_view_off_y = 0;
  g_draw_off_x = 0;  g_draw_off_y = 0;
#endif


    /* Allocate RAM buffers for double buffering if requested.
     On Human68k + CRT_MODE 14, we prefer "tiled VRAM" flip to avoid memcpy. */
#ifdef __human68k__
  if (g_double_enabled && g_crt_mode == 14) {
    g_vram_tiled_flip = 1;
    g_view_off_x = 0;  g_view_off_y = 0;
    g_draw_off_x = 256; g_draw_off_y = 0;
  }
#endif

  if (g_double_enabled) {
#ifdef __human68k__
    if (!g_vram_tiled_flip)
#endif
    {
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
#ifdef __human68k__
        g_vram_tiled_flip = 0;
#endif
      } else {
        /* Initialize both buffers to black. */
        memset(g_front_buf, 0, n * 2u);
        memset(g_back_buf, 0, n * 2u);
      }
    }
  }

#ifdef __human68k__
  if (!g_saved_crtmod_valid) {
    /* IOCS _CRTMOD(-1) returns current CRT mode. */
    g_saved_crtmod = _iocs_crtmod(-1);
    g_saved_crtmod_valid = 1;
  }

  _iocs_crtmod(g_crt_mode);

  /* Hide text cursor (B_CUROFF only stops blinking; OS_CUROF actually hides it). */
  _iocs_os_curof();
  _iocs_b_curoff();

  _iocs_vpage(0);
  _iocs_g_clr_on();
  /* Ensure the 256x256 view starts at (0,0). */
  _iocs_home(0, 0, 0);
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

void miniglSetFillTriangles(int enable)
{
  /* Backward-compatible extension: map to standard-like state toggles. */
  if (enable) {
    glShadeModel(GL_FLAT);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  } else {
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  }
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

void glShadeModel(GLenum mode)
{
  g_shade_model = mode;
}

void glPolygonMode(GLenum face, GLenum mode)
{
  (void)face;
  g_polygon_mode = mode;
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
        int do_fill = (g_polygon_mode == GL_FILL) && (g_shade_model == GL_FLAT);
        if (do_fill) {
          fillTriangleFlat(g_tri_pts[0], g_tri_pts[1], g_tri_pts[2], g_draw_color);
        } else {
          emitLine(g_tri_pts[0], g_tri_pts[1]);
          emitLine(g_tri_pts[1], g_tri_pts[2]);
          emitLine(g_tri_pts[2], g_tri_pts[0]);
        }
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
