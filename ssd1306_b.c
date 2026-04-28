/**
 * ssd1306_b.c
 *
 * Minimal SSD1306 driver for a SECOND display on I2C2 (hi2c2).
 * Designed to coexist with the afiskon ssd1306 library running on hi2c1
 * for a first display.
 *
 * v2: I2C calls now have a 100ms timeout instead of HAL_MAX_DELAY, and
 *     ssd1306b_Init probes the bus first with HAL_I2C_IsDeviceReady. If
 *     the second display is missing or its I2C lines aren't working,
 *     init returns early and the rest of the program (including the
 *     first display) keeps running.
 */

#include "ssd1306_b.h"
#include <string.h>

#ifndef SSD1306_BUFFER_SIZE
#define SSD1306_BUFFER_SIZE   (SSD1306_WIDTH * SSD1306_HEIGHT / 8)
#endif

/* Per-call timeout. 100 ms is plenty for any normal I2C transaction at
 * 100 kHz; if a transfer takes longer than this, something is wrong and
 * we'd rather drop the frame than hang the MCU.                         */
#define SSD1306B_I2C_TIMEOUT_MS  100U

extern I2C_HandleTypeDef hi2c2;

static SSD1306_t SSD1306b;
static uint8_t   SSD1306b_Buffer[SSD1306_BUFFER_SIZE];
static uint8_t   SSD1306b_Present = 0;   /* 1 once Init verifies the bus */

/* ── Low-level I2C ──────────────────────────────────────────────────── */
static void ssd1306b_WriteCommand(uint8_t byte)
{
    if (!SSD1306b_Present) return;
    HAL_I2C_Mem_Write(&hi2c2, SSD1306_I2C_ADDR, 0x00, 1,
                      &byte, 1, SSD1306B_I2C_TIMEOUT_MS);
}

static void ssd1306b_WriteData(uint8_t *buffer, size_t buff_size)
{
    if (!SSD1306b_Present) return;
    HAL_I2C_Mem_Write(&hi2c2, SSD1306_I2C_ADDR, 0x40, 1,
                      buffer, buff_size, SSD1306B_I2C_TIMEOUT_MS);
}

/* ── Init ──────────────────────────────────────────────────────────── */
void ssd1306b_Init(void)
{
    HAL_Delay(100);                       /* let the panel boot         */

    /* Probe the bus first. If nothing responds, mark display absent and
     * return — main code will keep running and display 1 will work.    */
    if (HAL_I2C_IsDeviceReady(&hi2c2, SSD1306_I2C_ADDR, 3, 100) != HAL_OK) {
        SSD1306b_Present = 0;
        return;
    }
    SSD1306b_Present = 1;

    ssd1306b_WriteCommand(0xAE);          /* display OFF                */
    ssd1306b_WriteCommand(0x20); ssd1306b_WriteCommand(0x00);  /* horiz */
    ssd1306b_WriteCommand(0xB0);          /* page start                 */
    ssd1306b_WriteCommand(0xC8);          /* COM scan dir remapped      */
    ssd1306b_WriteCommand(0x00);
    ssd1306b_WriteCommand(0x10);
    ssd1306b_WriteCommand(0x40);          /* start line = 0             */
    ssd1306b_WriteCommand(0x81); ssd1306b_WriteCommand(0xFF);  /* contrast */
    ssd1306b_WriteCommand(0xA1);          /* segment remap              */
    ssd1306b_WriteCommand(0xA6);          /* normal display             */
    ssd1306b_WriteCommand(0xA8); ssd1306b_WriteCommand(0x3F);  /* 1/64 mux */
    ssd1306b_WriteCommand(0xA4);          /* output follows RAM         */
    ssd1306b_WriteCommand(0xD3); ssd1306b_WriteCommand(0x00);  /* offset */
    ssd1306b_WriteCommand(0xD5); ssd1306b_WriteCommand(0xF0);  /* clkdiv */
    ssd1306b_WriteCommand(0xD9); ssd1306b_WriteCommand(0x22);  /* pchg   */
    ssd1306b_WriteCommand(0xDA); ssd1306b_WriteCommand(0x12);  /* COM hw */
    ssd1306b_WriteCommand(0xDB); ssd1306b_WriteCommand(0x20);  /* VCOMH  */
    ssd1306b_WriteCommand(0x8D); ssd1306b_WriteCommand(0x14);  /* charge pump on */
    ssd1306b_WriteCommand(0xAF);          /* display ON                 */

    ssd1306b_Fill(Black);
    ssd1306b_UpdateScreen();

    SSD1306b.CurrentX    = 0;
    SSD1306b.CurrentY    = 0;
    SSD1306b.Initialized = 1;
    SSD1306b.DisplayOn   = 1;
}

/* ── Buffer-only operations ──────────────────────────────────────── */
void ssd1306b_Fill(SSD1306_COLOR color)
{
    memset(SSD1306b_Buffer,
           (color == Black) ? 0x00 : 0xFF,
           sizeof(SSD1306b_Buffer));
}

void ssd1306b_DrawPixel(uint8_t x, uint8_t y, SSD1306_COLOR color)
{
    if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) return;

    if (color == White) {
        SSD1306b_Buffer[x + (y / 8) * SSD1306_WIDTH] |=  (1U << (y % 8));
    } else {
        SSD1306b_Buffer[x + (y / 8) * SSD1306_WIDTH] &= ~(1U << (y % 8));
    }
}

void ssd1306b_SetCursor(uint8_t x, uint8_t y)
{
    SSD1306b.CurrentX = x;
    SSD1306b.CurrentY = y;
}

void ssd1306b_UpdateScreen(void)
{
    if (!SSD1306b_Present) return;
    for (uint8_t i = 0; i < SSD1306_HEIGHT / 8; i++) {
        ssd1306b_WriteCommand(0xB0 + i);
        ssd1306b_WriteCommand(0x00);
        ssd1306b_WriteCommand(0x10);
        ssd1306b_WriteData(&SSD1306b_Buffer[SSD1306_WIDTH * i],
                           SSD1306_WIDTH);
    }
}

/* ── Text ────────────────────────────────────────────────────────── */
char ssd1306b_WriteChar(char ch, SSD1306_Font_t Font, SSD1306_COLOR color)
{
    if (ch < 32 || ch > 126) return 0;

    if (SSD1306_WIDTH  < (SSD1306b.CurrentX + Font.width)  ||
        SSD1306_HEIGHT < (SSD1306b.CurrentY + Font.height)) {
        return 0;
    }

    for (uint32_t i = 0; i < Font.height; i++) {
        uint16_t b = Font.data[(ch - 32) * Font.height + i];
        for (uint32_t j = 0; j < Font.width; j++) {
            if ((b << j) & 0x8000) {
                ssd1306b_DrawPixel(SSD1306b.CurrentX + j,
                                   SSD1306b.CurrentY + i, color);
            } else {
                ssd1306b_DrawPixel(SSD1306b.CurrentX + j,
                                   SSD1306b.CurrentY + i,
                                   (SSD1306_COLOR)!color);
            }
        }
    }

    SSD1306b.CurrentX += (Font.char_width != 0)
                            ? Font.char_width[ch - 32]
                            : Font.width;
    return ch;
}

char ssd1306b_WriteString(char *str, SSD1306_Font_t Font, SSD1306_COLOR color)
{
    while (*str) {
        if (ssd1306b_WriteChar(*str, Font, color) != *str) {
            return *str;
        }
        str++;
    }
    return *str;
}
