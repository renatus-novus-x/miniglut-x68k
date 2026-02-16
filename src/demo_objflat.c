
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "GL/glut.h"
#include "GL/gl.h"
#include "GL/glu.h"

/*
  demo_objflat:
    - Load a Wavefront .obj mesh (positions only; faces triangulated).
    - Render either:
        (A) Wireframe (library draws triangle edges)
        (B) Flat-shaded filled triangles (MiniGL extension + per-face lighting)
    - Toggle with keyboard:
        't' or SPACE : toggle flat <-> wire
        'w'          : wireframe
        'f'          : flat shading
        ESC          : quit

  Usage:
    demo_objflat.x model.obj
*/

typedef struct { float x, y, z; } Vec3;
typedef struct { int i0, i1, i2; } Tri;

typedef struct {
  Vec3 *v;
  int v_count, v_cap;
  Tri  *t;
  int t_count, t_cap;
} Mesh;

static Mesh g_mesh;
static float g_angle = 0.0f;
static int g_mode_flat = 1;
static int g_is_fallback = 0;

/* Headlight in view space: towards +Z (camera at origin looking -Z). */
static const Vec3 g_light_dir_view = { 0.0f, 0.0f, 1.0f };

static void meshFree(Mesh *m)
{
  free(m->v);
  free(m->t);
  memset(m, 0, sizeof(*m));
}

static void meshPushV(Mesh *m, Vec3 p)
{
  if (m->v_count >= m->v_cap) {
    int nc = (m->v_cap == 0) ? 1024 : (m->v_cap * 2);
    Vec3 *nv = (Vec3 *)realloc(m->v, (size_t)nc * sizeof(Vec3));
    if (!nv) exit(1);
    m->v = nv;
    m->v_cap = nc;
  }
  m->v[m->v_count++] = p;
}

static void meshPushTri(Mesh *m, int a, int b, int c)
{
  if (m->t_count >= m->t_cap) {
    int nc = (m->t_cap == 0) ? 2048 : (m->t_cap * 2);
    Tri *nt = (Tri *)realloc(m->t, (size_t)nc * sizeof(Tri));
    if (!nt) exit(1);
    m->t = nt;
    m->t_cap = nc;
  }
  Tri tr; tr.i0 = a; tr.i1 = b; tr.i2 = c;
  m->t[m->t_count++] = tr;
}

static int parseIndex(const char *s, int vcount)
{
  /* Accept "idx", "idx/..", "idx//..", "idx/..../.." */
  int sign = 1;
  if (*s == '-') { sign = -1; s++; }
  int v = 0;
  while (*s && isdigit((unsigned char)*s)) {
    v = v * 10 + (*s - '0');
    s++;
  }
  v *= sign;

  if (v > 0) return v - 1;
  if (v < 0) return vcount + v; /* relative to end */
  return 0;
}

static int loadObj(const char *path, Mesh *m)
{
  FILE *fp = fopen(path, "rb");
  if (!fp) return 0;

  char line[512];
  while (fgets(line, sizeof(line), fp)) {
    char *p = line;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '#' || *p == '\0') continue;

    if (p[0] == 'v' && isspace((unsigned char)p[1])) {
      float x, y, z;
      if (sscanf(p + 1, "%f %f %f", &x, &y, &z) == 3) {
        Vec3 v = { x, y, z };
        meshPushV(m, v);
      }
    } else if (p[0] == 'f' && isspace((unsigned char)p[1])) {
      int idx[32];
      int n = 0;

      char *q = p + 1;
      while (*q) {
        while (*q && isspace((unsigned char)*q)) q++;
        if (!*q || *q == '\n' || *q == '\r') break;
        if (n >= (int)(sizeof(idx)/sizeof(idx[0]))) break;

        idx[n++] = parseIndex(q, m->v_count);

        while (*q && !isspace((unsigned char)*q)) q++;
      }

      if (n >= 3) {
        for (int k = 1; k + 1 < n; k++) {
          meshPushTri(m, idx[0], idx[k], idx[k + 1]);
        }
      }
    }
  }

  fclose(fp);
  return (m->v_count > 0 && m->t_count > 0);
}

