#ifndef __SAMPLE_H__
#define __SAMPLE_H__

#include "base.h"

// 最大采样数
#define SAMPLES_MAX                 2048

// 定时器计数周期(72MHz)
#define SAMPLE_TIMER_PERIOD_32K     2250
#define SAMPLE_TIMER_PERIOD_8K      9000

// 采样结果缓冲区
extern uint16_t gSampleValues[SAMPLES_MAX];

// 初始化
void Sample_Init(void);

// 开始采样
// 参数：采样周期，采样数
void Sample_Start(uint16_t speriod, uint16_t scount);

// 采样完成事件通知函数，需外部实现
extern void OnSampleComplete(void);

#endif //__SAMPLE_H__
