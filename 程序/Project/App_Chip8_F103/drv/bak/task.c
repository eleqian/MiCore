/*
 * 任务优先级和时间管理模块 for stm32
 * eleqian 2013-2-9
 */

#include "base.h"
#include "task.h"

// 任务就绪延时
static volatile task_time_t gTaskDelays[TASK_MAX];

// 当前执行任务号
task_id_t gTaskCur;

/*-----------------------------------*/

void Task_Init(void)
{
    task_id_t i;

    for (i = 0; i < TASK_MAX; i++) {
        gTaskDelays[i] = 0;
    }

    gTaskCur = TASK_INVALID;
}

void Task_OnTick(void)
{
    task_id_t i;
    task_time_t t;

    for (i = 0; i < TASK_MAX; i++) {
        t = gTaskDelays[i];
        if (t != 0 && t != TASK_TIME_INVALID) {
            gTaskDelays[i] = t - 1;
        }
    }
}

void Task_CheckReady(void)
{
    task_id_t i;

    // 获取就绪任务中优先级最高的
    for (i = 0; i < TASK_MAX; i++) {
        if (gTaskDelays[i] == 0) {
            gTaskDelays[i] = TASK_TIME_INVALID;
            gTaskCur = i;
            return;
        }
    }

    gTaskCur = TASK_INVALID;
}

void Task_SetTime(task_time_t t)
{
    if (t == 0) {
        t++;
    }
    gTaskDelays[gTaskCur] = t;
}
