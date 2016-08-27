/* 示波器触发模块
 * eleqian 2014-3-9
 */

#include "base.h"
#include "timebase.h"
#include "sample.h"
#include "sfifo.h"
#include "trigger.h"

// 判定边沿
#define IS_RISE_EDGE(pre, cur, level)   \
    ((pre) < (cur) && (pre) <= (level) && (level) <= (cur))
#define IS_FALL_EDGE(pre, cur, level)   \
    ((pre) > (cur) && (pre) >= (level) && (level) >= (cur))

// 触发器标志位
#define TRIG_FLAG_PRESAMPLE     0x1     // 预采样标志；进行首次预触发采样时置位，采样完成清除
#define TRIG_FLAG_TRIGGERED     0x2     // 触发标志；成功进行触发时置位，后触发采样完成时清除
#define TRIG_FLAG_UPDATED       0x4     // 更新标志；一次触发并采样完成时置位，CLR_UPDATE被置位时清除
#define TRIG_FLAG_CLR_UPDATE    0x8     // 清除更新标志；外部置位，在采样处理中立即清除

// 定义自动刷新时间间隔(ms)
#define TRIG_AUTO_UPDATE_TIM    100

// 触发参数
static uint8_t gTriggerMode;
static uint8_t gTriggerEdge;
static uint16_t gTriggerLevel;
static uint16_t gTriggerPreSize;

// 上次采样数据
static uint16_t gPreSample;

// 已采样数目
static uint32_t gSampleCnt;

// 上次采样数目
static uint32_t gLastSampleCnt;

// 上次更新时间，用于Auto模式更新
static uint32_t gLastUpdateTime;

// 触发标志
static uint8_t gTriggerFlag;

/*-----------------------------------*/

// 初始化
void Trig_Init(void)
{
    gTriggerMode = TRIG_MODE_AUTO;
    gTriggerEdge = TRIG_EDGE_RISE;
    gTriggerLevel = (SAMPLE_RESULT_MAX + SAMPLE_RESULT_MIN) / 2;
    gTriggerPreSize = SFIFO_SIZE / 2;

    gPreSample = SAMPLE_RESULT_INVALID;
    gSampleCnt = 0;
    gTriggerFlag = 0;
    gLastUpdateTime = 0;
}

// 启动触发
void Trig_Start(void)
{
    gPreSample = SAMPLE_RESULT_INVALID;
    gSampleCnt = 0;
    gTriggerFlag = 0;
    gLastUpdateTime = millis();

    if (gTriggerMode != TRIG_MODE_ROLL) {
        // 第一次触发前需要进行预触发采样
        BITS_SET(gTriggerFlag, TRIG_FLAG_PRESAMPLE);
    }

    SFIFO_Reset();
    Sample_Start();
}

// 停止触发
void Trig_Stop(void)
{
    Sample_Stop();
    gTriggerFlag = 0;
}

/*-----------------------------------*/

// 检查更新标志位
BOOL Trig_CheckUpdate(void)
{
    return BITS_CHK(gTriggerFlag, TRIG_FLAG_UPDATED);
}

// 清除更新标志位
void Trig_ClearUpdate(void)
{
    if (gTriggerMode == TRIG_MODE_ROLL || gTriggerMode == TRIG_MODE_SINGLE) {
        // 滚动或单次触发模式，直接清除更新标志模式即可
        BITS_CLR(gTriggerFlag, TRIG_FLAG_UPDATED);
    } else {
        // 其它模式需要在采样中断中进行其它处理
        BITS_SET(gTriggerFlag, TRIG_FLAG_CLR_UPDATE);
    }
}

/*-----------------------------------*/

// 设置触发方式
void Trig_SetMode(uint8_t mode)
{
    gTriggerMode = mode;

    if (mode == TRIG_MODE_ROLL) {
        // 滚动模式不需要使用双FIFO，而且单FIFO可以使存储深度加倍
        SFIFO_SetMode(SFIFO_MODE_NORMAL);
    } else {
        // 其它模式使用双FIFO，保证始终有一个完整的采样记录
        SFIFO_SetMode(SFIFO_MODE_SEPARATE);
    }
}

// 设置触发边沿
void Trig_SetEdge(uint8_t edge)
{
    gTriggerEdge = edge;
}

// 设置触发电平
void Trig_SetLevel(uint16_t level)
{
    gTriggerLevel = level;
}

// 设置预触发深度
void Trig_SetPreSize(uint16_t size)
{
    gTriggerPreSize = size;
}

// 取得采样数
uint16_t Trig_GetSampleCnt(void)
{
    return (uint16_t)gLastSampleCnt;
}

/*-----------------------------------*/

