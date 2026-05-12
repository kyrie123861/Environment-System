// filter.c - 传感器数据滤波
// 
// 温度/湿度/光照: 8点滑动平均 (MA), 维护sum实现O(1)更新
// PM2.5: 一阶IIR低通, y(n) = 0.1*x(n) + 0.9*y(n-1)
// 
// 冷启动优化: 首次收到有效数据时直接填充整个窗口, 避免被初始的0拉低均值。

#include "filter.h"

// 滑动平均滤波器结构体
// sum维护当前窗口总和: sum_new = sum_old - 被淘汰旧值 + 新值, 避免每次遍历求和
typedef struct {
	float buf[FILTER_WINDOW_SIZE];
	uint8_t index;  // 下一次写入位置
	uint8_t count;  // 当前有效数据个数, 未满时用于正确计算均值
	float sum;  // 窗口内数据总和
} MAFilter;

static MAFilter g_temp_filter;
static MAFilter g_humi_filter;
static MAFilter g_light_filter;

// PM2.5 IIR滤波状态
static float g_pm25_filtered = 0.0f;
static uint8_t g_pm25_inited = 0;

static void MAFilter_Init(MAFilter *f)
{
	uint8_t i;
	for (i = 0; i < FILTER_WINDOW_SIZE; i++)
		f->buf[i] = 0.0f;
	f->index = 0;
	f->count = 0;
	f->sum = 0.0f;
}

// O(1)滑动平均更新: sum = sum - 被覆盖的旧值 + 新值
static float MAFilter_Update(MAFilter *f, float val)
{
	f->sum -= f->buf[f->index];  // 减去即将被覆盖的旧值
	f->buf[f->index] = val;  // 新值写入环形缓冲
	f->sum += val;  // 加上新值

	f->index = (f->index + 1) % FILTER_WINDOW_SIZE;
	if (f->count < FILTER_WINDOW_SIZE)
		f->count++;  // 窗口未满时计数递增

	return f->sum / (float)f->count;  // 返回窗口均值
}

// 冷启动快速填充: 把所有历史值设为当前值, 立即输出真实值
static float MAFilter_Fill(MAFilter *f, float val)
{
	uint8_t i;
	for (i = 0; i < FILTER_WINDOW_SIZE; i++)
		f->buf[i] = val;
	f->sum = val * FILTER_WINDOW_SIZE;
	f->count = FILTER_WINDOW_SIZE;
	return val;
}

void Filter_Reset(void)
{
	MAFilter_Init(&g_temp_filter);
	MAFilter_Init(&g_humi_filter);
	MAFilter_Init(&g_light_filter);
	g_pm25_filtered = 0.0f;
	g_pm25_inited = 0;
}

float FilterTemperature(float val)
{
// 无效值: 返回上次均值
	if (val < -100.0f)
		return g_temp_filter.count > 0 ? g_temp_filter.sum / g_temp_filter.count : 0.0f;

// 冷启动: 窗口为空, 快速填充
	if (g_temp_filter.count == 0)
		return MAFilter_Fill(&g_temp_filter, val);

	return MAFilter_Update(&g_temp_filter, val);
}

float FilterHumidity(float val)
{
	if (val < -100.0f)
		return g_humi_filter.count > 0 ? g_humi_filter.sum / g_humi_filter.count : 0.0f;

	if (g_humi_filter.count == 0)
		return MAFilter_Fill(&g_humi_filter, val);

	return MAFilter_Update(&g_humi_filter, val);
}

float FilterLight(float val)
{
	if (val < 0.0f)  // 光照不可能为负
		return g_light_filter.count > 0 ? g_light_filter.sum / g_light_filter.count : 0.0f;

	if (g_light_filter.count == 0)
		return MAFilter_Fill(&g_light_filter, val);

	return MAFilter_Update(&g_light_filter, val);
}

int FilterPM25(int val)
{
	if (val < 0)  // 无效值: 返回上次输出
		return (int)(g_pm25_filtered + 0.5f);  // +0.5用于四舍五入

// 冷启动: 直接赋值
	if (!g_pm25_inited)
	{
		g_pm25_filtered = (float)val;
		g_pm25_inited = 1;
		return val;
	}

// IIR: y = α*x + (1-α)*y_prev
// α=0.1: 新数据权重只有10%, 强平滑, 适合PM2.5数据波动大的特点
	g_pm25_filtered = IIR_ALPHA * val + (1.0f - IIR_ALPHA) * g_pm25_filtered;

	return (int)(g_pm25_filtered + 0.5f);
}
