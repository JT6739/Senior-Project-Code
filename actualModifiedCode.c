// you can just copy and paste this part into main with out download the actual main.c file
#include "main.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include <stdio.h>

while (1)
  {
    /* USER CODE END WHILE */
	  float voltage = 12.3;
	      float current = 1.2;

	      char buf[30];

	      ssd1306_Fill(Black);

	      sprintf(buf,"V: %.2f V", voltage); // @suppress("Float formatting support")
	      ssd1306_SetCursor(0,10);
	      ssd1306_WriteString(buf, Font_11x18, White);

	      sprintf(buf,"I: %.2f A", current); // @suppress("Float formatting support")
	      ssd1306_SetCursor(0,40);
	      ssd1306_WriteString(buf, Font_11x18, White);

	      ssd1306_UpdateScreen();

	      HAL_Delay(500);
  }