static void buildFallbackCube(Mesh *m)
{
  static const float V[8][3] = {
    {-1,-1,-1},{ 1,-1,-1},{ 1, 1,-1},{-1, 1,-1},
    {-1,-1, 1},{ 1,-1, 1},{ 1, 1, 1},{-1, 1, 1}
  };
  static const int F[12][3] = {
    {0,1,2},{0,2,3},
    {4,6,5},{4,7,6},
    {0,4,5},{0,5,1},
    {1,5,6},{1,6,2},
    {2,6,7},{2,7,3},
    {3,7,4},{3,4,0}
  };
  for (int i = 0; i < 8; i++) {
    Vec3 v = { V[i][0], V[i][1], V[i][2] };
    meshPushV(m, v);
  }
  for (int i = 0; i < 12; i++) {
    /* Flip winding so outward normals match the viewer/light convention. */
    meshPushTri(m, F[i][0], F[i][2], F[i][1]);
  }
}

static void normalizeMeshToUnit(Mesh *m)
{
  if (m->v_count <= 0) return;

  float minx=m->v[0].x, maxx=m->v[0].x;
  float miny=m->v[0].y, maxy=m->v[0].y;
  float minz=m->v[0].z, maxz=m->v[0].z;

  for (int i=1;i<m->v_count;i++) {
    Vec3 p=m->v[i];
    if (p.x<minx) minx=p.x; if (p.x>maxx) maxx=p.x;
    if (p.y<miny) miny=p.y; if (p.y>maxy) maxy=p.y;
    if (p.z<minz) minz=p.z; if (p.z>maxz) maxz=p.z;
  }

  float cx=(minx+maxx)*0.5f;
  float cy=(miny+maxy)*0.5f;
  float cz=(minz+maxz)*0.5f;

  float ex=maxx-minx, ey=maxy-miny, ez=maxz-minz;
  float e=ex; if (ey>e) e=ey; if (ez>e) e=ez;
  if (e < 1e-8f) e = 1.0f;
  float s = 2.0f / e;

  for (int i=0;i<m->v_count;i++) {
    m->v[i].x = (m->v[i].x - cx) * s;
    m->v[i].y = (m->v[i].y - cy) * s;
    m->v[i].z = (m->v[i].z - cz) * s;
  }
}

static Vec3 vsub(Vec3 a, Vec3 b) { Vec3 r={a.x-b.x,a.y-b.y,a.z-b.z}; return r; }
static Vec3 vcross(Vec3 a, Vec3 b) {
  Vec3 r={a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x}; return r;
}
static float vdot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }


static Vec3 rotateNormalAxis110(Vec3 v, float angDeg)
{
  /* Match demo_wirecube: glRotatef(angle, 1,1,0) */
  const float inv = 0.7071067811865475f; /* 1/sqrt(2) */
  Vec3 a = { inv, inv, 0.0f };

  float t = angDeg * (float)M_PI / 180.0f;
  float c = cosf(t);
  float s = sinf(t);

  /* Rodrigues' rotation formula */
  Vec3 axv = {
    a.y * v.z - a.z * v.y,
    a.z * v.x - a.x * v.z,
    a.x * v.y - a.y * v.x
  };
  float adv = a.x * v.x + a.y * v.y + a.z * v.z;

  Vec3 r;
  r.x = v.x * c + axv.x * s + a.x * adv * (1.0f - c);
  r.y = v.y * c + axv.y * s + a.y * adv * (1.0f - c);
  r.z = v.z * c + axv.z * s + a.z * adv * (1.0f - c);
  return r;
}



static void drawCubeLines(void)
{
  /* Same cube edges as demo_wirecube.x (no diagonals). */
  const float v[8][3] = {
    {-1, -1, -1},
    { 1, -1, -1},
    { 1,  1, -1},
    {-1,  1, -1},
    {-1, -1,  1},
    { 1, -1,  1},
    { 1,  1,  1},
    {-1,  1,  1},
  };

  const int e[12][2] = {
    {0,1},{1,2},{2,3},{3,0},
    {4,5},{5,6},{6,7},{7,4},
    {0,4},{1,5},{2,6},{3,7},
  };

  glBegin(GL_LINES);
  for (int i = 0; i < 12; i++) {
    int a = e[i][0];
    int b = e[i][1];
    glVertex3f(v[a][0], v[a][1], v[a][2]);
    glVertex3f(v[b][0], v[b][1], v[b][2]);
  }
  glEnd();
}

