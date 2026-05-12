// alarm.c - 蜂鸣器报警 (TIM2_CH2 PWM)
//
// PB3 默认是 JTDO 调试引脚, 需要关闭 JTAG 保留 SWD 才能用作 PWM 输出。
// PWM 频率 = 72MHz / (PSC+1) / (ARR+1) = 72MHz / 72 / 500 = 2kHz

#include "alarm.h"

void Alarm_Init(void)
{
	// 1. 开启时钟: GPIOB, AFIO(重映射), TIM2(APB1)
	RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
	RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

	// 2. 关闭JTAG保留SWD, 释放PB3为GPIO
	// SWJ_CFG = JTAGDISABLE: 仅SWD使能
	AFIO->MAPR &= ~AFIO_MAPR_SWJ_CFG;
	AFIO->MAPR |= AFIO_MAPR_SWJ_CFG_JTAGDISABLE;

	// 设置TIM2重映射: 默认CH2在PA1, PartialRemap1后CH2在PB3
	// 不设置此位则PB3处于无外设连接的复用输出态, 输出不定导致蜂鸣器常响
	AFIO->MAPR |= AFIO_MAPR_TIM2_REMAP_PARTIALREMAP1;

	// 3. PB3 复用推挽输出 50MHz
	// CNF3_1 + MODE3 = 复用推挽, 50MHz
	GPIOB->CRL &= ~(GPIO_CRL_CNF3 | GPIO_CRL_MODE3);
	GPIOB->CRL |= GPIO_CRL_CNF3_1 | GPIO_CRL_MODE3_0 | GPIO_CRL_MODE3_1;

	// 4. TIM2 时基: PSC=71, ARR=499 -> 2kHz
	// 72MHz / (71+1) = 1MHz计数频率, 1MHz / (499+1) = 2kHz
	TIM2->PSC = 71;
	TIM2->ARR = 499;

	// 5. TIM2_CH2 PWM模式1, 预装载使能
	TIM2->CCMR1 &= ~(TIM_CCMR1_CC2S | TIM_CCMR1_OC2FE | TIM_CCMR1_OC2PE | TIM_CCMR1_OC2M);
	TIM2->CCMR1 |= TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2;  // PWM模式1: CNT<CCR时有效
	TIM2->CCMR1 |= TIM_CCMR1_OC2PE;  // 预装载: 更新事件时才加载CCR, 避免毛刺

	// 6. 使能CH2输出, 极性反转(CC2P=1: 低电平有效)
	// 蜂鸣器模块低电平触发, 不加反转会导致CCR=0时输出低电平误响
	TIM2->CCER |= TIM_CCER_CC2E;  // CH2输出使能
	TIM2->CCER |= TIM_CCER_CC2P;  // 极性反转, 高电平=静音

	// 7. 初始占空比0% -> 输出高电平 (蜂鸣器静音)
	TIM2->CCR2 = 0;

	// 8. 使能自动重载预装载, 产生初始更新事件加载影子寄存器, 启动计数
	TIM2->CR1 |= TIM_CR1_ARPE;  // 自动重载预装载使能
	TIM2->EGR |= TIM_EGR_UG;  // 手动产生更新事件
	TIM2->CR1 |= TIM_CR1_CEN;  // 启动计数器
}

void Alarm_On(void)
{
	TIM2->CCR2 = 250;  // 50%占空比 = 250/500
}

void Alarm_Off(void)
{
	TIM2->CCR2 = 0;  // 0%占空比 -> 高电平 (静音)
}
