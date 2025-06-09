#ifndef VGA_DRAW_H
#define VGA_DRAW_H

void api_draw_rectangle(int width, int height, int x, int y, int color);
void api_draw_hline(int x, int y, int length, int color);
void api_draw_vline(int x, int y, int length, int color);
void api_draw_text(int x, int y, const char* text, int color);

#endif // VGA_DRAW_H 