static void display(void)
{
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  /* Match demo_wirecube.x */
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(60.0f, 1.0f, 0.1f, 100.0f);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0.0f, 0.0f, -5.0f);
  glRotatef(g_angle, 1.0f, 1.0f, 0.0f);
  glScalef(1.2f, 1.2f, 1.2f);

  /* When we are showing the fallback cube in wireframe mode, draw exactly the same edges as demo_wirecube.x. */
  if (!g_mode_flat && g_is_fallback) {
    glColor3ub(255, 255, 255);
    drawCubeLines();
    glFlush();
    glutSwapBuffers();
    return;
  }

  miniglSetFillTriangles(g_mode_flat);

  glBegin(GL_TRIANGLES);
  for (int i = 0; i < g_mesh.t_count; i++) {
    Tri tr = g_mesh.t[i];

    Vec3 p0 = g_mesh.v[tr.i0];
    Vec3 p1 = g_mesh.v[tr.i1];
    Vec3 p2 = g_mesh.v[tr.i2];

    if (g_mode_flat) {
      Vec3 e1 = vsub(p1, p0);
      Vec3 e2 = vsub(p2, p0);
      Vec3 n  = vcross(e1, e2);

      float len2 = vdot(n, n);
      float I = 0.0f;
      if (len2 > 1e-12f) {
        float inv_len = 1.0f / sqrtf(len2);
        n.x *= inv_len; n.y *= inv_len; n.z *= inv_len;

        /* Transform normal with the same rotation as demo_wirecube.x */
        Vec3 nv = rotateNormalAxis110(n, g_angle);

        /* Headlight (vector from surface to camera) in view space: +Z */
        I = vdot(nv, g_light_dir_view);
        if (I < 0.0f) I = 0.0f;
        if (I > 1.0f) I = 1.0f;
      }

      /* Backface cull in filled mode. */
      if (I <= 0.0f) continue;

      unsigned char c = (unsigned char)(I * 255.0f);
      glColor3ub(c, c, c);
    } else {
      glColor3ub(255, 255, 255);
    }

    glVertex3f(p0.x, p0.y, p0.z);
    glVertex3f(p1.x, p1.y, p1.z);
    glVertex3f(p2.x, p2.y, p2.z);
  }
  glEnd();

  glFlush();
  glutSwapBuffers();
}

static void idle(void)
{
  g_angle += 1.0f;
  if (g_angle >= 360.0f) g_angle -= 360.0f;
  glutPostRedisplay();
}

static void key(unsigned char k, int x, int y)
{
  (void)x; (void)y;
  if (k == 27) { glutLeaveMainLoop(); return; }
  if (k == 't' || k == ' ') g_mode_flat = !g_mode_flat;
  else if (k == 'w') g_mode_flat = 0;
  else if (k == 'f') g_mode_flat = 1;
}

int main(int argc, char **argv)
{
  const char *path = (argc >= 2) ? argv[1] : NULL;
  memset(&g_mesh, 0, sizeof(g_mesh));

  if (path && loadObj(path, &g_mesh)) {
    printf("Loaded OBJ: %s (%d vertices, %d triangles)\n", path, g_mesh.v_count, g_mesh.t_count);
    g_is_fallback = 0;
  } else {
    if (path) printf("Failed to load OBJ: %s\n", path);
    printf("Using fallback cube.\n");
    buildFallbackCube(&g_mesh);
    g_is_fallback = 1;
  }
  normalizeMeshToUnit(&g_mesh);

  glutInit(&argc, argv);

  /* Choose 256 or 512 by editing these, or let the library pick the closest. */
  glutInitWindowSize(512, 512);
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
  glutCreateWindow("demo_objflat");

  glutDisplayFunc(display);
  glutKeyboardFunc(key);
  glutIdleFunc(idle);

  glutMainLoop();

  meshFree(&g_mesh);
  return 0;
}
