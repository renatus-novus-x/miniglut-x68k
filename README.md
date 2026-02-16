<img src="https://raw.githubusercontent.com/renatus-novus-x/miniglut-x68k/main/images/tether.gif" title="tether" />

miniglut-x68k (experimental)
===========================

This is a tiny, GLUT-compatible(ish) subset intended for elf2x68k / Human68k.
It is not freeglut. It is a single fullscreen "window" plus a minimal event loop.

Implemented GLUT APIs (core asked in this prototype)
- glutInit
- glutCreateWindow
- glutDisplayFunc
- glutMainLoop
- glutKeyboardFunc
- glutIdleFunc

Extra compatibility helpers (commonly used by GLUT samples)
- glutInitDisplayMode (GLUT_SINGLE or GLUT_DOUBLE)
- glutSwapBuffers
- glutPostRedisplay
- glutLeaveMainLoop

A minimal "OpenGL-like" fixed pipeline subset is also provided to let you start with
wireframe-only demos without external OpenGL libraries. It draws directly into GVRAM
(512x512, 16-bit mode via IOCS _CRTMOD(12)).

Files
- include/GL/glut.h : minimal GLUT header
- include/GL/gl.h   : minimal OpenGL-like header (immediate mode, wireframe)
- include/GL/glu.h  : minimal GLU-like header (gluPerspective only)
- src/miniglut.c    : GLUT subset implementation
- src/minigl.c      : OpenGL-like wireframe renderer (software line rasterizer)
- src/demo_wirecube.c : demo (rotating wireframe cube, ESC to quit)

Build (elf2x68k)
1) Ensure m68k-xelf-gcc is in PATH.
2) Run:
   make

It should produce demo_wirecube.x (and demo_wirecube.x.elf).

Notes / limitations
- No textures, no Z-buffer, no blending. Wireframe only.
- No real window system: one fullscreen surface only.
- No mouse, no menus, no reshape callbacks yet.
- No clipping; lines outside the screen are simply clipped by bounds checks.
- Performance is intentionally simple (pixel-by-pixel line drawing).

References that informed the minimal X68000 video/IOCS access style:
- Target-Earth X68000 dev code examples (GVRAM access, _dos_super, _iocs_crtmod) 

## Download
- [demo_wirecube.x](https://raw.githubusercontent.com/renatus-novus-x/miniglut-x68k/main/bin/demo_wirecube.x)
- [demo_objflat.x](https://raw.githubusercontent.com/renatus-novus-x/miniglut-x68k/main/bin/demo_objflat.x)

## demo_wirecube

Run:
- `demo_wirecube.x`

Keys:
- `ESC`: quit

## demo_objflat

Run:
- `demo_objflat.x model.obj`

Keys:
- `t` or Space: toggle flat <-> wire
- `f`: flat shading
- `w`: wireframe
- `ESC`: quit
