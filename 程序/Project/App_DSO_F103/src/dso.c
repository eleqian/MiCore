/* 示波器逻辑模块
 * eleqian 2014-8-18
 */

#include "base.h"
#include "timebase.h"
#include "task.h"
#include "key.h"
#include "lcd.h"
#include "led.h"
#include "sample.h"
#include "spi_flash.h"
#include "sfifo.h"
#include "trigger.h"
#include "dso.h"

// DSO显示刷新时间间隔
#define DSO_INTERVAL        50

// 波形区域显示宽高
#define DSO_VIEW_WAVE_W     100
#define DSO_VIEW_WAVE_H     80

// 采样值中点（零点校准）
#define DSO_SAMPLE_CENTER   2040

// 滚动模式使能时基值
#define DSO_ROLLMODE_TB     12

// 时基等级数
#define DSO_TB_COUNT        19

// 时基字符串
static const char DSO_TB_Strings[DSO_TB_COUNT][5] = {
    " 5uS", "10uS", "20uS", "50uS", ".1mS", ".2mS", ".5mS", " 1mS",
    " 2mS", " 5mS", "10mS", "20mS", "50mS", "0.1S", "0.2S", "0.5S",
    "  1S", "  2S", "  5S"
};

// 时基对应X缩放（除以）
static const uint8_t DSO_XScales[DSO_TB_COUNT] = {
     1,  1,  1,  1,  1,  1,  1,  2,
     2,  5,  5, 10, 10, 10, 10, 10,
    10, 10, 10
};

// 时基对应的采样率
static const sampleRate_t DSO_SampleRates[DSO_TB_COUNT] = {
    SAMPLE_RATE_2M, SAMPLE_RATE_1M, SAMPLE_RATE_500K, SAMPLE_RATE_200K,
    SAMPLE_RATE_100K, SAMPLE_RATE_50K, SAMPLE_RATE_20K, SAMPLE_RATE_20K,
    SAMPLE_RATE_10K, SAMPLE_RATE_10K, SAMPLE_RATE_5K, SAMPLE_RATE_5K,
    SAMPLE_RATE_2K, SAMPLE_RATE_1K, SAMPLE_RATE_500, SAMPLE_RATE_200,
    SAMPLE_RATE_100, SAMPLE_RATE_50, SAMPLE_RATE_20
};

// Y缩放等级数
#define DSO_YS_COUNT    6

// Y缩放字符串
static const char DSO_YS_Strings[DSO_YS_COUNT][5] = {
    "10mV", "20mV", "50mV", "0.1V", "0.2V", "0.5V"
};

// Y缩放实际倍数（除以）
static const uint8_t DSO_YScales[DSO_YS_COUNT] = {
    1, 2, 5, 10, 20, 50
};

// 触发模式字符串
static const char DSO_TrigMode_Strings[TRIG_MODE_MAX][2] = {
    "R", "S", "N", "A"
};

// 上升沿图标
static const uint8_t DSO_Trig_RiseSym[] = {
    0x40,0x40,0x58,0x4C,0x7F,0x0D,0x19,0x01,0x01,0x00,
};

// 下降沿图标
static const uint8_t DSO_Trig_FallSym[] = {
    0x01,0x01,0x0D,0x19,0x7F,0x58,0x4C,0x40,0x40,0x00,
};

// 触发边沿图标数组
static const uint8_t *DSO_Trig_EdgeSym[TRIG_EDGE_MAX] = {
    DSO_Trig_RiseSym, DSO_Trig_FallSym
};

// 波形Y坐标类型枚举
enum {
    WAVE_IDX_MIN,
    WAVE_IDX_MAX
};

// 显示焦点枚举
enum {
    VIEW_FOCUS_XSCALE,
    VIEW_FOCUS_YSCALE,
    VIEW_FOCUS_XOFFSET,
    VIEW_FOCUS_YOFFSET,
    VIEW_FOCUS_TRIG_LEVEL,
    VIEW_FOCUS_TRIG_MODE_EDGE,
    // 待补充

