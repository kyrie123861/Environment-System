// i2c.c - 软件模拟I2C (PB6=SCL, PB7=SDA)
//
// 为什么不用硬件I2C: STM32F1的硬件I2C有已知缺陷(锁死/超时), 软件模拟更稳定。
//
// 开漏输出原理: 写ODR=0 -> NMOS导通 -> 拉低; 写ODR=1 -> NMOS截止 -> 外部上拉拉高。
// 用BSRR寄存器实现原子IO操作: 低16位写1置位ODR(高电平), 高16位写1复位ODR(低电平)。
//
// I2C时序要点:
// 起始: SCL高时SDA从高变低
// 停止: SCL高时SDA从低变高
// 数据: SCL低时SDA可变化, SCL高时SDA必须稳定
// 应答: 第9个SCL脉冲, 接收方拉低SDA = ACK, 不拉低 = NACK
// 顺序: 先发MSB(bit7), 最后LSB(bit0)

#include "i2c.h"
#include "main.h"
#include "delay.h"

// 用BSRR操作GPIO: 低16位置位(高), 高16位复位(低) -> 原子操作, 无需读-改-写
#define SCL_H()  (GPIOB->BSRR = (1 << I2C_SCL_PIN))
#define SCL_L()  (GPIOB->BSRR = (1 << (I2C_SCL_PIN + 16)))
#define SDA_H()  (GPIOB->BSRR = (1 << I2C_SDA_PIN))
#define SDA_L()  (GPIOB->BSRR = (1 << (I2C_SDA_PIN + 16)))
#define SDA_IN() ((GPIOB->IDR >> I2C_SDA_PIN) & 1)

void I2C_Init(void)
{
	// 开启GPIOB时钟
	RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;

	// PB6(SCL): 开漏输出50MHz (CNF6_0=开漏, MODE6=50MHz)
	GPIOB->CRL &= ~(GPIO_CRL_CNF6 | GPIO_CRL_MODE6);
	GPIOB->CRL |= GPIO_CRL_CNF6_0 | GPIO_CRL_MODE6_0 | GPIO_CRL_MODE6_1;

	// PB7(SDA): 同上
	GPIOB->CRL &= ~(GPIO_CRL_CNF7 | GPIO_CRL_MODE7);
	GPIOB->CRL |= GPIO_CRL_CNF7_0 | GPIO_CRL_MODE7_0 | GPIO_CRL_MODE7_1;

	// 初始释放总线(高电平), 等待稳定
	SCL_H();
	SDA_H();
	Delay_us(10);
}

// 起始信号: SCL高时SDA下降
void I2C_Start(void)
{
	SDA_H();
	SCL_H();
	Delay_us(5);
	SDA_L();  // SDA下降 -> 起始条件
	Delay_us(5);
	SCL_L();  // 拉低SCL, 准备发数据
	Delay_us(5);
}

// 停止信号: SCL高时SDA上升
void I2C_Stop(void)
{
	SCL_L();
	SDA_L();
	Delay_us(5);
	SCL_H();
	Delay_us(5);
	SDA_H();  // SDA上升 -> 停止条件
	Delay_us(5);
}

// 等待从机应答: 主机释放SDA, 发第9个SCL脉冲, 读SDA电平
// 返回: 0=ACK(从机拉低), 1=NACK(从机未响应)
uint8_t I2C_WaitAck(void)
{
	uint8_t ack;

	SDA_H();  // 释放SDA, 由从机控制
	Delay_us(5);
	SCL_H();  // 第9个SCL脉冲
	Delay_us(5);
	ack = SDA_IN();  // 读SDA: 0=从机拉低(ACK), 1=未拉低(NACK)
	SCL_L();
	Delay_us(5);

	return ack;
}

// 主机发送应答: ack=0发ACK(继续), ack=1发NACK(停止)
void I2C_SendAck(uint8_t ack)
{
	if (ack)
		SDA_H();  // NACK: 释放SDA为高
	else
		SDA_L();  // ACK: 拉低SDA
	Delay_us(5);
	SCL_H();
	Delay_us(5);
	SCL_L();
	Delay_us(5);
	SDA_H();  // 释放SDA
}

// 写一个字节: 从MSB到LSB依次发送, 第9个脉冲收应答
// 数据在SCL低时变化, SCL高时被从机采样
uint8_t I2C_WriteByte(uint8_t data)
{
	uint8_t i;

	for (i = 0; i < 8; i++)
	{
		if (data & 0x80)  // 取最高位
			SDA_H();
		else
			SDA_L();

		Delay_us(5);
		SCL_H();  // 上升沿: 从机锁存数据
		Delay_us(5);
		SCL_L();  // 下降沿: 准备下一位
		Delay_us(5);

		data <<= 1;  // 左移, 下一位到bit7
	}

	return I2C_WaitAck();  // 第9个脉冲: 接收应答
}

// 读一个字节: 释放SDA由从机控制, 依次读8位(MSB先)
// 读完必须由调用者发I2C_SendAck(最后一个字节发NACK)
uint8_t I2C_ReadByte(void)
{
	uint8_t i, data = 0;

	SDA_H();  // 释放SDA

	for (i = 0; i < 8; i++)
	{
		data <<= 1;  // 先左移, 腾出LSB
		SCL_H();  // 上升沿: 从机输出数据
		Delay_us(5);
		if (SDA_IN())  // 读SDA: 高=1, 低=0
			data |= 1;
		SCL_L();
		Delay_us(5);
	}

	return data;
}
