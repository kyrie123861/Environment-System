// pm25.c - PMS5003 激光PM2.5传感器 (UART, 被动接收)
// 
// 传感器以200~800ms间隔主动发送32字节数据帧, 无需MCU请求。
// 
// 数据帧格式 (32字节):
// 偏移0: 0x42 (帧头1)
// 偏移1: 0x4D (帧头2)
// 偏移2~3: 帧长度 (固定0x001C=28)
// 偏移4~5: PM1.0标准浓度   偏移6~7: PM2.5标准浓度 ★
// 偏移8~9: PM10标准浓度    偏移10~15: 大气环境浓度
// 偏移16~29: 颗粒物计数    偏移30~31: 前30字节累加和(低16位)
// 
// 状态机解析: 逐字节扫描, 不假设帧边界对齐, 抗干扰能力强。
// IDLE -> (收0x42) -> GOT_42 -> (收0x4D) -> FRAME(收30字节) -> 校验 -> IDLE

#include "pm25.h"
#include "uart.h"

// 状态机三状态
#define STATE_IDLE    0  // 搜索帧头0x42
#define STATE_GOT_42  1  // 已收0x42, 等0x4D
#define STATE_FRAME   2  // 帧头完整, 收剩余30字节

void PM25_Init(void)
{
	UART_Flush();            /* 清空UART缓冲区(丢弃上电乱码)
	                           PMS5003风扇需约30s稳定后才输出有效数据 */
}

PM25_Data PM25_Read(void)
{
	PM25_Data result;
	uint8_t buf[32];
	uint8_t state = STATE_IDLE;
	uint8_t byte;
	uint8_t idx = 0;
	uint16_t calc_crc, recv_crc;
	uint8_t i;

// 初始化返回值为无效
	result.pm1_0 = 0;
	result.pm2_5 = 0;
	result.pm10  = 0;
	result.valid = 0;

// 逐字节扫描环形缓冲区
	while (UART_ReadByte(&byte))
	{
		switch (state)
		{
		case STATE_IDLE:
			if (byte == 0x42)  // 发现帧头首字节
			{
				buf[0] = byte;
				state = STATE_GOT_42;
				idx = 1;
			}
			break;

		case STATE_GOT_42:
			if (byte == 0x4D)  // 帧头完整 (0x42 0x4D)
			{
				buf[1] = byte;
				state = STATE_FRAME;
				idx = 2;
			}
			else if (byte == 0x42)  // 连续两个0x42: 将当前作为新帧头
			{
				buf[0] = byte;
// state保持GOT_42
			}
			else  // 0x42后既非0x4D也非0x42: 无效
			{
				state = STATE_IDLE;
				idx = 0;
			}
			break;

		case STATE_FRAME:
			buf[idx++] = byte;

			if (idx >= 32)  // 收满32字节 = 完整帧
			{
// ① 验证帧长度: 必须=28
				uint16_t frame_len = ((uint16_t)buf[2] << 8) | buf[3];
				if (frame_len != 28)
				{
					state = STATE_IDLE;  // 帧长异常, 丢弃重来
					idx = 0;
					break;
				}

// ② 验证校验和: 前30字节累加和 = buf[30:31]
				calc_crc = 0;
				for (i = 0; i < 30; i++)
					calc_crc += buf[i];
				recv_crc = ((uint16_t)buf[30] << 8) | buf[31];

				if (calc_crc == recv_crc)
				{
// ③ 校验通过: 提取PM1.0/2.5/10 (大端, 高字节在前)
					result.pm1_0  = ((uint16_t)buf[4] << 8) | buf[5];
					result.pm2_5  = ((uint16_t)buf[6] << 8) | buf[7];
					result.pm10   = ((uint16_t)buf[8] << 8) | buf[9];
					result.valid  = 1;

					return result;  // 成功, 返回有效帧
				}

// 校验失败: 丢弃, 重新搜索
				state = STATE_IDLE;
				idx = 0;
			}
			break;
		}
	}

// 缓冲区数据不足一帧, 返回无效 (下次中断积累足够数据后再试)
	return result;
}
