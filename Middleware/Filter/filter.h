#ifndef __FILTER_H
#define __FILTER_H

#include "stm32f10x.h"
#include "main.h"

float FilterTemperature(float val);
float FilterHumidity(float val);
float FilterLight(float val);
int FilterPM25(int val);
void Filter_Reset(void);

#endif