    VIEW_FOCUS_MAX
};

/*-----------------------------------*/

// 转换采样值为Y坐标
//#define DSO_AD2Y(ad)    (DSO_VIEW_WAVE_H / 2 - ((int)(ad) - gYOffset) / gYScale)
#define DSO_AD2Y(ad)    ((DSO_VIEW_WAVE_H / 2 * gYScale + gYOffset - (ad) + gYScale / 2) / gYScale)

// 转换X偏移序号为X偏移
#define DSO_XI2XO(idx)  (SFIFO_SIZE / 2 - (idx) * gXScale)

// 转换Y偏移序号为Y偏移
#define DSO_YI2YO(idx)  (DSO_SAMPLE_CENTER + (idx) * gYScale)

// 转换触发电平序号为触发电平
#define DSO_TI2TL(idx)  (DSO_SAMPLE_CENTER - (idx) * gYScale)

/*-----------------------------------*/

// 帧数据缓冲区
static uint16_t gDSOFrameBuf[DSO_VIEW_WAVE_W];

// 波形图形缓冲区，buf[x][0]为Y最小值，buf[x][1]为Y最大值
static uint8_t gDSOWaveBuf[DSO_VIEW_WAVE_W][2];

// 实际的偏移量和缩放倍数
static uint16_t gYOffset;
static uint16_t gXOffset;
static uint16_t gYScale;
static uint16_t gXScale;

// 时基序号
static uint8_t gDSOTimeBaseIdx;

// Y缩放序号
static uint8_t gDSOYScaleIdx;

// X偏移序号
static int8_t gDSOXOffsetIdx;

// Y偏移序号
static int8_t gDSOYOffsetIdx;

// 触发电平序号
static int8_t gDSOTrigLevelIdx;

// 触发模式
static uint8_t gDSOTrigMode;

// 触发边沿
static uint8_t gDSOTrigEdge;

// 显示焦点
static uint8_t gViewFocus;

// 强制更新显示
static BOOL gDSOForceUpdate;

/*-----------------------------------*/

// 绘制水平点线
void DrawHDotLine(uint8_t y, uint8_t gray)
{
    uint8_t x;

    for (x = 1; x < DSO_VIEW_WAVE_W; x += 2) {
        LCD_DrawPoint(x, y, gray);
    }
}

// 绘制竖直点线
void DrawVDotLine(uint8_t x, uint8_t gray)
{
    uint8_t y;

    for (y = 0; y < DSO_VIEW_WAVE_H; y += 2) {
        LCD_DrawPoint(x, y, gray);
    }
}

// 绘制波形区域网格
void DrawGrid(void)
{
    DrawHDotLine(0, 0x1);
    DrawHDotLine(10, 0x1);
    DrawHDotLine(20, 0x1);
    DrawHDotLine(30, 0x1);
    DrawHDotLine(50, 0x1);
    DrawHDotLine(60, 0x1);
    DrawHDotLine(70, 0x1);

    DrawVDotLine(9, 0x1);
    DrawVDotLine(19, 0x1);
    DrawVDotLine(29, 0x1);
    DrawVDotLine(39, 0x1);
    DrawVDotLine(59, 0x1);
    DrawVDotLine(69, 0x1);
    DrawVDotLine(79, 0x1);
    DrawVDotLine(89, 0x1);
    DrawVDotLine(99, 0x1);

    DrawHDotLine(40, 0x2);
    DrawVDotLine(49, 0x2);
}

