// control.c - 继电器控制 + 按键模式切换
//
// 继电器: PB0(风扇)/PB1(加湿器)/PB10(补光灯) 低电平触发 (输出0吸合, 输出1断开)
// 按键:   PA0 EXTI0 上升沿中断, 软件消抖200ms, 切换自动/手动模式
//
// 滞回控制: 设置上下两个阈值, 中间为死区, 防止传感器在阈值附近抖动导致反复开关。
// 例: 温度 >30°C开风扇, <26°C关风扇, 26~30°C之间保持原状态不变。

#include "control.h"
#include "delay.h"

// 三个继电器状态: 1=吸合(低电平), 0=断开(高电平)
static uint8_t g_relay_status[3] = {0, 0, 0};

void Control_Init(void)
{
	// 1. 开启时钟
	RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;  // GPIOB: 继电器
	RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;  // GPIOA: 按键
	RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;  // AFIO: EXTI配置需要

	// 2. PB0/PB1/PB10 推挽输出 50MHz (推挽输出驱动能力强, 可靠驱动光耦)
	GPIOB->CRL &= ~(GPIO_CRL_CNF0 | GPIO_CRL_MODE0);  // PB0
	GPIOB->CRL |= GPIO_CRL_MODE0_0 | GPIO_CRL_MODE0_1;
	GPIOB->CRL &= ~(GPIO_CRL_CNF1 | GPIO_CRL_MODE1);  // PB1
	GPIOB->CRL |= GPIO_CRL_MODE1_0 | GPIO_CRL_MODE1_1;
	GPIOB->CRH &= ~(GPIO_CRH_CNF10 | GPIO_CRH_MODE10);  // PB10
	GPIOB->CRH |= GPIO_CRH_MODE10_0 | GPIO_CRH_MODE10_1;

	// 3. 初始输出高电平, 继电器全断开 (安全状态)
	GPIOB->ODR |= (GPIO_ODR_ODR0 | GPIO_ODR_ODR1 | GPIO_ODR_ODR10);

	// 4. PA0 浮空输入 (CNF0_0=浮空, MODE0=输入)
	GPIOA->CRL &= ~(GPIO_CRL_CNF0 | GPIO_CRL_MODE0);
	GPIOA->CRL |= GPIO_CRL_CNF0_0;

	// 5. EXTI0中断: 映射到PA0, 上升沿触发 (按键松开: 低->高)
	AFIO->EXTICR[0] &= ~AFIO_EXTICR1_EXTI0;  // EXTI0 -> PA0
	EXTI->IMR  |= EXTI_IMR_MR0;  // 使能EXTI0中断
	EXTI->RTSR |= EXTI_RTSR_TR0;  // 上升沿触发

	// 6. NVIC: 抢占优先级=2 (低于UART的1, 按键不需要严格的实时性)
	NVIC_SetPriorityGrouping(3);
	NVIC_SetPriority(EXTI0_IRQn, 2);
	NVIC_EnableIRQ(EXTI0_IRQn);
}

// 继电器吸合: 输出低电平
static void Relay_On(uint16_t pin)
{
	GPIOB->ODR &= ~pin;
}

// 继电器断开: 输出高电平
static void Relay_Off(uint16_t pin)
{
	GPIOB->ODR |= pin;
}

// 滞回控制单个设备
// 只有当前状态与目标状态不同时才操作, 避免重复写ODR
void ControlDevice(DeviceType type, float value)
{
	uint16_t pin;
	uint8_t *status;

	// 非自动模式: 不执行自动控制
	if (g_systemMode != MODE_AUTO)
		return;

	switch (type)
	{
	case DEVICE_FAN:       pin = RELAY_FAN_PIN;       break;
	case DEVICE_HUMIDIFIER:pin = RELAY_HUMIDIFIER_PIN; break;
	case DEVICE_LIGHT:     pin = RELAY_LIGHT_PIN;     break;
	default: return;
	}
	status = &g_relay_status[type];

	switch (type)
	{
	case DEVICE_FAN:
		// 温度滞回: >30°C开, <26°C关, 4°C死区
		if (value > TEMP_HIGH_THRESHOLD && *status == 0)
			{ Relay_On(pin); *status = 1; }
		else if (value < TEMP_LOW_THRESHOLD && *status == 1)
			{ Relay_Off(pin); *status = 0; }
		break;

	case DEVICE_HUMIDIFIER:
		// 湿度滞回: <40%开, >60%关, 20%死区
		if (value < HUMI_LOW_THRESHOLD && *status == 0)
			{ Relay_On(pin); *status = 1; }
		else if (value > HUMI_HIGH_THRESHOLD && *status == 1)
			{ Relay_Off(pin); *status = 0; }
		break;

	case DEVICE_LIGHT:
		// 光照滞回: <100Lux开, >500Lux关, 400Lux死区
		// 死区设得大是因为光照受外界影响波动大
		if (value < LIGHT_LOW_THRESHOLD && *status == 0)
			{ Relay_On(pin); *status = 1; }
		else if (value > LIGHT_HIGH_THRESHOLD && *status == 1)
			{ Relay_Off(pin); *status = 0; }
		break;
	}
}

// 一次控制全部三个设备
void ControlAll(float temperature, float humidity, float light)
{
	ControlDevice(DEVICE_FAN, temperature);
	ControlDevice(DEVICE_HUMIDIFIER, humidity);
	ControlDevice(DEVICE_LIGHT, light);
}

// 返回三个继电器状态位掩码: bit0=风扇, bit1=加湿器, bit2=补光灯
uint8_t Control_GetStatus(void)
{
	return (g_relay_status[0] << 0)
	     | (g_relay_status[1] << 1)
	     | (g_relay_status[2] << 2);
}

// EXTI0中断: PA0按键切换自动/手动模式, 200ms软件消抖
void EXTI0_IRQHandler(void)
{
	static uint32_t last_tick = 0;

	if (EXTI->PR & EXTI_PR_PR0)  // 检查EXTI0挂起标志
	{
		// 软件消抖: 200ms内的重复触发视为抖动, 忽略
		if ((g_sysTick - last_tick) > 200)
		{
			last_tick = g_sysTick;

			// 切换模式: AUTO <-> MANUAL
			if (g_systemMode == MODE_AUTO)
				g_systemMode = MODE_MANUAL;
			else
				g_systemMode = MODE_AUTO;
		}

		EXTI->PR |= EXTI_PR_PR0;  // 写1清零挂起标志
	}
}
