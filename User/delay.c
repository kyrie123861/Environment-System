// delay.c - 微秒/毫秒/秒 延时函数 (基于SysTick)
//
// SysTick是Cortex-M3内核自带的24位递减定时器。
// LOAD: 重载值, VAL: 当前值, CTRL: 控制寄存器(bit2=时钟源, bit1=中断, bit0=使能)
//
// 72MHz下1us需要72个周期, LOAD = us * 72。
// 24位计数器最大约233ms, 更长时间通过多次调用实现。

#include "delay.h"

// Delay_us - 微秒延时 (最大~233ms)
//
// 占用SysTick外设做轮询延时, 不产生中断。
// 保存并恢复SysTick原有配置, 避免打断系统1ms时基中断。
// 若SysTick_Init尚未调用(初始化阶段), saved_ctrl=0, 恢复后亦为0, 无副作用。
void Delay_us(uint16_t us)
{
	uint32_t saved_ctrl = SysTick->CTRL;  // 保存当前配置(含TICKINT使能位)
	uint32_t saved_load = SysTick->LOAD;  // 保存1ms重载值

// LOAD = 72 * us (72MHz, 每us需72个周期)
	SysTick->LOAD = 72 * us;
	SysTick->VAL  = 0;  // 清空当前值, 确保从0开始计数

// CTRL = 0x05: bit2(内核时钟) | bit0(使能), 不使能中断(用轮询)
	SysTick->CTRL = 0x05;

// 等待COUNTFLAG置位(计数器递减到0时硬件置1)
	while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG) == 0)
	{}

// 恢复SysTick到延时前状态: 关->恢复LOAD->重置VAL->恢复CTRL(含TICKINT)
	SysTick->CTRL = 0;  // 关闭计数器
	SysTick->LOAD = saved_load;  // 恢复1ms重载值
	SysTick->VAL  = 0;  // 重置当前值, 避免单次1ms周期时序错位
	SysTick->CTRL = saved_ctrl;  // 恢复原配置(若之前TICKINT使能则恢复中断)
}

// 通过1000次Delay_us(1000)实现, 不能直接Delay_us(ms*1000)因为会超过24位
void Delay_ms(uint16_t ms)
{
	while (ms--)
	{
		Delay_us(1000);
	}
}

void Delay_s(uint16_t s)
{
	while (s--)
	{
		Delay_ms(1000);
	}
}
