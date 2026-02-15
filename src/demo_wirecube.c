#include <stdlib.h>
#include <math.h>
#include "GL/glut.h"
#include "GL/gl.h"
#include "GL/glu.h"

static float g_angle = 0.0f;

static void drawCubeLines(void)
{
  /* 8 cube vertices */
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

  /* 12 edges as pairs of indices */
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

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(60.0f, 1.0f, 0.1f, 100.0f);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0.0f, 0.0f, -5.0f);
  glRotatef(g_angle, 1.0f, 1.0f, 0.0f);
  glScalef(1.2f, 1.2f, 1.2f);

  glColor3ub(255, 255, 255);
  drawCubeLines();

  glFlush();
  glutSwapBuffers();
}

static void idle(void)
{
  g_angle += 1.0f;
  if (g_angle >= 360.0f) g_angle -= 360.0f;
  glutPostRedisplay();
}

static void keyboard(unsigned char key, int x, int y)
{
  (void)x;
  (void)y;

  if (key == 27 || key == 'q' || key == 'Q') {
    glutLeaveMainLoop();
  }
}

int main(int argc, char **argv)
{
  glutInit(&argc, argv);
  glutInitDisplayMode(/*GLUT_SINGLE*/ GLUT_DOUBLE| GLUT_RGB);
  glutInitWindowSize(512, 512);
  glutCreateWindow("miniglut-x68k demo");

  glutDisplayFunc(display);
  glutIdleFunc(idle);
  glutKeyboardFunc(keyboard);

  glutMainLoop();
  return 0;
}
