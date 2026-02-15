\
#pragma once
/*
  Host shim so the project can compile on non-Human68k environments for quick checks.

  When __human68k__ is defined, real <iocs.h> and <dos.h> are used instead.
*/
#include <stdint.h>

static inline uint16_t _dos_super(uint16_t x) { (void)x; return 0; }
static inline void _iocs_crtmod(int mode) { (void)mode; }
static inline void _iocs_vpage(int page) { (void)page; }
static inline void _iocs_g_clr_on(void) {}
static inline void _iocs_b_curoff(void) {}
static inline void _iocs_b_curon(void) {}
static inline int  _iocs_b_keysns(void) { return 0; }
static inline int  _iocs_b_keyinp(void) { return 0; }
