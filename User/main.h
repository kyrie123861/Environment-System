// main.h - 主配置文件
// 
// 包含系统参数、控制阈值、硬件引脚、数据类型、全局变量声明。
// 硬件: STM32F103C8T6, 72MHz

#ifndef __MAIN_H
#define __MAIN_H

#include "stm32f10x.h"
#include "systick.h"
#include <stdint.h>

// ========================================================================
// 系统参数
// =======================================================================

// 传感器采样间隔 1000ms: SysTick每1ms中断, 计数满1000触发一次采集
#define SAMPLE_INTERVAL_MS     1000

// 滑动平均滤波窗口大小: 8个采样点, 越大越平滑但响应越慢
#define FILTER_WINDOW_SIZE     8

// IIR低通滤波系数: 0.1, 新数据占10%权重, 适合PM2.5波动大的场景
#define IIR_ALPHA              0.1f

// ========================================================================
// 控制阈值 (滞回控制, 上下限之间有死区, 防止继电器在临界点反复开关)
// =======================================================================

// 温度: >30°C开风扇, <26°C关风扇, 4°C死区
#define TEMP_HIGH_THRESHOLD    30.0f
#define TEMP_LOW_THRESHOLD     26.0f

// 湿度: <40%开加湿器, >60%关加湿器, 20%死区
#define HUMI_LOW_THRESHOLD     40.0f
#define HUMI_HIGH_THRESHOLD    60.0f

// 光照: <100Lux开补光灯, >500Lux关补光灯, 400Lux死区 (光照波动大)
#define LIGHT_LOW_THRESHOLD    100.0f
#define LIGHT_HIGH_THRESHOLD   500.0f

// PM2.5报警阈值: >150 ug/m3 触发蜂鸣器
#define PM25_ALARM_THRESHOLD   150

// 温度严重超限: >35°C 触发蜂鸣器 (设备过热保护)
#define TEMP_CRITICAL          27.0f

// ========================================================================
// 硬件引脚定义
// =======================================================================

// 继电器 (PB0/PB1/PB10, 低电平触发, 使用ODR位掩码控制)
#define RELAY_FAN_PIN          GPIO_ODR_ODR0  // PB0: 风扇
#define RELAY_HUMIDIFIER_PIN   GPIO_ODR_ODR1  // PB1: 加湿器
#define RELAY_LIGHT_PIN        GPIO_ODR_ODR10  // PB10: 补光灯

// DHT22 (PA1, 单线协议, 需外接4.7kΩ上拉电阻)
#define DHT22_PORT             GPIOA
#define DHT22_PIN              1

// I2C总线1 (PB6=SCL, PB7=SDA, 软件模拟, 专用于OLED)
#define I2C_SCL_PIN            6
#define I2C_SDA_PIN            7

// I2C总线2 (PA5=SCL, PA6=SDA, 软件模拟, 专用于BH1750)
// 面包板环境下无法用杜邦线将BH1750与OLED并联到同一组MCU引脚,
// 因此为BH1750单独分配独立I2C总线
#define I2C2_SCL_PIN           5
#define I2C2_SDA_PIN           6

// I2C设备地址 (7位地址左移1位 = 8位写地址)
// BH1750: 7位地址0x23, 写=0x46, 读=0x47
// OLED:   SSD1306固定7位地址0x3C, 写=0x78
#define BH1750_ADDR            0x46
#define OLED_ADDR              0x78

// PM2.5 UART接收环形缓冲区: 64字节可缓冲2帧 (每帧32字节)
#define PM25_RX_BUF_SIZE       64

// ========================================================================
// 公共数据类型
// =======================================================================

// 设备类型: 用于ControlDevice统一操作三个继电器
typedef enum {
	DEVICE_FAN = 0,  // 风扇
	DEVICE_HUMIDIFIER = 1,  // 加湿器
	DEVICE_LIGHT = 2  // 补光灯
} DeviceType;

// 系统运行模式
typedef enum {
	MODE_AUTO = 0,  // 自动模式: 根据传感器阈值自动控制
	MODE_MANUAL = 1  // 手动模式: 禁用自动控制
} SystemMode;

// DHT22返回数据结构
typedef struct {
	float temperature;  // 温度 (°C), 分辨率0.1
	float humidity;  // 湿度 (%RH), 分辨率0.1
	uint8_t valid;  // 校验通过标志: 1=有效
} DHT22_Data;

// PMS5003返回数据结构
typedef struct {
	uint16_t pm1_0;  // PM1.0浓度 (ug/m3)
	uint16_t pm2_5;  // PM2.5浓度 (ug/m3)
	uint16_t pm10;  // PM10浓度  (ug/m3)
	uint8_t valid;  // 帧校验通过: 1=有效
} PM25_Data;

// ========================================================================
// 全局变量外部声明
// =======================================================================

extern volatile uint8_t  g_deviceStatus;  // 设备状态: bit0=风扇,bit1=加湿器,bit2=补光灯
extern volatile SystemMode g_systemMode;  // 当前运行模式

#endif  // __MAIN_H
