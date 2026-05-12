// systick.c - SysTick 1ms时基 + 采集分频
// 
// LOAD = 72MHz/1000 - 1 = 71999, 每72000周期(1ms)中断一次。
// 中断中递增g_sysTick, 每1000ms置g_sample_flag通知主循环采集。

#include "systick.h"
#include "main.h"  // SAMPLE_INTERVAL_MS

volatile uint32_t g_sysTick        = 0;  // 系统毫秒计数
volatile uint8_t  g_sample_flag    = 0;  // 采集触发标志
static volatile uint16_t g_sample_counter = 0;  // 分频计数器

void SysTick_Init(void)
{
	SysTick->LOAD = 72000 - 1;  // 72MHz / 1000 = 72000周期 = 1ms
	SysTick->VAL  = 0;  // 清当前值
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE  // 时钟源=HCLK(72MHz)
	              | SysTick_CTRL_TICKINT// 递减到0产生中断
	              | SysTick_CTRL_ENABLE;// 启动计数器
}

void SysTick_Handler(void)
{
	g_sysTick++;  // 毫秒计数+1

	g_sample_counter++;
	if (g_sample_counter >= SAMPLE_INTERVAL_MS)
	{
		g_sample_counter = 0;
		g_sample_flag = 1;  // 通知主循环: 该采集了
	}
}
