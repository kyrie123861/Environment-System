// bh1750.c - BH1750FVI 数字光照传感器 (独立I2C: PA5=SCL, PA6=SDA)
//
// 7位地址0x23, 写=0x46, 读=0x47
// 使用第二路I2C总线(i2c2), 与OLED的I2C总线(PB6/PB7)物理隔离。
// 面包板环境下无法用杜邦线将两个I2C设备并联到同一组MCU引脚。
//
// 命令:
// 0x01: 上电      0x10: 连续高分辨率 (1Lux, ~120ms)
// 0x07: 复位      0x11: 连续高分辨率2 (0.5Lux)
// 0x00: 断电      0x13: 连续低分辨率 (4Lux, ~16ms)
// 0x20: 单次高分辨率
//
// 本系统用0x10: 连续高分辨率模式, 1Lux精度满足室内监测需求。
// Lux = 原始值 / 1.2 (传感器内部ADC的1.2倍系数, 手册规定)

#include "bh1750.h"
#include "i2c2.h"
#include "delay.h"

#define BH1750_WRITE  0x46  // 写地址: 7位0x23 << 1
#define BH1750_READ   0x47  // 读地址: 写地址 | 1
#define CMD_POWER_ON  0x01  // 上电命令
#define CMD_CONT_H    0x10  // 连续高分辨率模式

void BH1750_Init(void)
{
	// 步骤1: 上电
	I2C2_Start();
	I2C2_WriteByte(BH1750_WRITE);
	I2C2_WriteByte(CMD_POWER_ON);
	I2C2_Stop();

	Delay_ms(10);  // 等待电路稳定

	// 步骤2: 设为连续高分辨率模式
	I2C2_Start();
	I2C2_WriteByte(BH1750_WRITE);
	I2C2_WriteByte(CMD_CONT_H);
	I2C2_Stop();

	// 步骤3: 首次测量需~120ms, 等180ms确保完成
	Delay_ms(180);
}

// BH1750_Read - 读取光照强度
// @return: Lux值, 通信失败返回-1.0
float BH1750_Read(void)
{
	uint16_t raw;
	uint8_t h, l;

	// 发读地址, 同时检查ACK; 返回非0=NACK(从机未应答)
	I2C2_Start();
	if (I2C2_WriteByte(BH1750_READ))
	{
		I2C2_Stop();
		return -1.0f;  // 通信失败
	}

	// 读高字节, 发ACK(继续)
	h = I2C2_ReadByte();
	I2C2_SendAck(0);  // ACK: 还要读低字节

	// 读低字节, 发NACK(停止)
	l = I2C2_ReadByte();
	I2C2_SendAck(1);  // NACK: 最后一个字节

	I2C2_Stop();

	raw = ((uint16_t)h << 8) | l;
	return (float)raw / 1.2f;  // Lux = raw / 1.2
}
