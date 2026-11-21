#ifndef ALPOS_GFX_H
#define ALPOS_GFX_H
#include <stdint.h>
void gfx_init(uint32_t mbi);
int gfx_available(void);
void gfx_desktop(void);
void gfx_clear_terminal(void);
void gfx_terminal_putc(char c);
void gfx_mouse_move(int x, int y);
#endif
