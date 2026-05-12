#ifndef __DHT22_H
#define __DHT22_H

#include "stm32f10x.h"
#include "main.h"

void DHT22_Init(void);
DHT22_Data DHT22_Read(void);

#endif
