// display.c - OLED 8页屏幕布局渲染
// 
// Page0: 温度  (Temp:25.5C)
// Page1: 湿度  (Humi:55.2%)
// Page2: 光照  (Lux :320)
// Page3: PM2.5 (PM2.5:35 ug)
// Page4: 风扇/加湿器状态
// Page5: 补光灯状态/系统模式
// Page6: 标题分隔线
// Page7: 运行时间
// 
// 传感器未就绪时对应位置显示 "---"。

#include "display.h"
#include "oled.h"
#include "main.h"

void Display_Init(void)
{
	OLED_Init();
}

void DisplayAll(float temperature, float humidity, float light,
                int pm25, uint8_t deviceStatus)
{
// Page 0: 温度
	OLED_ShowString(0, 0, "Temp:");
	OLED_ClearLine(0, 36);  // 清除数字区域旧像素 (col36~77)
	if (temperature > -100.0f)
	{
		OLED_ShowFloat(0, 36, temperature);
		OLED_ShowString(0, 78, "C");
	}
	else
		OLED_ShowString(0, 36, "---");  // 传感器未就绪, 显示占位符

// Page 1: 湿度
	OLED_ShowString(1, 0, "Humi:");
	OLED_ClearLine(1, 36);  // 清除数字区域旧像素
	if (humidity > -100.0f)
	{
		OLED_ShowFloat(1, 36, humidity);
		OLED_ShowString(1, 78, "%");
	}
	else
		OLED_ShowString(1, 36, "---");

// Page 2: 光照 (整数显示, 1 Lux精度足够) 
	OLED_ShowString(2, 0, "Lux :");
	OLED_ClearLine(2, 36);  // 清除数字区域旧像素
	if (light >= 0.0f)
		OLED_ShowInt(2, 36, (int)light);
	else
		OLED_ShowString(2, 36, "---");

// Page 3: PM2.5 (ug = 简化μg/m³, 字库无希腊字母) 
	OLED_ShowString(3, 0, "PM2.5:");
	OLED_ClearLine(3, 42);  // 清除数字区域旧像素
	if (pm25 >= 0)
	{
		OLED_ShowInt(3, 42, pm25);
		OLED_ShowString(3, 78, "ug");
	}
	else
		OLED_ShowString(3, 42, "---");

// Page 4: 风扇/加湿器状态
// deviceStatus: bit0=风扇, bit1=加湿器, bit2=补光灯
	OLED_ShowString(4, 0, "Fan:");
	OLED_ShowString(4, 24, (deviceStatus & 0x01) ? "ON " : "OFF");
	OLED_ShowString(4, 60, "Hum:");
	OLED_ShowString(4, 84, (deviceStatus & 0x02) ? "ON " : "OFF");

// Page 5: 补光灯状态/系统模式 
	OLED_ShowString(5, 0, "Lux:");
	OLED_ShowString(5, 24, (deviceStatus & 0x04) ? "ON " : "OFF");
	OLED_ShowString(5, 60, "Mod:");
	OLED_ShowString(5, 84, (g_systemMode == MODE_AUTO) ? "AUTO" : "MANU");

// Page 6: 标题 
	OLED_ShowString(6, 0, "---Smart Env Monitor---");

// Page 7: 运行时间 (SysTick / 1000 = 秒)
	OLED_ShowString(7, 0, "Run:");
	OLED_ClearLine(7, 30);  // 清除数字区域旧像素
	OLED_ShowInt(7, 30, (int)(g_sysTick / 1000));
	OLED_ShowString(7, 78, "s");
}
