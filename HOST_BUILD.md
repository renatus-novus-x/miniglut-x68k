Host build (Windows / Ubuntu / macOS)
--------------------------------

This repository contains X68000-specific GLUT headers under include/GL/.
For host builds we intentionally do NOT add "include/" to the include path.

Build demo_wirecube_host with CMake:

  mkdir build_host
  cd build_host
  cmake ..
  cmake --build .

Output:
- demo_wirecube_host (or demo_wirecube_host.exe)

X68000 builds are still done via the existing Makefile (m68k-xelf-gcc).
