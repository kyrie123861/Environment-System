// sensor.c - 传感器统一采集模块
// 
// 封装 DHT22/BH1750/PMS5003 的读取, 维护数据缓存。
// 读取失败时保留上次有效值, 避免单次故障导致后续模块拿到错误数据。
// 
// 缓存初始值设为不可能的值 (-999 / -1), 用于判断"从未读取成功"。

#include "sensor.h"
#include "dht22.h"
#include "bh1750.h"
#include "pm25.h"

// 传感器数据缓存, 初始值表示"从未有效读取"
static float g_temperature = -999.0f;
static float g_humidity    = -999.0f;
static float g_light       = -1.0f;
static int   g_pm25        = -1;

void Sensor_Init(void)
{
	DHT22_Init();  // 配置PA1, 等待1s稳定
	BH1750_Init();  // I2C初始化, 等待180ms首次测量
	PM25_Init();  // 清空UART缓冲区
}

void Sensor_Collect(void)
{
	DHT22_Data dht;
	float light;
	PM25_Data  pm;

// 读DHT22, 校验通过才更新缓存
	dht = DHT22_Read();
	if (dht.valid)
	{
		g_temperature = dht.temperature;
		g_humidity    = dht.humidity;
	}
// 校验失败: 不更新, 保留上次有效值

// 读光照, 返回值>=0表示成功
	light = BH1750_Read();
	if (light >= 0.0f)
		g_light = light;

// 读PM2.5, valid=1表示帧校验通过
	pm = PM25_Read();
	if (pm.valid)
		g_pm25 = (int)pm.pm2_5;
}

float GetTemperature(void) { return g_temperature; }
float GetHumidity(void)    { return g_humidity; }
float GetLight(void)       { return g_light; }
int   GetPM25(void)        { return g_pm25; }
