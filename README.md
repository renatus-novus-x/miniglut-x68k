[English](README.md) | [日本語](README.ja.md)

[![windows](https://github.com/renatus-novus-x/miniglut-x68k/workflows/windows/badge.svg)](https://github.com/renatus-novus-x/miniglut-x68k/actions?query=workflow%3Awindows)
[![macos](https://github.com/renatus-novus-x/miniglut-x68k/workflows/macos/badge.svg)](https://github.com/renatus-novus-x/miniglut-x68k/actions?query=workflow%3Amacos)
[![ubuntu](https://github.com/renatus-novus-x/miniglut-x68k/workflows/ubuntu/badge.svg)](https://github.com/renatus-novus-x/miniglut-x68k/actions?query=workflow%3Aubuntu)

![demo](images/tether.gif)

miniglut-x68k (experimental)
============================

A tiny GLUT-compatible(ish) subset for X68000 (elf2x68k / Human68k).

This is not freeglut. There is no window system. It provides a single fullscreen surface,
a minimal event loop, and a small OpenGL-like immediate-mode API that draws directly to GVRAM.

Scope and goals
---------------
- Run small GLUT sample-style programs on X68000 with minimal porting work
- Start from wireframe and simple flat fill, then iterate toward more features
- Keep the codebase small and easy to hack on real hardware/emulators

Implemented GLUT APIs (core)
----------------------------
- glutInit
- glutCreateWindow
- glutDisplayFunc
- glutKeyboardFunc
- glutIdleFunc
- glutMainLoop

Extra compatibility helpers (used by many GLUT samples)
-------------------------------------------------------
- glutInitDisplayMode (GLUT_SINGLE / GLUT_DOUBLE)
- glutInitWindowSize
- glutSwapBuffers
- glutPostRedisplay

Video modes and buffering
-------------------------
- The runtime chooses either 512x512 or 256x256 based on glutInitWindowSize().
  If a different size is requested, it is rounded to the nearest of 256 or 512.
- Double buffering is enabled only when GLUT_DOUBLE is passed to glutInitDisplayMode().
  - 512x512: render to a RAM back buffer and present by copying into GVRAM on swap
  - 256x256: uses a faster present path when possible by flipping a 256x256 tile via IOCS HOME
- On exit, the program restores the previous screen mode and re-enables the text cursor.

OpenGL-like subset (minimal fixed pipeline style)
-------------------------------------------------
Headers are in include/GL. This is a minimal subset, not a full OpenGL implementation.

- Clearing: glClearColor, glClear
- Matrices: glMatrixMode, glLoadIdentity, glPushMatrix, glPopMatrix
- Transforms: glTranslatef, glRotatef, glScalef, glOrtho
- Immediate mode: glBegin/glEnd (GL_LINES, GL_TRIANGLES), glVertex3f
- Color: glColor3ub
- Minimal compatibility switches:
  - Flat fill for triangles is enabled when:
    - glShadeModel(GL_FLAT)
    - glPolygonMode(GL_FRONT_AND_BACK, GL_FILL)
  - Wireframe is selected by:
    - glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)
- GLU-like: gluPerspective only

File layout
-----------
- include/GL/glut.h : GLUT subset header
- include/GL/gl.h   : OpenGL-like subset header
- include/GL/glu.h  : GLU-like subset header (gluPerspective only)
- src/miniglut.c    : GLUT subset implementation
- src/minigl.c      : software renderer and X68000 platform setup/restore
- src/demo_wirecube.c : demo (rotating wireframe cube)
- src/demo_objflat.c  : demo (OBJ loader, wire/flat toggle)

Build (elf2x68k)
----------------
Requirements:
- m68k-xelf-gcc (elf2x68k toolchain) in PATH

Build:
- make

Outputs:
- demo_wirecube.x
- demo_objflat.x

Demos
-----
demo_wirecube
- Run: demo_wirecube.x
- Keys:
  - ESC: quit

demo_objflat
- Run:
  - demo_objflat.x model.obj
  - Example: demo_objflat.x bin/bunny.obj
- Keys:
  - t or Space: toggle flat <-> wire
  - f: flat (filled triangles, flat shading)
  - w: wireframe
  - ESC: quit

Notes and limitations
---------------------
- No textures, no alpha blending, no Z buffer.
- Hidden-surface "look" in demo_objflat is done by a simple triangle depth sort (Painter’s algorithm).
  It can break on complex or self-intersecting meshes.
- No real clipping; primitives are mostly constrained by bounds checks.
- There is no reshape callback, mouse input, menus, etc.

Downloads (prebuilt)
--------------------
- [demo_wirecube.x](https://raw.githubusercontent.com/renatus-novus-x/miniglut-x68k/main/bin/demo_wirecube.x)
- [demo_objflat.x](https://raw.githubusercontent.com/renatus-novus-x/miniglut-x68k/main/bin/demo_objflat.x)