// 绘制裁剪过的竖直线
void DrawVLine_Clip(int x, int y0, int y1, uint8_t gray)
{
    if (y0 > y1) {
        int temp;

        temp = y1;
        y1 = y0;
        y0 = temp;
    }

    if (y0 >= LCD_HEIGHT || y1 < 0) {
        return;
    }

    if (y1 >= LCD_HEIGHT) {
        y1 = LCD_HEIGHT - 1;
    }

    if (y0 < 0) {
        y0 = 0;
    }

    LCD_DrawVLine(x, (uint8_t)y0, (uint8_t)y1, 0x3);
}

// 创建波形到gDSOWaveBuf
void BuildWave(void)
{
    uint16_t smpcnt;

    smpcnt = Trig_GetSampleCnt();

    if (gXScale == 1) {
        size_t i;
        int curY, lastY = 0;

        // 无缩放时直接连接各点绘制
        SFIFO_ReadDirect(gXOffset - DSO_VIEW_WAVE_W / 2/* + SFIFO_SIZE - smpcnt*/, gDSOFrameBuf, DSO_VIEW_WAVE_W);
        for (i = 0; i < DSO_VIEW_WAVE_W; i++) {
            curY = DSO_AD2Y(gDSOFrameBuf[i]);
            if (i == 0) {
                lastY = curY;
            }
            gDSOWaveBuf[i][WAVE_IDX_MIN] = lastY;
            DrawVLine_Clip(i, lastY, curY, 0x3);
            lastY = curY;
        }
    } else {
        uint16_t smin, smax;
        uint32_t addr;
        size_t i, j;
        int curYmin, curYmax;
        int lastYmin = 0, lastYmax = 0;

        // 有缩放时根据最大值和最小值绘制
        addr = gXOffset - DSO_VIEW_WAVE_W / 2 * gXScale/* + SFIFO_SIZE - smpcnt*/;
        for (i = 0; i < DSO_VIEW_WAVE_W; i++) {
            SFIFO_ReadDirect(addr, gDSOFrameBuf, gXScale);
            smin = SAMPLE_RESULT_MAX;
            smax = SAMPLE_RESULT_MIN;
            for (j = 0; j < gXScale; j++) {
                /*if (gDSOFrameBuf[j] == SAMPLE_RESULT_INVALID) {
                    gDSOFrameBuf[j] = DSO_SAMPLE_CENTER;
                }*/
                if (gDSOFrameBuf[j] < smin) {
                    smin = gDSOFrameBuf[j];
                }
                if (gDSOFrameBuf[j] > smax) {
                    smax = gDSOFrameBuf[j];
                }
            }
            // 由于LCD屏Y轴是向下增长，所以交叉min和max
            curYmin = DSO_AD2Y(smax);
            curYmax = DSO_AD2Y(smin);
            
            if (i != 0) {
                if (lastYmin < 0) {
                    DrawVLine_Clip(i, curYmax, curYmax, 0x3);
                } else if (curYmax < lastYmin) {
                    DrawVLine_Clip(i, curYmin, lastYmin, 0x3);
                } else if (curYmin > lastYmax) {
                    DrawVLine_Clip(i, lastYmax, curYmax, 0x3);
                } else {
                    DrawVLine_Clip(i, curYmin, curYmax, 0x3);
                }
            } else {
                DrawVLine_Clip(i, curYmin, curYmax, 0x3);
            }
            lastYmin = curYmin;
            lastYmax = curYmax;
            addr += gXScale;
        }
    }
}

