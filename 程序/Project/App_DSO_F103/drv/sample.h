#ifndef __SAMPLE_H__
#define __SAMPLE_H__

#include "base.h"

// 无效采样结果
#define SAMPLE_RESULT_INVALID       0xffff

// 采样最大值
#define SAMPLE_RESULT_MAX           4095

// 采样最小值
#define SAMPLE_RESULT_MIN           0

// 采样率枚举
typedef enum _sampleRate_enum {
    SAMPLE_RATE_20,
    SAMPLE_RATE_50,
    SAMPLE_RATE_100,
    SAMPLE_RATE_200,
    SAMPLE_RATE_500,
    SAMPLE_RATE_1K,
    SAMPLE_RATE_2K,
    SAMPLE_RATE_5K,
    SAMPLE_RATE_10K,
    SAMPLE_RATE_20K,
    SAMPLE_RATE_50K,
    SAMPLE_RATE_100K,
    SAMPLE_RATE_200K,
    SAMPLE_RATE_500K,
    SAMPLE_RATE_1M,
    SAMPLE_RATE_2M,
    SAMPLE_RATE_MAX
} sampleRate_t;

// 采样率枚举对应的实际值
extern const uint32_t tblSampleRateValue[SAMPLE_RATE_MAX];

// 初始化
void Sample_Init(void);

// 设置采样率
// 参数：采样率枚举（20Hz~2MHz，以1-2-5递增）
void Sample_SetRate(sampleRate_t rate);

// 开始采样
void Sample_Start(void);

// 停止采样
void Sample_Stop(void);

// 采样完成事件通知函数，需外部实现
// 参数：结果缓冲区，结果数目
extern void OnSampleComplete(const uint16_t *buf, uint16_t count);

#endif //__SAMPLE_H__
