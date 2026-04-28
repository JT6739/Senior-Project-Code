/**
 * ssd1306_b.h
 *
 * Second SSD1306 display, hardcoded to I2C2 (hi2c2).
 * Uses the same typedefs (SSD1306_COLOR, SSD1306_Font_t) and fonts
 * (Font_7x10, Font_11x18, Font_16x26 ...) as the original ssd1306.h —
 * only the function names and internal framebuffer are duplicated.
 *
 * Pair file: ssd1306_b.c
 * Place this file in the same folder as ssd1306.h (e.g. Core/Inc/).
 */

#ifndef __SSD1306_B_H__
#define __SSD1306_B_H__

#include "ssd1306.h"   /* SSD1306_COLOR, SSD1306_Font_t, etc. */

_BEGIN_STD_C

/* Renamed API — second display, talks to hi2c2 */
void ssd1306b_Init(void);
void ssd1306b_Fill(SSD1306_COLOR color);
void ssd1306b_UpdateScreen(void);
void ssd1306b_DrawPixel(uint8_t x, uint8_t y, SSD1306_COLOR color);
char ssd1306b_WriteChar  (char ch,   SSD1306_Font_t Font, SSD1306_COLOR color);
char ssd1306b_WriteString(char *str, SSD1306_Font_t Font, SSD1306_COLOR color);
void ssd1306b_SetCursor(uint8_t x, uint8_t y);

_END_STD_C

#endif /* __SSD1306_B_H__ */