// 绘制波形
void DrawWave(void)
{
    //uint16_t smpcnt;

    //smpcnt = Trig_GetSampleCnt();

    if (gXScale == 1) {
        size_t i;
        int curY, lastY = 0;

        // 无缩放时直接连接各点绘制
        SFIFO_ReadDirect(gXOffset - DSO_VIEW_WAVE_W / 2/* + SFIFO_SIZE - smpcnt*/, gDSOFrameBuf, DSO_VIEW_WAVE_W);
        for (i = 0; i < DSO_VIEW_WAVE_W; i++) {
            curY = DSO_AD2Y(gDSOFrameBuf[i]);
            if (i == 0) {
                lastY = curY;
            }
            DrawVLine_Clip(i, lastY, curY, 0x3);
            lastY = curY;
        }
    } else {
        uint16_t smin, smax;
        uint32_t addr;
        size_t i, j;
        int curYmin, curYmax;
        int lastYmin = 0, lastYmax = 0;

        // 有缩放时根据最大值和最小值绘制
        addr = gXOffset - DSO_VIEW_WAVE_W / 2 * gXScale/* + SFIFO_SIZE - smpcnt*/;
        for (i = 0; i < DSO_VIEW_WAVE_W; i++) {
            SFIFO_ReadDirect(addr, gDSOFrameBuf, gXScale);
            smin = SAMPLE_RESULT_MAX;
            smax = SAMPLE_RESULT_MIN;
            for (j = 0; j < gXScale; j++) {
                /*if (gDSOFrameBuf[j] == SAMPLE_RESULT_INVALID) {
                    gDSOFrameBuf[j] = DSO_SAMPLE_CENTER;
                }*/
                if (gDSOFrameBuf[j] < smin) {
                    smin = gDSOFrameBuf[j];
                }
                if (gDSOFrameBuf[j] > smax) {
                    smax = gDSOFrameBuf[j];
                }
            }
            // 由于LCD屏Y轴是向下增长，所以交叉min和max
            curYmin = DSO_AD2Y(smax);
            curYmax = DSO_AD2Y(smin);
            
            if (i != 0) {
                if (lastYmin < 0) {
                    DrawVLine_Clip(i, curYmax, curYmax, 0x3);
                } else if (curYmax < lastYmin) {
                    DrawVLine_Clip(i, curYmin, lastYmin, 0x3);
                } else if (curYmin > lastYmax) {
                    DrawVLine_Clip(i, lastYmax, curYmax, 0x3);
                } else {
                    DrawVLine_Clip(i, curYmin, curYmax, 0x3);
                }
            } else {
                DrawVLine_Clip(i, curYmin, curYmax, 0x3);
            }
            lastYmin = curYmin;
            lastYmax = curYmax;
            addr += gXScale;
        }
    }
}

// 绘制设置项
void DrawLabel(void)
{
    const uint8_t disp_x = 102;
    uint8_t scolors[VIEW_FOCUS_MAX] = {0x03, 0x03, 0x03, 0x03, 0x03, 0x03};

    scolors[gViewFocus] = 0x00;
    LCD_FillRect(100, 8 * gViewFocus, 28, 8, 0x3);

    LCD_DrawString(DSO_TB_Strings[gDSOTimeBaseIdx], disp_x, 0, FONT_SIZE_5X8, scolors[0]);
    LCD_DrawString(DSO_YS_Strings[gDSOYScaleIdx], disp_x, 8, FONT_SIZE_5X8, scolors[1]);
    LCD_DrawString("<XO>", disp_x, 16, FONT_SIZE_5X8, scolors[2]);
    LCD_DrawString("<YO>", disp_x, 24, FONT_SIZE_5X8, scolors[3]);
    LCD_DrawString("<TL>", disp_x, 32, FONT_SIZE_5X8, scolors[4]);
    LCD_DrawString(DSO_TrigMode_Strings[gDSOTrigMode], disp_x, 40, FONT_SIZE_5X8, scolors[5]);
    LCD_DrawPicture8_1Bpp(DSO_Trig_EdgeSym[gDSOTrigEdge], disp_x + 6 * 2, 40 / 8, 10, 8 / 8, scolors[5]);
}

