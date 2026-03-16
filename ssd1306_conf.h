/**
 * ssd1306_conf.h
 * Configuration file for the SSD1306 OLED library.
 * Place this file in your Core/Inc directory alongside ssd1306.h
 */

#ifndef __SSD1306_CONF_H__
#define __SSD1306_CONF_H__

// -------------------------------------------------------
// FIX: ssd1306.h checks for "STM32F3" but CubeIDE only
// defines "STM32F303xE". We define STM32F3 here so the
// correct HAL header gets included inside ssd1306.h.
// -------------------------------------------------------
#ifndef STM32F3
#define STM32F3
#endif

// -------------------------------------------------------
// Also include the HAL directly to guarantee I2C types
// and HAL_MAX_DELAY are visible before ssd1306.h uses them
// -------------------------------------------------------
#include "stm32f3xx_hal.h"

// -------------------------------------------------------
// Communication interface: I2C
// -------------------------------------------------------
#define SSD1306_USE_I2C

// -------------------------------------------------------
// I2C handle — must match the handle declared in main.c
// -------------------------------------------------------
#define SSD1306_I2C_PORT    hi2c1
#define SSD1306_I2C_ADDR    (0x3C << 1)   // 0x78 — change to (0x3D << 1) if display doesn't respond

// -------------------------------------------------------
// Display dimensions
// -------------------------------------------------------
#define SSD1306_WIDTH       128
#define SSD1306_HEIGHT      64

// -------------------------------------------------------
// Fonts to include
// -------------------------------------------------------
#define SSD1306_INCLUDE_FONT_6x8
#define SSD1306_INCLUDE_FONT_7x10
#define SSD1306_INCLUDE_FONT_11x18
#define SSD1306_INCLUDE_FONT_16x26

#endif /* __SSD1306_CONF_H__ */
