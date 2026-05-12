#ifndef __CONTROL_H
#define __CONTROL_H

#include "stm32f10x.h"
#include "main.h"

void Control_Init(void);
void ControlDevice(DeviceType type, float value);
void ControlAll(float temperature, float humidity, float light);
uint8_t Control_GetStatus(void);

extern volatile uint8_t g_deviceStatus;
extern volatile SystemMode g_systemMode;
extern volatile uint32_t g_sysTick;

#endif