// 绘制偏移指示
void DrawOffset(void)
{
    LCD_DrawChar('^', DSO_VIEW_WAVE_W / 2 - 1 + gDSOXOffsetIdx - 2, 76, FONT_SIZE_5X8, 0x03);
    if (gViewFocus == VIEW_FOCUS_XOFFSET) {
        DrawVDotLine(DSO_VIEW_WAVE_W / 2 - 1 + gDSOXOffsetIdx, 0x03);
    }

    LCD_DrawChar('>', 0, DSO_VIEW_WAVE_H / 2 + gDSOYOffsetIdx - 3, FONT_SIZE_5X8, 0x03);
    if (gViewFocus == VIEW_FOCUS_YOFFSET) {
        DrawHDotLine(DSO_VIEW_WAVE_H / 2 + gDSOYOffsetIdx, 0x03);
    }
}

// 绘制触发电平
void DrawTrigLevel(void)
{
    LCD_DrawChar('<', 94, DSO_VIEW_WAVE_H / 2 + gDSOTrigLevelIdx - 3, FONT_SIZE_5X8, 0x03);
    if (gViewFocus == VIEW_FOCUS_TRIG_LEVEL) {
        DrawHDotLine(DSO_VIEW_WAVE_H / 2 + gDSOTrigLevelIdx, 0x03);
    }
}

// 全屏更新
void UpdateFrame(void)
{
    LCD_Clear();
    DrawGrid();
    DrawWave();
    DrawLabel();
    DrawOffset();
    DrawTrigLevel();
    LCD_Refresh();
}

/*-----------------------------------*/

// 更新焦点
void DSO_UpdateFocus(int8_t d)
{
    if (d < 0) {
        if (gViewFocus > 0) {
            gViewFocus--;
        } else {
            gViewFocus = VIEW_FOCUS_MAX - 1;
        }
    } else if (d > 0) {
        if (gViewFocus < VIEW_FOCUS_MAX - 1) {
            gViewFocus++;
        } else {
            gViewFocus = 0;
        }
    } else {
        return;
    }

    LCD_FillRect(100, 0, 28, 80, 0x0);
    DrawLabel();
    LCD_Refresh();
}

// 更新DSO时基
void DSO_UpdateTimeBase(int8_t d)
{
    if (d < 0 && gDSOTimeBaseIdx > 0) {
        gDSOTimeBaseIdx--;
    } else if (d > 0 && gDSOTimeBaseIdx < DSO_TB_COUNT - 1) {
        gDSOTimeBaseIdx++;
    } else {
        return;
    }

    Trig_Stop();
    Sample_SetRate(DSO_SampleRates[gDSOTimeBaseIdx]);
    gXScale = DSO_XScales[gDSOTimeBaseIdx];
    if (gDSOTimeBaseIdx >= DSO_ROLLMODE_TB) {
        // 时基较大时切换为滚动模式，避免长时间不出波形
        gDSOTrigMode = TRIG_MODE_ROLL;
    } else {
        if (gDSOTrigMode == TRIG_MODE_ROLL) {
            // 时基低于可使用滚动模式时切换为自动模式，避免看不清波形
            gDSOTrigMode = TRIG_MODE_AUTO;
        }
    }
    Trig_SetMode(gDSOTrigMode);
    Trig_Start();

    LCD_Clear();
    DrawGrid();
    DrawLabel();
    LCD_Refresh();
}

// 更新垂直缩放
void DSO_UpdateYScale(int8_t d)
{
    if (d < 0 && gDSOYScaleIdx > 0) {
        gDSOYScaleIdx--;
    } else if (d > 0 && gDSOYScaleIdx < DSO_YS_COUNT - 1) {
        gDSOYScaleIdx++;
    } else {
        return;
    }

    gYScale = DSO_YScales[gDSOYScaleIdx];
    UpdateFrame();
}

