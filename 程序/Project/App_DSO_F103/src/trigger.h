#ifndef __TRIGGER_H__
#define __TRIGGER_H__

#include "base.h"

// 触发模式枚举
enum {
    TRIG_MODE_ROLL,     // 滚动，不进行触发而直接更新触发状态（仅采样率较低时可用）
    TRIG_MODE_SINGLE,   // 单次，只进行一次触发然后停止
    TRIG_MODE_NORMAL,   // 普通，每当满足触发条件即进行触发，未触发时不刷新状态
    TRIG_MODE_AUTO,     // 自动，每当满足触发条件即进行触发，同时未触发时也刷新状态
    TRIG_MODE_MAX
};

// 触发边沿枚举
enum {
    TRIG_EDGE_RISE,     // 上升沿
    TRIG_EDGE_FALL,     // 下降沿
    TRIG_EDGE_MAX
};

void Trig_Init(void);

void Trig_Start(void);

void Trig_Stop(void);

BOOL Trig_CheckUpdate(void);

void Trig_ClearUpdate(void);

void Trig_SetMode(uint8_t mode);

void Trig_SetEdge(uint8_t edge);

void Trig_SetLevel(uint16_t level);

void Trig_SetPreSize(uint16_t size);

uint16_t Trig_GetSampleCnt(void);

#endif // __TRIGGER_H__
