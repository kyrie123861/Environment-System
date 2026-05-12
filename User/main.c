// main.c - 系统主程序
//
// 流程: 外设初始化 -> 主循环(采集/滤波/控制/报警/显示)
// 主循环每1s执行一次, 由SysTick模块的g_sample_flag驱动, 空闲时__WFI()休眠。
//
// 时钟配置: startup_stm32f10x_hd.s 在进入main()之前调用 SystemInit(),
// SystemInit() -> SetSysClock() -> SetSysClockTo72() 已完成:
// HSE(8M) -> PLL x9 -> 72MHz, Flash 2WS, HPRE=/1, PPRE1=/2, PPRE2=/1
// 因此 main() 中无需再次配置时钟。

#include "main.h"
#include "delay.h"
#include "i2c.h"
#include "i2c2.h"
#include "uart.h"
#include "dht22.h"
#include "bh1750.h"
#include "pm25.h"
#include "oled.h"
#include "sensor.h"
#include "filter.h"
#include "control.h"
#include "display.h"
#include "alarm.h"

// 全局变量
volatile uint8_t   g_deviceStatus  = 0;  // 设备状态位掩码
volatile SystemMode g_systemMode   = MODE_AUTO;

// 报警条件检查: PM2.5>150 或 温度>35°C 触发
static uint8_t CheckAlarm(int pm25, float temp)
{
	if (pm25 > PM25_ALARM_THRESHOLD) return 1;
	if (temp > TEMP_CRITICAL)        return 1;
	return 0;
}

// main - 主函数
// 
// 初始化顺序:
// I2C -> UART -> 继电器+按键 -> 蜂鸣器 -> 传感器 -> OLED -> 滤波器 -> SysTick
// 注意: I2C必须在BH1750/OLED之前初始化, SysTick最后启动避免打断初始化
// 时钟已在startup阶段由SystemInit()配置为72MHz, 无需再次配置
int main(void)
{
	float temp_raw, temp_filtered;
	float humi_raw, humi_filtered;
	float light_raw, light_filtered;
	int   pm25_raw, pm25_filtered;

	// 1. 基础外设: I2C(OLED), I2C2(BH1750), UART(PM2.5), 继电器+按键, 蜂鸣器
	// 时钟已在startup阶段由SystemInit()配置为72MHz, 无需再次配置
	I2C_Init();
	I2C2_Init();
	UART_Init();
	Control_Init();
	Alarm_Init();

	// 2. 传感器 + OLED + 滤波器
	Sensor_Init();
	Display_Init();
	Filter_Reset();

	// 3. 启动SysTick 1ms时基 (放在初始化完成之后)
	SysTick_Init();

	// 4. 启动画面 + 首次采集预热
	OLED_ShowString(3, 0, "  System Starting...");
	Delay_ms(1000);
	OLED_Clear();
	Sensor_Collect();
	Delay_ms(500);

	// 5. 主循环: 等待采集标志 -> 5阶段处理 -> WFI休眠
	// (A)采集原始数据 -> (B)滤波 -> (C)滞回控制 -> (D)报警 -> (E)刷新显示
	while (1)
	{
		if (g_sample_flag)
		{
			g_sample_flag = 0;  // 立即清零, 防止重复执行

			// (A) 采集: 一次读取三个传感器, 通过缓存获取值
			Sensor_Collect();
			temp_raw  = GetTemperature();
			humi_raw  = GetHumidity();
			light_raw = GetLight();
			pm25_raw  = GetPM25();

			// (B) 滤波: 温湿光用8点滑动平均, PM2.5用IIR低通
			temp_filtered  = FilterTemperature(temp_raw);
			humi_filtered  = FilterHumidity(humi_raw);
			light_filtered = FilterLight(light_raw);
			pm25_filtered  = FilterPM25(pm25_raw);

			// (C) 控制: 滞回比较, 仅在MODE_AUTO下生效
			ControlAll(temp_filtered, humi_filtered, light_filtered);
			g_deviceStatus = Control_GetStatus();

			// (D) 报警: PM2.5/温度超限 -> 蜂鸣器响
			if (CheckAlarm(pm25_filtered, temp_filtered))
				Alarm_On();
			else
				Alarm_Off();

			// (E) 显示: 刷新OLED 8页内容
			DisplayAll(temp_filtered, humi_filtered, light_filtered,
			           pm25_filtered, g_deviceStatus);
		}

		__WFI();  // 休眠, 任意中断唤醒
	}
}
