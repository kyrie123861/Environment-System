#ifndef __PM25_H
#define __PM25_H

#include "stm32f10x.h"
#include "main.h"

void PM25_Init(void);
PM25_Data PM25_Read(void);

#endif
