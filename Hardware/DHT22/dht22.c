// dht22.c - DHT22数字温湿度传感器 (单线协议, PA1)
//
// 通信流程 (~5ms):
// ① MCU拉低1.2ms, 释放30us (起始信号)
// ② DHT22拉低80us再拉高80us (响应)
// ③ DHT22发送40位数据 (5字节: 湿整+湿小+温整+温小+校验)
//
// 位判断: 每bit从50us低电平开始, 之后高电平长度区分0/1
// bit0: 高26~28us  -> 延时40us后读到低
// bit1: 高~70us    -> 延时40us后读到高
// 策略: 等低电平结束 -> 延时40us -> 读电平
//
// 整个通信过程关全局中断, 防止时序被干扰。
// 调用间隔需>=2秒 (传感器的物理限制)。

#include "dht22.h"
#include "delay.h"

// PA1位操作: BSRR低16位写1置高, 高16位写1置低, IDR读取
#define DHT22_H()  (GPIOA->BSRR = (1 << DHT22_PIN))
#define DHT22_L()  (GPIOA->BSRR = (1 << (DHT22_PIN + 16)))
#define DHT22_IN() ((GPIOA->IDR >> DHT22_PIN) & 1)

void DHT22_Init(void)
{
	// PA1: 开漏输出50MHz (CNF1_0=开漏, MODE1=50MHz)
	// 开漏输出: 写0拉低, 写1靠外部4.7kΩ上拉到高, 适合单线双向通信
	GPIOA->CRL &= ~(GPIO_CRL_CNF1 | GPIO_CRL_MODE1);
	GPIOA->CRL |= GPIO_CRL_CNF1_0 | GPIO_CRL_MODE1_0 | GPIO_CRL_MODE1_1;

	DHT22_H();  // 释放总线

	Delay_ms(1000);  // 等待传感器上电稳定 (手册要求1~2s)
}

DHT22_Data DHT22_Read(void)
{
	DHT22_Data result = {0.0f, 0.0f, 0};  // 默认valid=0 (无效)
	uint8_t data[5] = {0};
	uint8_t i, j;
	uint16_t timeout;

	// === 阶段1: MCU发起始信号 ===
	// 拉低1.2ms(>1ms要求) + 释放30us(20~40us)
	__disable_irq();  // ★ 关全局中断, 保护时序 (~5ms)

	DHT22_L();
	Delay_us(1200);  // >1ms低电平
	DHT22_H();
	Delay_us(30);  // 20~40us释放

	// === 阶段2: 等待DHT22响应 (拉低80us -> 拉高80us) ===

	// 2a. 等DHT22拉低
	timeout = 1000;
	while (DHT22_IN() && --timeout) {}
	if (timeout == 0) { __enable_irq(); return result; }  // 超时: 返回无效

	// 2b. 等低电平结束 (~80us)
	timeout = 1000;
	while (!DHT22_IN() && --timeout) {}
	if (timeout == 0) { __enable_irq(); return result; }

	// 2c. 等高电平结束 (~80us, 准备发数据)
	timeout = 1000;
	while (DHT22_IN() && --timeout) {}
	if (timeout == 0) { __enable_irq(); return result; }

	// === 阶段3: 接收40位数据 (5字节×8位) ===
	//每一位: 50us低 + 26~70us高
	// 等低电平结束 -> 延时40us -> 采样: 高=bit1, 低=bit0
	for (j = 0; j < 5; j++)
	{
		for (i = 0; i < 8; i++)
		{
			// 等50us低电平结束
			timeout = 500;
			while (!DHT22_IN() && --timeout) {}
			if (timeout == 0) { __enable_irq(); return result; }

			// 延时40us后采样:
			//bit0: 高仅26~28us, 40us时已回落 -> 读到0
			// bit1: 高持续70us,   40us时仍为高 -> 读到1
			Delay_us(40);

			data[j] <<= 1;  // 左移腾出LSB
			if (DHT22_IN())
				data[j] |= 1;

			// 等当前bit剩余高电平结束
			timeout = 500;
			while (DHT22_IN() && --timeout) {}
		}
	}

	__enable_irq();  // ★ 恢复中断

	// === 阶段4: 校验 ===
	//data[0]=湿整, data[1]=湿小, data[2]=温整, data[3]=温小
	// data[4] = (data[0]+[1]+[2]+[3])的低8位
	if ((uint8_t)(data[0] + data[1] + data[2] + data[3]) == data[4])
	{
		result.valid = 1;

		// 湿度: 16位值 × 0.1 = %RH
		// 温度: bit7=1表示负温(>0°C时此位为0)
		result.humidity = (float)((data[0] << 8) | data[1]) * 0.1f;

		if (data[2] & 0x80)  // 负温度 (bit7=1)
		{
			result.temperature = -(float)(((data[2] & 0x7F) << 8) | data[3]) * 0.1f;
		}
		else  // 正温度
		{
			result.temperature = (float)((data[2] << 8) | data[3]) * 0.1f;
		}
	}

	return result;
}
