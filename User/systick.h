// systick.h - SysTick 系统时基模块
// 
// SysTick是Cortex-M3内核自带的24位递减定时器。
// 配置为1ms中断, 提供系统毫秒计数和采集分频。

#ifndef __SYSTICK_H
#define __SYSTICK_H

#include "stm32f10x.h"

void SysTick_Init(void);

// 全局毫秒计数器: SysTick中断每1ms+1, delay/display/control模块使用
extern volatile uint32_t g_sysTick;

// 采集标志: ISR中每1s置1, main.c中清0
extern volatile uint8_t g_sample_flag;

#endif
