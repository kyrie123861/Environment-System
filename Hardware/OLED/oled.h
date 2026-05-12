#ifndef __OLED_H
#define __OLED_H

#include "stm32f10x.h"

void OLED_Init(void);
void OLED_Clear(void);
void OLED_ClearLine(uint8_t page, uint8_t col_start);
void OLED_SetPos(uint8_t page, uint8_t col);
void OLED_ShowChar(uint8_t page, uint8_t col, char ch);
void OLED_ShowString(uint8_t page, uint8_t col, const char *str);
void OLED_ShowInt(uint8_t page, uint8_t col, int num);
void OLED_ShowFloat(uint8_t page, uint8_t col, float num);

#endif
