// i2c2.c - 第二路软件模拟I2C (PA5=SCL, PA6=SDA)
//
// 专用于BH1750光照传感器, 与OLED(使用i2c.c的PB6/PB7)物理隔离。
// 面包板环境下无法用杜邦线将两个I2C设备并联到同一组MCU引脚,
// 因此为BH1750单独分配一组GPIO引脚实现独立I2C总线。
//
// I2C时序要点:
// 起始: SCL高时SDA从高变低
// 停止: SCL高时SDA从低变高
// 数据: SCL低时SDA可变化, SCL高时SDA必须稳定
// 应答: 第9个SCL脉冲, 接收方拉低SDA = ACK, 不拉低 = NACK
// 顺序: 先发MSB(bit7), 最后LSB(bit0)

#include "i2c2.h"
#include "main.h"
#include "delay.h"

// 用BSRR操作GPIO: 低16位置位(高), 高16位复位(低) -> 原子操作, 无需读-改-写
#define SCL2_H()  (GPIOA->BSRR = (1 << I2C2_SCL_PIN))
#define SCL2_L()  (GPIOA->BSRR = (1 << (I2C2_SCL_PIN + 16)))
#define SDA2_H()  (GPIOA->BSRR = (1 << I2C2_SDA_PIN))
#define SDA2_L()  (GPIOA->BSRR = (1 << (I2C2_SDA_PIN + 16)))
#define SDA2_IN() ((GPIOA->IDR >> I2C2_SDA_PIN) & 1)

void I2C2_Init(void)
{
	// 开启GPIOA时钟
	RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

	// PA5(SCL): 开漏输出50MHz (CNF5_0=开漏, MODE5=50MHz)
	GPIOA->CRL &= ~(GPIO_CRL_CNF5 | GPIO_CRL_MODE5);
	GPIOA->CRL |= GPIO_CRL_CNF5_0 | GPIO_CRL_MODE5_0 | GPIO_CRL_MODE5_1;

	// PA6(SDA): 同上
	GPIOA->CRL &= ~(GPIO_CRL_CNF6 | GPIO_CRL_MODE6);
	GPIOA->CRL |= GPIO_CRL_CNF6_0 | GPIO_CRL_MODE6_0 | GPIO_CRL_MODE6_1;

	// 初始释放总线(高电平), 等待稳定
	SCL2_H();
	SDA2_H();
	Delay_us(10);
}

// 起始信号: SCL高时SDA下降
void I2C2_Start(void)
{
	SDA2_H();
	SCL2_H();
	Delay_us(5);
	SDA2_L();  // SDA下降 -> 起始条件
	Delay_us(5);
	SCL2_L();  // 拉低SCL, 准备发数据
	Delay_us(5);
}

// 停止信号: SCL高时SDA上升
void I2C2_Stop(void)
{
	SCL2_L();
	SDA2_L();
	Delay_us(5);
	SCL2_H();
	Delay_us(5);
	SDA2_H();  // SDA上升 -> 停止条件
	Delay_us(5);
}

// 等待从机应答: 主机释放SDA, 发第9个SCL脉冲, 读SDA电平
// 返回: 0=ACK(从机拉低), 1=NACK(从机未响应)
uint8_t I2C2_WaitAck(void)
{
	uint8_t ack;

	SDA2_H();  // 释放SDA, 由从机控制
	Delay_us(5);
	SCL2_H();  // 第9个SCL脉冲
	Delay_us(5);
	ack = SDA2_IN();  // 读SDA: 0=从机拉低(ACK), 1=未拉低(NACK)
	SCL2_L();
	Delay_us(5);

	return ack;
}

// 主机发送应答: ack=0发ACK(继续), ack=1发NACK(停止)
void I2C2_SendAck(uint8_t ack)
{
	if (ack)
		SDA2_H();  // NACK: 释放SDA为高
	else
		SDA2_L();  // ACK: 拉低SDA
	Delay_us(5);
	SCL2_H();
	Delay_us(5);
	SCL2_L();
	Delay_us(5);
	SDA2_H();  // 释放SDA
}

// 写一个字节: 从MSB到LSB依次发送, 第9个脉冲收应答
uint8_t I2C2_WriteByte(uint8_t data)
{
	uint8_t i;

	for (i = 0; i < 8; i++)
	{
		if (data & 0x80)  // 取最高位
			SDA2_H();
		else
			SDA2_L();

		Delay_us(5);
		SCL2_H();  // 上升沿: 从机锁存数据
		Delay_us(5);
		SCL2_L();  // 下降沿: 准备下一位
		Delay_us(5);

		data <<= 1;  // 左移, 下一位到bit7
	}

	return I2C2_WaitAck();  // 第9个脉冲: 接收应答
}

// 读一个字节: 释放SDA由从机控制, 依次读8位(MSB先)
uint8_t I2C2_ReadByte(void)
{
	uint8_t i, data = 0;

	SDA2_H();  // 释放SDA

	for (i = 0; i < 8; i++)
	{
		data <<= 1;  // 先左移, 腾出LSB
		SCL2_H();  // 上升沿: 从机输出数据
		Delay_us(5);
		if (SDA2_IN())  // 读SDA: 高=1, 低=0
			data |= 1;
		SCL2_L();
		Delay_us(5);
	}

	return data;
}