// 接收采样完成事件
void OnSampleComplete(const uint16_t *buf, uint16_t count)
{
    // 无触发模式，直接写FIFO并更新
    if (gTriggerMode == TRIG_MODE_ROLL) {
        SFIFO_Write(buf, count);
        BITS_SET(gTriggerFlag, TRIG_FLAG_UPDATED);
        gSampleCnt += count;
        if (gSampleCnt > SFIFO_SIZE) {
            gSampleCnt = SFIFO_SIZE;
        }
        gLastSampleCnt = gSampleCnt;
        return;
    }

    // 正在进行首次预触发采样时不进行触发
    if (BITS_CHK(gTriggerFlag, TRIG_FLAG_PRESAMPLE)) {
        SFIFO_Write(buf, count);
        gSampleCnt += count;
        if (gSampleCnt >= gTriggerPreSize) {
            BITS_CLR(gTriggerFlag, TRIG_FLAG_PRESAMPLE);
            gLastUpdateTime = millis();
            gSampleCnt = gTriggerPreSize;
            gPreSample = *(buf + count - 1);
        }
        return;
    }

    // 更新标志位被外部清除，可以进行下一次触发
    if (BITS_CHK(gTriggerFlag, TRIG_FLAG_CLR_UPDATE)) {
        BITS_CLR(gTriggerFlag, TRIG_FLAG_CLR_UPDATE | TRIG_FLAG_UPDATED);
        // 在采样率较低的时候会出现预触发采样空间没能在显示刷新期间采样满
        // 通过将部分上次采样数据作为本次预触发采样，可以提高刷新率
        if (gSampleCnt < gTriggerPreSize) {
            SFIFO_Copy_R2W(gTriggerPreSize - gSampleCnt);
        }
        gSampleCnt = gTriggerPreSize;
    }

    // 更新标志位没被清除，说明上次更新还没处理
    if (BITS_CHK(gTriggerFlag, TRIG_FLAG_UPDATED)) {
        // 继续进行下一次预触发采样
        SFIFO_Write(buf, count);
        gSampleCnt += count;
        gPreSample = *(buf + count - 1);
        return;
    }

    // 尚未触发
    if (!BITS_CHK(gTriggerFlag, TRIG_FLAG_TRIGGERED)) {
        size_t i;
        uint16_t level;
        uint16_t sample, presample;

        // 边沿触发检测
        level = gTriggerLevel;
        presample = gPreSample;
        if (gTriggerEdge == TRIG_EDGE_RISE) {
            // 上升沿
            for (i = 0; i < count; i++) {
                sample = *(buf + i);
                if (IS_RISE_EDGE(presample, sample, level)) {
                    BITS_SET(gTriggerFlag, TRIG_FLAG_TRIGGERED);
                    break;
                }
                presample = sample;
            }
        } else {
            // 下降沿
            for (i = 0; i < count; i++) {
                sample = *(buf + i);
                if (IS_FALL_EDGE(presample, sample, level)) {
                    BITS_SET(gTriggerFlag, TRIG_FLAG_TRIGGERED);
                    break;
                }
                presample = sample;
            }
        }
        gPreSample = *(buf + count - 1);

        // 将预触发采样部分写FIFO
        SFIFO_Write(buf, i);

        if (BITS_CHK(gTriggerFlag, TRIG_FLAG_TRIGGERED)) {
            gSampleCnt = gTriggerPreSize;
        } else {
            gSampleCnt += i;
            if (gSampleCnt > SFIFO_SIZE) {
                gSampleCnt = SFIFO_SIZE;
            }
        }

        // 继续进行触发后采样部分处理
        buf += i;
        count -= i;
    }

    // 已经触发（正在进行的是后触发采样）
    if (BITS_CHK(gTriggerFlag, TRIG_FLAG_TRIGGERED)) {
        if (gSampleCnt + count >= SFIFO_SIZE) {
            // FIFO写满即产生更新事件
            SFIFO_Write(buf, SFIFO_SIZE - gSampleCnt);
            BITS_CLR(gTriggerFlag, TRIG_FLAG_TRIGGERED);
            BITS_SET(gTriggerFlag, TRIG_FLAG_UPDATED);
            gLastSampleCnt = SFIFO_SIZE;

            if (gTriggerMode == TRIG_MODE_SINGLE) {
                // 单次模式时触发后就停止
                Sample_Stop();
                SFIFO_Switch_RW();
            } else {
                // 普通或自动模式切换FIFO继续采样准备下一次触发
                gLastUpdateTime = millis();
                gSampleCnt = gSampleCnt + count - SFIFO_SIZE;
                SFIFO_Switch_RW();
                SFIFO_Write(buf, gSampleCnt);
            }
        } else {
            // 否则继续写后触发FIFO
            SFIFO_Write(buf, count);
            gSampleCnt += count;
        }
        return;
    }

    // 自动模式在未触发时进行超时更新
    if (gTriggerMode == TRIG_MODE_AUTO && !BITS_CHK(gTriggerFlag, TRIG_FLAG_TRIGGERED)) {
        uint32_t tnow;

        tnow = millis();
        if (tnow >= gLastUpdateTime + TRIG_AUTO_UPDATE_TIM) {
            gLastUpdateTime = tnow;
            /*if (gSampleCnt < SFIFO_SIZE) {
                SFIFO_Copy_R2W(SFIFO_SIZE - gSampleCnt);
            }*/
            //SFIFO_Fill(SAMPLE_RESULT_INVALID, SFIFO_SIZE - gSampleCnt);
            SFIFO_Switch_RW();
            gLastSampleCnt = gSampleCnt;
            gSampleCnt = 0;
            BITS_SET(gTriggerFlag, TRIG_FLAG_UPDATED);
            return;
        }
    }
}
