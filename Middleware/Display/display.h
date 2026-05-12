#ifndef __DISPLAY_H
#define __DISPLAY_H

#include "stm32f10x.h"

void Display_Init(void);
void DisplayAll(float temperature, float humidity, float light,
                int pm25, uint8_t deviceStatus);

#endif