// 更新DSO X偏移
void DSO_UpdateXOffset(int8_t d)
{
    int idx_new;
    int offset_new;
    //int idx_min, idx_max;
    
    //idx_min = DSO_VIEW_WAVE_W / 2 * gXScale;

    idx_new = gDSOXOffsetIdx + d;

    if (idx_new < -(DSO_VIEW_WAVE_W / 2 - 1)) {
        idx_new = -(DSO_VIEW_WAVE_W / 2 - 1);
    } else if (idx_new > (DSO_VIEW_WAVE_W / 2)) {
        idx_new = (DSO_VIEW_WAVE_W / 2);
    }
    
    gDSOXOffsetIdx = idx_new;

    offset_new = DSO_XI2XO(idx_new);
    if (offset_new >= DSO_VIEW_WAVE_W / 2 * gXScale && offset_new < SFIFO_SIZE) {
        gXOffset = offset_new;
        UpdateFrame();
    }
}

// 更新DSO Y偏移
void DSO_UpdateYOffset(int8_t d)
{
    int idx_new;
    int offset_new;

    idx_new = gDSOYOffsetIdx + d;

    if (idx_new < -(DSO_VIEW_WAVE_H / 2)) {
        idx_new = -(DSO_VIEW_WAVE_H / 2);
    } else if (idx_new > (DSO_VIEW_WAVE_H / 2 - 1)) {
        idx_new = (DSO_VIEW_WAVE_H / 2 - 1);
    }

    gDSOYOffsetIdx = idx_new;

    offset_new = DSO_YI2YO(idx_new);
    if (offset_new >= SAMPLE_RESULT_MIN && offset_new <= SAMPLE_RESULT_MAX) {
        gYOffset = offset_new;
        UpdateFrame();
    }
}

// 更新DSO触发电平
void DSO_UpdateTrigLevel(int8_t d)
{
    int idx_new;
    int level_new;

    idx_new = gDSOTrigLevelIdx + d;

    if (idx_new < -(DSO_VIEW_WAVE_H / 2)) {
        idx_new = -(DSO_VIEW_WAVE_H / 2);
    } else if (idx_new > (DSO_VIEW_WAVE_H / 2 - 1)) {
        idx_new = (DSO_VIEW_WAVE_H / 2 - 1);
    }

    gDSOTrigLevelIdx = idx_new;

    level_new = DSO_TI2TL(idx_new);
    if (level_new >= SAMPLE_RESULT_MIN && level_new <= SAMPLE_RESULT_MAX) {
        Trig_SetLevel(level_new);
        UpdateFrame();
    }
}

// 更新触发模式
void DSO_UpdateTrigMode(void)
{
    gDSOTrigMode++;
    if (gDSOTrigMode >= TRIG_MODE_MAX) {
        if (gDSOTimeBaseIdx >= DSO_ROLLMODE_TB) {
            gDSOTrigMode = TRIG_MODE_ROLL;
        } else {
            gDSOTrigMode = TRIG_MODE_ROLL + 1;
        }
    }
    
    Trig_Stop();
    Trig_SetMode(gDSOTrigMode);
    Trig_Start();
    UpdateFrame();
}

// 更新触发边沿
void DSO_UpdateTrigEdge(void)
{
    gDSOTrigEdge++;
    if (gDSOTrigEdge >= TRIG_EDGE_MAX) {
        gDSOTrigEdge = TRIG_EDGE_RISE;
    }
    
    Trig_SetEdge(gDSOTrigEdge);
    UpdateFrame();
}

/*-----------------------------------*/

// DSO初始化
void DSO_Init(void)
{
    gDSOForceUpdate = FALSE;
    gViewFocus = VIEW_FOCUS_XSCALE;
    gDSOTimeBaseIdx = 6;
    gDSOYScaleIdx = 4;
    gDSOXOffsetIdx = 0;
    gDSOYOffsetIdx = 0;
    gDSOTrigLevelIdx = 0;
    gDSOTrigMode = TRIG_MODE_AUTO;
    gDSOTrigEdge = TRIG_EDGE_RISE;

    gXScale = DSO_XScales[gDSOTimeBaseIdx];
    gYScale = DSO_YScales[gDSOYScaleIdx];
    gXOffset = DSO_XI2XO(gDSOXOffsetIdx);
    gYOffset = DSO_YI2YO(gDSOYOffsetIdx);

    SFIFO_Init();
    Sample_Init();
    Trig_Init();

    Sample_SetRate(DSO_SampleRates[gDSOTimeBaseIdx]);
    Trig_SetMode(gDSOTrigMode);
    Trig_SetEdge(gDSOTrigEdge);
    Trig_SetLevel(DSO_TI2TL(gDSOTrigLevelIdx));
    Trig_SetPreSize(SFIFO_SIZE / 2);
    Trig_Start();
}

