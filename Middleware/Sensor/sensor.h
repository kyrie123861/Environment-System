#ifndef __SENSOR_H
#define __SENSOR_H

#include "stm32f10x.h"

void Sensor_Init(void);
void Sensor_Collect(void);
float GetTemperature(void);
float GetHumidity(void);
float GetLight(void);
int GetPM25(void);

#endif
