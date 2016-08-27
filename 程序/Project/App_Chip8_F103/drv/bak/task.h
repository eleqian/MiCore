#ifndef __TASK_H__
#define __TASK_H__

#include "base.h"

// 任务时间类型，和TASK_INTERVAL共同决定最大延迟时间
typedef uint16_t task_time_t;

// 任务号类型
typedef uint8_t task_id_t;

// 任务枚举
enum {
    TASK_KEY = 0,   // 按键扫描任务
    TASK_DSO,       // DSO任务
    TASK_LED,       // LED闪烁任务

    TASK_MAX
};

// 无效任务号
#define TASK_INVALID        ((task_id_t)-1)

// 任务计时时间间隔，单位ms
#define TASK_INTERVAL       1

// 无效任务时间间隔
#define TASK_TIME_INVALID   ((task_time_t)-1)

// 当前执行任务号
extern task_id_t gTaskCur;

// 初始化任务系统
// 必须在调用其它接口前调用
void Task_Init(void);

// 任务系统时钟
// 在时基中断中调用
void Task_OnTick(void);

// 检查当前就绪任务号
void Task_CheckReady(void);

// 设置下次任务执行时钟延迟
// 参数: 时钟数
void Task_SetTime(task_time_t t);

// 设置当前任务下次执行时间延迟
// 参数: 时间ms
#if TASK_INTERVAL == 1
#define Task_Delay(t) Task_SetTime(t)
#else
#define Task_Delay(t) Task_SetTime((task_time_t)((t) / TASK_INTERVAL))
#endif

#endif  // __TASK_H__