// DSO定时任务
void DSO_Exec(void)
{
    if (Trig_CheckUpdate()) {
        UpdateFrame();
        LED_Light(TRUE);
        Trig_ClearUpdate();
    } else {
        LED_Light(FALSE);
    }

    if (gDSOForceUpdate) {
        gDSOForceUpdate = FALSE;
        UpdateFrame();
    }

    Task_Delay(DSO_INTERVAL);
}

// DSO按键事件
void DSO_KeyEvent(key_event_t event, key_code_t key)
{
    switch (event) {
    case KEY_EVENT_REPEAT:
    case KEY_EVENT_RELEASE:
        switch (key) {
        case KEY_CODE_LEFT:
            switch (gViewFocus) {
            case VIEW_FOCUS_XSCALE:
                DSO_UpdateTimeBase(-1);
                break;
            case VIEW_FOCUS_YSCALE:
                DSO_UpdateYScale(-1);
                break;
            case VIEW_FOCUS_XOFFSET:
                if (event == KEY_EVENT_REPEAT) {
                    DSO_UpdateXOffset(-10);
                } else {
                    DSO_UpdateXOffset(-1);
                }
                break;
            case VIEW_FOCUS_YOFFSET:
                if (event == KEY_EVENT_REPEAT) {
                    DSO_UpdateYOffset(-10);
                } else {
                    DSO_UpdateYOffset(-1);
                }
                break;
            case VIEW_FOCUS_TRIG_LEVEL:
                if (event == KEY_EVENT_REPEAT) {
                    DSO_UpdateTrigLevel(-10);
                } else {
                    DSO_UpdateTrigLevel(-1);
                }
                break;
            case VIEW_FOCUS_TRIG_MODE_EDGE:
                DSO_UpdateTrigMode();
                break;
            }
            break;
        case KEY_CODE_RIGHT:
            switch (gViewFocus) {
            case VIEW_FOCUS_XSCALE:
                DSO_UpdateTimeBase(1);
                break;
            case VIEW_FOCUS_YSCALE:
                DSO_UpdateYScale(1);
                break;
            case VIEW_FOCUS_XOFFSET:
                if (event == KEY_EVENT_REPEAT) {
                    DSO_UpdateXOffset(10);
                } else {
                    DSO_UpdateXOffset(1);
                }
                break;
            case VIEW_FOCUS_YOFFSET:
                if (event == KEY_EVENT_REPEAT) {
                    DSO_UpdateYOffset(10);
                } else {
                    DSO_UpdateYOffset(1);
                }
                break;
            case VIEW_FOCUS_TRIG_LEVEL:
                if (event == KEY_EVENT_REPEAT) {
                    DSO_UpdateTrigLevel(10);
                } else {
                    DSO_UpdateTrigLevel(1);
                }
                break;
            case VIEW_FOCUS_TRIG_MODE_EDGE:
                DSO_UpdateTrigEdge();
                break;
            }
            break;
        case KEY_CODE_UP:
            DSO_UpdateFocus(-1);
            break;
        case KEY_CODE_DOWM:
            DSO_UpdateFocus(1);
            break;
        case KEY_CODE_M:
            break;
        }
        break;
    case KEY_EVENT_HOLD:
        break;
    case KEY_EVENT_PRESS:
        break;
    }
}
