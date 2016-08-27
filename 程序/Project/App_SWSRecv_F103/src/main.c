#include "base.h"
#include "timebase.h"
#include "led.h"
#include "lcd.h"
#include "hamm7484.h"
#include "ecc1608.h"
#include <stdio.h>
#include <string.h>

// "ELE“图标(32x32)
// 取模方式：从左到右，从上到下为bit0~bit7
static const uint8_t graphic1[] = {
    0x07, 0xFF, 0xFE, 0x1C, 0x1C, 0x38, 0x70, 0xF0, 0xE0, 0x80, 0x40, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x60, 0xF0, 0xF0, 0x38, 0x18, 0x1C, 0x1C, 0xBE, 0x7E,
    0x00, 0xFF, 0xFF, 0x18, 0x18, 0x38, 0xFE, 0xFE, 0x01, 0x01, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x00, 0x00, 0xFF, 0xFF, 0x18, 0x18, 0x38, 0xFF, 0xFF, 0x00,
    0x80, 0xFF, 0xFF, 0xE0, 0x60, 0x30, 0x38, 0x1C, 0x1F, 0x07, 0x0C, 0x0C, 0x0F, 0x07, 0x07, 0x04,
    0x04, 0x04, 0x06, 0x0E, 0x0F, 0x07, 0x08, 0x18, 0x1F, 0x3F, 0x38, 0x70, 0x60, 0xE0, 0xF0, 0xFC,
    0x0F, 0x3F, 0x3E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E,
    0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x3E, 0x3F, 0x1F
};

// 程序版本
//static const uint32_t version __attribute__((at(0x08005000))) = 0x00010001;

/*-----------------------------------*/

// 软件复位
void SoftReset(void)
{
    __set_FAULTMASK(1);     // 关闭所有中断
    NVIC_SystemReset();     // 复位
}

// 判断是否进入IAP模式
BOOL IAP_Check(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    BOOL isInto = FALSE;

    // 打开按键端口时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    // 初始化KEY4端口(PC15)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    // KEY4按下时为低电平，表示进入IAP
    isInto = !GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_15);

    // 复位按键端口状态
    GPIO_DeInit(GPIOC);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, DISABLE);

    return isInto;
}

/*-----------------------------------*/

// 计算校验和
// 计算规则：带进位累加取反
uint8_t CaclCheckSum(const volatile uint8_t *buf, uint8_t cnt)
{
    uint8_t i;
    uint16_t checksum;

    checksum = 0;
    for (i = 0; i < cnt; i++) {
        checksum += *buf++;
        if (checksum & 0xff00) {
            checksum &= 0xff;
            checksum++;
        }
    }

    return (uint8_t)~checksum;
}

/*-----------------------------------*/

// 帧编码方式定义
//#define SWS_RF_UART
//#define SWS_RF_PPM
#define SWS_RF_SYNC

// 帧特殊序号定义
#define SWS_RF_IDX_RUNCNT       (0)
#define SWS_RF_IDX_BUILDDATE    (1)
#define SWS_RF_IDX_AUTHOR       (2)

#ifdef SWS_RF_UART
// UART模式
// 输入信号经过反相

// 定时器时钟频率
#define SWS_TIMER_FREQ      (1200000UL)
// 标准波特率
#define SWS_BANDRARE_DEF    (1200)
// 标准波特率下每bit定时器计数值
#define SWS_BIT_TIMER_CNT   (uint16_t)(SWS_TIMER_FREQ/SWS_BANDRARE_DEF)

void SWS_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* Enable GPIO Clock */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA , ENABLE);

    /* Configure USART1 Rx as input pull-up */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* Configure USART1 Tx as push-pull output */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 使能接收模块CE
    GPIO_SetBits(GPIOA, GPIO_Pin_9);
}

void SWS_NVIC_Init(void)
{
    NVIC_InitTypeDef NVIC_InitStructure;

    /* Configure the NVIC Preemption Priority Bits */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);

    /* Enable the USART1 Interrupt */
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    /* Enable the TIM1 Capture/Compare Interrupt */
    NVIC_InitStructure.NVIC_IRQChannel = TIM1_CC_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

void SWS_Timer_Init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef  TIM_OCInitStructure;
    TIM_ICInitTypeDef  TIM_ICInitStructure;
    uint16_t PrescalerValue;

    /* TIM1 clock enable */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);

    /* Compute the prescaler value */
    PrescalerValue = (uint16_t)(SystemCoreClock / SWS_TIMER_FREQ) - 1;

    TIM_DeInit(TIM1);

    /* Time base configuration */
    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.TIM_Period = 0xffff;
    TIM_TimeBaseStructure.TIM_Prescaler = 0;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);

    /* Prescaler configuration */
    TIM_PrescalerConfig(TIM1, PrescalerValue, TIM_PSCReloadMode_Immediate);

    /* Output Compare Timing Mode configuration:
       Channel2 (PA.9)
       used to timing break and sync
    */
    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_Timing;
    TIM_OCInitStructure.TIM_Pulse = SWS_BIT_TIMER_CNT;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC2Init(TIM1, &TIM_OCInitStructure);
    TIM_OC2PreloadConfig(TIM1, TIM_OCPreload_Disable);

    /* Input Capture mode configuration:
       Channel3 (PA.10)
       The Falling edge is used as active edge,
       used to compute the bandrate
    */
    TIM_ICStructInit(&TIM_ICInitStructure);
    TIM_ICInitStructure.TIM_Channel = TIM_Channel_3;
    TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Falling;
    TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
    TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    TIM_ICInitStructure.TIM_ICFilter = 0xF; // 1200000/32/8=4687.5Hz
    TIM_ICInit(TIM1, &TIM_ICInitStructure);

    /* Enable the CC2 and CC3 Interrupt Request */
    TIM_ITConfig(TIM1, TIM_IT_CC2 | TIM_IT_CC3, ENABLE);

    /* TIM enable counter */
    //TIM_Cmd(TIM1, ENABLE);
}

void SWS_UART_Config(uint16_t band)
{
    USART_InitTypeDef USART_InitStructure;

    /* Enable USART1 Clock */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

    USART_DeInit(USART1);

    /* USART1 configured as follow:
        - BaudRate = 1200 baud
        - Word Length = 8 Bits
        - One Stop Bit
        - No parity
        - Hardware flow control disabled (RTS and CTS signals)
        - Receive enabled
    */
    USART_InitStructure.USART_BaudRate = band;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx;
    USART_Init(USART1, &USART_InitStructure);

    /* Enable USART1 Receive interrupts */
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

    /* Enable the USART1 */
    //USART_Cmd(USART1, ENABLE);
}

/*-----------------------------------*/

// 数据帧最大字节数
#define SWS_FRAME_SIZE_MAX      (15)
// 波特率允许误差
#define SWS_BANDRATE_DIFF_P     (0.1)   // 0.1即波特率误差+-10%
// 同步字每2bits定时器计数最小和最大值，用于波特率同步
#define SWS_CAP_CNT_MIN         (uint16_t)(SWS_TIMER_FREQ*2/(SWS_BANDRARE_DEF*(1+SWS_BANDRATE_DIFF_P)))
#define SWS_CAP_CNT_MAX         (uint16_t)(SWS_TIMER_FREQ*2/(SWS_BANDRARE_DEF*(1-SWS_BANDRATE_DIFF_P)))
// 同步字每2bits间定时器计数值标准离差率平方的缩放系数和最大值，用于避免错误同步
#define SWS_CAP_COV2_SCALE      (10000)
#define SWS_CAP_COV2_MAX        (16)    // 16/10000=0.0016对应偏移4%
// 引导电平最小标准比特数
#define SWS_BREAK_CNT_MIN       (11)
// 同步字最大标准比特数，用于同步字超时判断
#define SWS_SYNC_CNT_MAX        (9)
// 帧接收超时时间(ms)
#define SWS_FRAME_TIMEOUT       (200)

// 接收状态枚举
typedef enum {
    SWS_RECV_BREAK,         // 引导电平和空闲状态
    SWS_RECV_SYNC_PRE,      // 同步开始
    SWS_RECV_SYNC,          // 同步过程中
    SWS_RECV_SYNC_END,      // 同步成功
    SWS_RECV_DATA,          // 接收数据
    SWS_RECV_DATA_END       // 接收完成
} SWS_State_t;

// 错误码枚举
typedef enum {
    ERR_CODE_NONE,
    ERR_CODE_SYNC_TIMEOUT,
    ERR_CODE_SYNC_AVG,
    ERR_CODE_SYNC_COV,
    ERR_CODE_RECV_TIMEOUT,
    ERR_CODE_MAX
} SWS_Error_t;

// 接收状态
static volatile SWS_State_t gSWSState;
// 接收波特率
static volatile uint16_t gSWSBandrate;
// 帧缓存
static volatile uint8_t gSWSRecvBuf[SWS_FRAME_SIZE_MAX + 1];
// 帧字节计数
static volatile uint8_t gSWSRecvCnt;
// 帧大小
static volatile uint8_t gSWSFrameSize;
// 错误码
static volatile uint16_t gSWSErrCode[ERR_CODE_MAX];

void SWS_Recv(void)
{
    uint32_t timelast;
    char strbuf[32] = "";
    uint8_t len;
    uint8_t checksum;
    uint8_t checksum_recv;
    uint32_t total_ok = 0, total_bad = 0, total_last = 0;
    uint8_t idx_last;
    uint16_t cnt_h;
    uint8_t has_cnt_h = 0;

    SWS_NVIC_Init();
    SWS_GPIO_Init();
    SWS_Timer_Init();
    SWS_UART_Config(1200);

    LCD_Clear();
    LCD_DrawString("Solar Wireless Sensor", 0, 0, FONT_SIZE_5X8, 0x3);
    LCD_DrawString(" Reciever by eleqian ", 0, 8, FONT_SIZE_5X8, 0x3);
    LCD_DrawPicture8_1Bpp(graphic1, 48, 24 / 8, 32, 32 / 8, 0x3);
    LCD_DrawString("      Waiting...     ", 0, 64, FONT_SIZE_5X8, 0x3);
    LCD_Refresh();

    gSWSRecvBuf[SWS_FRAME_SIZE_MAX] = 0;

    while (1) {
        // 引导电平和同步字处理
        gSWSState = SWS_RECV_BREAK;
        TIM_Cmd(TIM1, ENABLE);
        while (SWS_RECV_SYNC_END != gSWSState) {
        }
        TIM_Cmd(TIM1, DISABLE);
        // 等待同步字停止位
        while (!GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_10)) {
        }
        // 配置UART波特率并启动接收
        SWS_UART_Config(gSWSBandrate);
        gSWSRecvCnt = 0;
        gSWSFrameSize = SWS_FRAME_SIZE_MAX;
        gSWSState = SWS_RECV_DATA;
        USART_Cmd(USART1, ENABLE);
        timelast = millis();
        while (SWS_RECV_DATA_END != gSWSState) {
            if (millis() - timelast >= SWS_FRAME_TIMEOUT) {
                gSWSErrCode[ERR_CODE_RECV_TIMEOUT]++;
                gSWSState = SWS_RECV_DATA_END;
                break;
            }
        }
        USART_Cmd(USART1, DISABLE);
        // 接收完成进行校验
        checksum = CaclCheckSum(gSWSRecvBuf, gSWSRecvCnt - 1);
        checksum_recv = gSWSRecvBuf[gSWSRecvCnt - 1];
        // 显示
        LCD_Clear();
        sprintf(strbuf, "ID: 0x%02x  Band: %4d", gSWSRecvBuf[0], gSWSBandrate);
        LCD_DrawString(strbuf, 0, 0, FONT_SIZE_5X8, 0x3);
        len = sprintf(strbuf, "Index: %d", gSWSRecvBuf[1]);
        if (has_cnt_h) {
            sprintf(strbuf + len, " (0x%04x)", cnt_h);
        }
        LCD_DrawString(strbuf, 0, 8, FONT_SIZE_5X8, 0x3);
        switch (gSWSRecvBuf[1]) {
        case 0:
            if (checksum == checksum_recv) {
                has_cnt_h = 1;
                cnt_h = (gSWSRecvBuf[2] << 8) | gSWSRecvBuf[3];
            }
            sprintf(strbuf, "Cnt High: 0x%04x", (gSWSRecvBuf[2] << 8) | gSWSRecvBuf[3]);
            LCD_DrawString(strbuf, 0, 16, FONT_SIZE_5X8, 0x3);
            break;
        case 1:
            sprintf(strbuf, "Sys Date: %02x%02x.%02x.%02x", gSWSRecvBuf[2], gSWSRecvBuf[3], gSWSRecvBuf[4], gSWSRecvBuf[5]);
            LCD_DrawString(strbuf, 0, 16, FONT_SIZE_5X8, 0x3);
            break;
        case 2:
            len = sprintf(strbuf, "Designer: ");
            memcpy(strbuf + len, (void *)&gSWSRecvBuf[2], gSWSFrameSize - 3);
            strbuf[len + gSWSFrameSize - 3] = '\0';
            LCD_DrawString(strbuf, 0, 16, FONT_SIZE_5X8, 0x3);
            break;
        default:
            sprintf(strbuf, "Temp: %d", (gSWSRecvBuf[2] << 4) | ((gSWSRecvBuf[3] & 0xf0) >> 4));
            LCD_DrawString(strbuf, 0, 16, FONT_SIZE_5X8, 0x3);
            sprintf(strbuf, "Light: %d", ((gSWSRecvBuf[3] & 0x0f) << 8) | gSWSRecvBuf[4]);
            LCD_DrawString(strbuf, 0, 24, FONT_SIZE_5X8, 0x3);
            break;
        }
        sprintf(strbuf, "CheckSum: 0x%02x %s", checksum_recv, (checksum == checksum_recv ? "OK" : "BAD"));
        LCD_DrawString(strbuf, 0, 32, FONT_SIZE_5X8, 0x3);
        if (checksum == checksum_recv) {
            if (total_ok > 0 && (uint8_t)(idx_last + 1) != gSWSRecvBuf[1]) {
                total_last += (uint8_t)(gSWSRecvBuf[1] - idx_last - 1);
            }
            idx_last = gSWSRecvBuf[1];
            total_ok++;
        } else {
            total_bad++;
        }
        sprintf(strbuf, "Total: %d OK", total_ok);
        LCD_DrawString(strbuf, 0, 40, FONT_SIZE_5X8, 0x3);
        sprintf(strbuf, "  %d BAD, %d LAST", total_bad, total_last);
        LCD_DrawString(strbuf, 0, 48, FONT_SIZE_5X8, 0x3);
        LCD_DrawString("  Err1 Err2 Err3 Err4", 0, 64, FONT_SIZE_5X8, 0x3);
        sprintf(strbuf, "0x%04x %04x %04x %04x", gSWSErrCode[1], gSWSErrCode[2], gSWSErrCode[3], gSWSErrCode[4]);
        LCD_DrawString(strbuf, 0, 72, FONT_SIZE_5X8, 0x3);
        LCD_Refresh();
    }
}

void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
        gSWSRecvBuf[gSWSRecvCnt++] = USART_ReceiveData(USART1);
        if (gSWSRecvCnt == 2) {
            // 根据序号判断帧大小
            switch (gSWSRecvBuf[1]) {
            case 0:
                gSWSFrameSize = 5;
                break;
            case 1:
                gSWSFrameSize = 7;
                break;
            case 2:
                gSWSFrameSize = 10;
                break;
            default:
                gSWSFrameSize = 6;
                break;
            }
        } else if (gSWSRecvCnt >= gSWSFrameSize) {
            gSWSState = SWS_RECV_DATA_END;
        }
    }
}

void TIM1_CC_IRQHandler(void)
{
    static uint8_t breakcnt = 0;
    static uint8_t synccnt = 0;
    static uint8_t capcnt = 0;
    static uint16_t caplast = 0;
    static uint16_t capdiff[4] = {0};
    uint16_t ccvalue;
    uint8_t iocnt;

    if (TIM_GetITStatus(TIM1, TIM_IT_CC2) != RESET) {
        TIM_ClearITPendingBit(TIM1, TIM_IT_CC2);
        if (SWS_RECV_BREAK == gSWSState) {
            iocnt = 0;
            if (!GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_10)) {
                iocnt++;
            }
            if (!GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_10)) {
                iocnt++;
            }
            if (!GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_10)) {
                iocnt++;
            }
            // 判断引导电平
            if (iocnt >= 2) {
                breakcnt++;
                if (breakcnt >= SWS_BREAK_CNT_MIN) {
                    breakcnt = 0;
                    gSWSState = SWS_RECV_SYNC_PRE;
                }
            } else {
                breakcnt = 0;
            }
        } else if (SWS_RECV_SYNC == gSWSState) {
            synccnt++;
            if (synccnt >= SWS_SYNC_CNT_MAX) {
                // 同步字超时则取消同步
                TIM_ClearITPendingBit(TIM1, TIM_IT_CC3);
                gSWSErrCode[ERR_CODE_SYNC_TIMEOUT]++;
                gSWSState = SWS_RECV_BREAK;
            }
        }
        ccvalue = TIM_GetCapture2(TIM1);
        TIM_SetCompare2(TIM1, ccvalue + SWS_BIT_TIMER_CNT);
    } else if (TIM_GetITStatus(TIM1, TIM_IT_CC3) != RESET) {
        /* Clear TIM1 Capture compare interrupt pending bit */
        TIM_ClearITPendingBit(TIM1, TIM_IT_CC3);
        /* Get the Input Capture value */
        ccvalue = TIM_GetCapture3(TIM1);
        if (SWS_RECV_SYNC_PRE == gSWSState) {
            capcnt = 0;
            caplast = ccvalue;
            // 位计时同步，用于判断同步字超时
            synccnt = 0;
            TIM_SetCompare2(TIM1, ccvalue + SWS_BIT_TIMER_CNT);
            TIM_ClearITPendingBit(TIM1, TIM_IT_CC2);
            gSWSState = SWS_RECV_SYNC;
        } else if (SWS_RECV_SYNC == gSWSState) {
            capdiff[capcnt++] = ccvalue - caplast;
            caplast = ccvalue;
            // 4次下降沿捕获包括起始位和前7位数据
            if (capcnt >= 4) {
                uint16_t capavg;
                uint32_t cap2avg;
                uint32_t capcov2;

                // 平均值
                capavg = (capdiff[0] + capdiff[1] + capdiff[2] + capdiff[3]) / 4;
                // 平均值在误差允许范围内
                if (capavg >= SWS_CAP_CNT_MIN && capavg <= SWS_CAP_CNT_MAX) {
                    // 平方的平均值
                    cap2avg = (capdiff[0] * capdiff[0] + capdiff[1] * capdiff[1] + capdiff[2] * capdiff[2] + capdiff[3] * capdiff[3]) / 4;
                    // 标准偏离率的平方*N倍, V^2=o^2/E^2=(E(X^2)-E(X)^2)/E(X)^2=(E(X^2)/E(X)^2)-1
                    capcov2 = cap2avg / capavg * SWS_CAP_COV2_SCALE / capavg - SWS_CAP_COV2_SCALE;
                    // 偏移率在误差允许范围内
                    if (capcov2 <= SWS_CAP_COV2_MAX) {
                        // 计算波特率并进行UART接收
                        gSWSBandrate = SWS_TIMER_FREQ * 2 / capavg;
                        gSWSState = SWS_RECV_SYNC_END;
                    } else {
                        gSWSErrCode[ERR_CODE_SYNC_COV]++;
                        gSWSState = SWS_RECV_BREAK;
                    }
                } else {
                    gSWSErrCode[ERR_CODE_SYNC_AVG]++;
                    gSWSState = SWS_RECV_BREAK;
                }
            }
        }
    }
}

#endif //SWS_RF_UART

#ifdef SWS_RF_PPM
// 脉冲位置调制方式
// 输入信号经过反相

// 定时器时钟频率
#define SWS_TIMER_FREQ      (1000000UL)
// 标准时钟下每个脉冲定时器计数值
#define SWS_PLUSE_PERIOD    (2000)

void SWS_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* Enable GPIO Clock */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA , ENABLE);

    /* Configure Rx(PA.10) as input pull-up */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* Configure CE(PA.9) as push-pull output */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 使能接收模块CE
    GPIO_SetBits(GPIOA, GPIO_Pin_9);
}

void SWS_NVIC_Init(void)
{
    NVIC_InitTypeDef NVIC_InitStructure;

    /* Configure the NVIC Preemption Priority Bits */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);

    /* Enable the TIM1 Capture/Compare Interrupt */
    NVIC_InitStructure.NVIC_IRQChannel = TIM1_CC_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

void SWS_Timer_Init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;
    TIM_ICInitTypeDef TIM_ICInitStructure;
    uint16_t PrescalerValue;

    /* TIM1 clock enable */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);

    /* Compute the prescaler value */
    PrescalerValue = (uint16_t)(SystemCoreClock / SWS_TIMER_FREQ) - 1;

    TIM_DeInit(TIM1);

    /* Time base configuration */
    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.TIM_Period = 0xffff;
    TIM_TimeBaseStructure.TIM_Prescaler = 0;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);

    /* Prescaler configuration */
    TIM_PrescalerConfig(TIM1, PrescalerValue, TIM_PSCReloadMode_Immediate);

    /* Output Compare Timing Mode configuration:
       Channel2 (PA.9)
       used to detect pluse timeout
    */
    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_Timing;
    TIM_OCInitStructure.TIM_Pulse = SWS_PLUSE_PERIOD;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC2Init(TIM1, &TIM_OCInitStructure);
    TIM_OC2PreloadConfig(TIM1, TIM_OCPreload_Disable);

    /* Input Capture mode configuration:
       Channel3 (PA.10)
       The Falling edge is used as active edge
    */
    TIM_ICStructInit(&TIM_ICInitStructure);
    TIM_ICInitStructure.TIM_Channel = TIM_Channel_3;
    TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Falling;
    TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
    TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    TIM_ICInitStructure.TIM_ICFilter = 0xd; // 1000000/32/5=6250Hz
    TIM_ICInit(TIM1, &TIM_ICInitStructure);

    /* Input Capture mode configuration:
       Channel4 (PA.10)
       The Rising edge is used as active edge
    */
    TIM_ICStructInit(&TIM_ICInitStructure);
    TIM_ICInitStructure.TIM_Channel = TIM_Channel_4;
    TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;
    TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_IndirectTI;
    TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    TIM_ICInitStructure.TIM_ICFilter = 0xd; // 1000000/32/5=6250Hz
    TIM_ICInit(TIM1, &TIM_ICInitStructure);

    /* Enable the CC2, CC3 and CC4 Interrupt Request */
    TIM_ITConfig(TIM1, TIM_IT_CC2 | TIM_IT_CC3 | TIM_IT_CC4, ENABLE);

    /* TIM enable counter */
    //TIM_Cmd(TIM1, ENABLE);
}

/*-----------------------------------*/

// 数据帧最大字节数
#define SWS_FRAME_SIZE_MAX      (16)
// 帧接收超时时间(ms)
#define SWS_FRAME_TIMEOUT       (200)

// 脉冲周期最小和最大值
#define SWS_PLUSE_PERIOD_MIN    ((uint16_t)(SWS_PLUSE_PERIOD * 0.95))
#define SWS_PLUSE_PERIOD_MAX    ((uint16_t)(SWS_PLUSE_PERIOD * 1.05))
// 同步脉冲序列参数
#define SWS_PLUSE_SYNC0_MIN     ((uint16_t)(SWS_PLUSE_PERIOD_MIN * 1.0))
#define SWS_PLUSE_SYNC0_MAX     ((uint16_t)(SWS_PLUSE_PERIOD_MAX * 1.0))
#define SWS_PLUSE_SYNC1_MIN     ((uint16_t)(SWS_PLUSE_PERIOD_MIN * 0.9))
#define SWS_PLUSE_SYNC1_MAX     ((uint16_t)(SWS_PLUSE_PERIOD_MAX * 0.9))
#define SWS_PLUSE_SYNC2_MIN     ((uint16_t)(SWS_PLUSE_PERIOD_MIN * 0.8))
#define SWS_PLUSE_SYNC2_MAX     ((uint16_t)(SWS_PLUSE_PERIOD_MAX * 0.8))
#define SWS_PLUSE_SYNC_AVG(a)   ((uint16_t)((uint32_t)(a) * 10 / 27))
// 辅助同步时钟参数
#define SWS_PLUSE_SYNCA_MIN     (50-4)
#define SWS_PLUSE_SYNCA_MAX     (50+4)
#define SWS_PLUSE_SYNCA_SCALE   (100)
#define SWS_PLUSE_SYNCA_RATE    (2)

// 接收状态枚举
typedef enum {
    SWS_RECV_SYNC,          // 时钟同步
    SWS_RECV_SYNC_AUX,      // 辅助时钟同步
    SWS_RECV_DATA,          // 接收数据
    SWS_RECV_DATA_END       // 接收完成
} SWS_State_t;

// 错误码枚举
typedef enum {
    ERR_CODE_NONE,
    ERR_CODE_PLUSE_TIMEOUT,
    ERR_CODE_PLUSE_MULTI,
    ERR_CODE_PLUSE_INVALID,
    ERR_CODE_SYNCA_INVALID,
    ERR_CODE_MAX
} SWS_Error_t;

// 接收状态
static volatile SWS_State_t gSWSState;
// 实际脉冲周期
static volatile uint16_t gSWSPlusePeriod;
// 帧缓存
static volatile uint8_t gSWSRecvBuf[SWS_FRAME_SIZE_MAX];
// 帧字节计数
static volatile uint8_t gSWSRecvCnt;
// 帧大小
static volatile uint8_t gSWSFrameSize;
// 错误码
static volatile uint16_t gSWSErrCode[ERR_CODE_MAX];
// 错误帧
static uint8_t gSWSRecvBufErr[SWS_FRAME_SIZE_MAX];

// 对7个脉冲进行反交织解码为3个(7,4)汉明码
// 交织顺序为从上到下、从左到右
static void DeInterPluse2Hamm(uint8_t *hbuf, const uint16_t *pbuf)
{
    uint8_t i;
    uint8_t idx;

    hbuf[0] = 0;
    hbuf[1] = 0;
    hbuf[2] = 0;
    for (i = 0; i < 7; i++) {
        idx = ((uint32_t)pbuf[i] * 10 + gSWSPlusePeriod / 2) / gSWSPlusePeriod;
        if (idx > 0) {
            idx--;
        } else {
            gSWSErrCode[ERR_CODE_PLUSE_INVALID]++;
        }
        if (idx > 7) {
            idx = 7;
        }
        hbuf[0] = (hbuf[0] << 1) | ((idx >> 2) & 0x1);
        hbuf[1] = (hbuf[1] << 1) | ((idx >> 1) & 0x1);
        hbuf[2] = (hbuf[2] << 1) | ((idx >> 0) & 0x1);
    }
}

void SWS_Recv(void)
{
    char strbuf[32] = "";
    uint8_t len;
    uint8_t checksum;
    uint8_t checksum_recv;
    uint32_t total_ok = 0, total_bad = 0, total_last = 0;
    uint8_t idx_last;
    uint16_t cnt_h;
    uint8_t has_cnt_h = 0;

    SWS_NVIC_Init();
    SWS_GPIO_Init();
    SWS_Timer_Init();

    LCD_Clear();
    LCD_DrawString("Solar Wireless Sensor", 0, 0, FONT_SIZE_5X8, 0x3);
    LCD_DrawString(" Reciever by eleqian ", 0, 8, FONT_SIZE_5X8, 0x3);
    LCD_DrawPicture8_1Bpp(graphic1, 48, 24 / 8, 32, 32 / 8, 0x3);
    LCD_DrawString("      Waiting...     ", 0, 64, FONT_SIZE_5X8, 0x3);
    LCD_Refresh();

    TIM_Cmd(TIM1, ENABLE);

    while (1) {
        // 引导电平和同步字处理
        gSWSPlusePeriod = SWS_PLUSE_PERIOD;
        gSWSFrameSize = SWS_FRAME_SIZE_MAX;
        gSWSRecvCnt = 0;
        gSWSState = SWS_RECV_SYNC;
        while (SWS_RECV_DATA != gSWSState);
        while (SWS_RECV_DATA_END != gSWSState);
        // 接收完成进行校验
        checksum = CaclCheckSum(gSWSRecvBuf, gSWSRecvCnt - 1);
        checksum_recv = gSWSRecvBuf[gSWSRecvCnt - 1];
        // 显示
        LCD_Clear();
        sprintf(strbuf, "ID: 0x%02x  T: %4d", gSWSRecvBuf[0], gSWSPlusePeriod);
        LCD_DrawString(strbuf, 0, 0, FONT_SIZE_5X8, 0x3);
        len = sprintf(strbuf, "Index: %d", gSWSRecvBuf[1]);
        if (has_cnt_h) {
            sprintf(strbuf + len, " (0x%04x)", cnt_h);
        }
        LCD_DrawString(strbuf, 0, 8, FONT_SIZE_5X8, 0x3);
        switch (gSWSRecvBuf[1]) {
        case 0:
            if (checksum == checksum_recv) {
                has_cnt_h = 1;
                cnt_h = (gSWSRecvBuf[2] << 8) | gSWSRecvBuf[3];
            }
            sprintf(strbuf, "Cnt High: 0x%04x", (gSWSRecvBuf[2] << 8) | gSWSRecvBuf[3]);
            LCD_DrawString(strbuf, 0, 16, FONT_SIZE_5X8, 0x3);
            break;
        case 1:
            sprintf(strbuf, "Sys Date: %02x%02x.%02x.%02x", gSWSRecvBuf[2], gSWSRecvBuf[3], gSWSRecvBuf[4], gSWSRecvBuf[5]);
            LCD_DrawString(strbuf, 0, 16, FONT_SIZE_5X8, 0x3);
            break;
        case 2:
            len = sprintf(strbuf, "Designer: ");
            memcpy(strbuf + len, (void *)&gSWSRecvBuf[2], gSWSFrameSize - 3);
            strbuf[len + gSWSFrameSize - 3] = '\0';
            LCD_DrawString(strbuf, 0, 16, FONT_SIZE_5X8, 0x3);
            break;
        default:
            sprintf(strbuf, "Temp: %d", (gSWSRecvBuf[2] << 4) | ((gSWSRecvBuf[3] & 0xf0) >> 4));
            LCD_DrawString(strbuf, 0, 16, FONT_SIZE_5X8, 0x3);
            sprintf(strbuf, "Light: %d  U: 0x%02x", ((gSWSRecvBuf[3] & 0x0f) << 8) | gSWSRecvBuf[4], gSWSRecvBuf[5]);
            LCD_DrawString(strbuf, 0, 24, FONT_SIZE_5X8, 0x3);
            break;
        }
        sprintf(strbuf, "CheckSum: 0x%02x %s", checksum_recv, (checksum == checksum_recv ? "OK" : "BAD"));
        LCD_DrawString(strbuf, 0, 32, FONT_SIZE_5X8, 0x3);
        if (checksum == checksum_recv) {
            if (total_ok > 0 && (uint8_t)(idx_last + 1) != gSWSRecvBuf[1]) {
                //total_last += (uint8_t)(gSWSRecvBuf[1] - idx_last - 1);
            }
            idx_last = gSWSRecvBuf[1];
            total_ok++;
        } else {
            memcpy(gSWSRecvBufErr, (uint8_t*)gSWSRecvBuf, SWS_FRAME_SIZE_MAX);
            total_bad++;
        }
        sprintf(strbuf, "Total: %d OK", total_ok);
        LCD_DrawString(strbuf, 0, 40, FONT_SIZE_5X8, 0x3);
        sprintf(strbuf, "  %d BAD, %d LAST", total_bad, total_last);
        LCD_DrawString(strbuf, 0, 48, FONT_SIZE_5X8, 0x3);
        sprintf(strbuf, "%02x %02x %02x %02x %02x %02x %02x", gSWSRecvBufErr[0], gSWSRecvBufErr[1], gSWSRecvBufErr[2], gSWSRecvBufErr[3],
            gSWSRecvBufErr[4], gSWSRecvBufErr[5], gSWSRecvBufErr[6]);
        LCD_DrawString(strbuf, 0, 56, FONT_SIZE_5X8, 0x3);
        LCD_DrawString("  Err1 Err2 Err3 Err4", 0, 64, FONT_SIZE_5X8, 0x3);
        sprintf(strbuf, "0x%04x %04x %04x %04x", gSWSErrCode[1], gSWSErrCode[2], gSWSErrCode[3], gSWSErrCode[4]);
        LCD_DrawString(strbuf, 0, 72, FONT_SIZE_5X8, 0x3);
        LCD_Refresh();
    }
}

/*-----------------------------------*/

void TIM1_CC_IRQHandler(void)
{
    static uint16_t lastfall = 0;
    static uint16_t lastrise = 0;
    static uint16_t capbase = 0;
    static uint16_t capdiff[7] = {0};
    static uint8_t capcnt = 0;
    static BOOL ispluseok = FALSE;
    static BOOL ishammleft = FALSE;
    static uint8_t hammlast = 0;
    uint8_t hamm[3];
    uint16_t ccvalue;

    if (TIM_GetITStatus(TIM1, TIM_IT_CC2) != RESET) {
        /* Clear TIM1 Capture compare interrupt pending bit */
        TIM_ClearITPendingBit(TIM1, TIM_IT_CC2);
        // 脉冲开始时刻
        capbase = TIM_GetCapture2(TIM1);
        // 下次脉冲开始时刻
        TIM_SetCompare2(TIM1, capbase + gSWSPlusePeriod);

        if (SWS_RECV_DATA == gSWSState) {
            // 脉冲超时假设为最大值，如果可能将由FEC纠正
            if (!ispluseok) {
                capdiff[capcnt++] = gSWSPlusePeriod;
                gSWSErrCode[ERR_CODE_PLUSE_TIMEOUT]++;
            }
            // 每7个脉冲为一组数据
            if (capcnt >= 7) {
                capcnt = 0;

                // 解交织
                DeInterPluse2Hamm(hamm, capdiff);
                // 解码汉明码为数据
                if (!ishammleft) {
                    gSWSRecvBuf[gSWSRecvCnt++] = (Decode_74(hamm[0]) << 4) | Decode_74(hamm[1]);
                    hammlast = hamm[2];
                    ishammleft = TRUE;
                } else {
                    gSWSRecvBuf[gSWSRecvCnt++] = (Decode_74(hammlast) << 4) | Decode_74(hamm[0]);
                    gSWSRecvBuf[gSWSRecvCnt++] = (Decode_74(hamm[1]) << 4) | Decode_74(hamm[2]);
                    ishammleft = FALSE;
                }
                
                // 进入辅助时钟同步状态
                gSWSState = SWS_RECV_SYNC_AUX;

                if (gSWSRecvCnt >= gSWSFrameSize) {
                    // 根据帧大小判断接收完成
                    gSWSState = SWS_RECV_DATA_END;
                } else if (gSWSRecvCnt >= 2) {
                    // 根据序号判断帧大小
                    switch (gSWSRecvBuf[1]) {
                    case 0:
                        gSWSFrameSize = 7;
                        break;
                    case 1:
                        gSWSFrameSize = 7;
                        break;
                    case 2:
                        gSWSFrameSize = 10;
                        break;
                    default:
                        gSWSFrameSize = 7;
                        break;
                    }
                }
            }
            // 允许继续接收脉冲
            TIM_ClearITPendingBit(TIM1, TIM_IT_CC3);
            ispluseok = FALSE;
        } else if (SWS_RECV_SYNC_AUX == gSWSState) {
            if (!ispluseok) {
                gSWSErrCode[ERR_CODE_SYNCA_INVALID]++;
            }
            ispluseok = FALSE;
            // 辅助同步时钟不管成功失败都继续接收
            gSWSState = SWS_RECV_DATA;
        }
    } else if (TIM_GetITStatus(TIM1, TIM_IT_CC3) != RESET) {
        /* Falling edge */
        /* Clear TIM1 Capture compare interrupt pending bit */
        TIM_ClearITPendingBit(TIM1, TIM_IT_CC3);
        /* Get the Input Capture value */
        ccvalue = TIM_GetCapture3(TIM1);
        if (SWS_RECV_SYNC == gSWSState) {
            // 以引导序列最后一个下降沿为0时刻
            lastfall = ccvalue;
        } else if (SWS_RECV_SYNC_AUX == gSWSState) {
            // 辅助同步时钟
            if ((uint16_t)(ccvalue - capbase) >= gSWSPlusePeriod * SWS_PLUSE_SYNCA_MIN / SWS_PLUSE_SYNCA_SCALE && 
                (uint16_t)(ccvalue - capbase) <= gSWSPlusePeriod * SWS_PLUSE_SYNCA_MAX / SWS_PLUSE_SYNCA_SCALE) {
                // 下次脉冲开始时刻
                TIM_SetCompare2(TIM1, capbase + (ccvalue - capbase) * SWS_PLUSE_SYNCA_RATE);
                TIM_ClearITPendingBit(TIM1, TIM_IT_CC2);
                ispluseok = TRUE;
            }
        } else if (SWS_RECV_DATA == gSWSState) {
            if (!ispluseok) {
                // 在每个脉冲周期内只识别第一个脉冲
                capdiff[capcnt++] = ccvalue - capbase;
                ispluseok = TRUE;
            } else {
                gSWSErrCode[ERR_CODE_PLUSE_MULTI]++;
            }
        }
    } else if (TIM_GetITStatus(TIM1, TIM_IT_CC4) != RESET) {
        /* Rising edge */
        /* Clear TIM1 Capture compare interrupt pending bit */
        TIM_ClearITPendingBit(TIM1, TIM_IT_CC4);
        /* Get the Input Capture value */
        ccvalue = TIM_GetCapture4(TIM1);
        if (SWS_RECV_SYNC == gSWSState) {
            capdiff[0] = capdiff[1];
            capdiff[1] = capdiff[2];
            capdiff[2] = ccvalue - lastrise;
            lastrise = ccvalue;
            // 检测引导序列
            if (capdiff[0] >= SWS_PLUSE_SYNC0_MIN && capdiff[0] <= SWS_PLUSE_SYNC0_MAX &&
                capdiff[1] >= SWS_PLUSE_SYNC1_MIN && capdiff[1] <= SWS_PLUSE_SYNC1_MAX &&
                capdiff[2] >= SWS_PLUSE_SYNC2_MIN && capdiff[2] <= SWS_PLUSE_SYNC2_MAX) {
                // 计算接收脉冲周期
                gSWSPlusePeriod = SWS_PLUSE_SYNC_AVG(capdiff[0] + capdiff[1] + capdiff[2]);
                // 配置下个脉冲周期计时
                capbase = lastfall + gSWSPlusePeriod;
                TIM_SetCompare2(TIM1, capbase + gSWSPlusePeriod);
                TIM_ClearITPendingBit(TIM1, TIM_IT_CC2);
                // 开始接收数据
                capcnt = 0;
                ishammleft = FALSE;
                ispluseok = FALSE;
                gSWSState = SWS_RECV_DATA;
            }
        }
    }
}

#endif //SWS_RF_PPM

/*-----------------------------------*/

#ifdef SWS_RF_SYNC
// 同步通信模式
// 输入信号为同相

// 调试模式
#define SWS_DEBUG

// 定时器时钟频率
#define SWS_TIMER_FREQ          (2400000UL)
// 标准波特率
#define SWS_BANDRARE_DEF        (2400U)
// 标准波特率下每bit定时器计数值
#define SWS_BIT_TIMER_CNT       (uint16_t)(SWS_TIMER_FREQ/SWS_BANDRARE_DEF)
// 波特率允许误差(误差+-5%)
#define SWS_BANDRATE_DIFF_P     (0.05)
// 引导序列每2bits定时器计数最小和最大值，用于波特率同步
#define SWS_CAP_CNT_MIN         (uint16_t)(SWS_TIMER_FREQ*2/(SWS_BANDRARE_DEF*(1+SWS_BANDRATE_DIFF_P)))
#define SWS_CAP_CNT_MAX         (uint16_t)(SWS_TIMER_FREQ*2/(SWS_BANDRARE_DEF*(1-SWS_BANDRATE_DIFF_P)))
// 引导序列每2bits间定时器计数值标准离差率平方的缩放系数和最大值，用于避免错误同步
#define SWS_CAP_COV2_SCALE      (10000)
#define SWS_CAP_COV2_MAX        (64)    // 64/10000=(0.08)^2对应偏移8%
// 辅助同步时钟参数(偏移+-40%)
#define SWS_SYNCA_CNT_MIN       (100-40)
#define SWS_SYNCA_CNT_MAX       (100+40)
#define SWS_SYNCA_CNT_SCALE     (100)
// 数据帧最大字节数
#define SWS_FRAME_SIZE_MAX      (16)
// 接收字异或值
#define SWS_CODE_XOR            (0xcccc)

// 接收状态枚举
typedef enum {
    SWS_RECV_SYNC,          // 同步过程中
    SWS_RECV_SYNC_OK,       // 同步成功
    SWS_RECV_SYNC_A,        // 辅助同步
    SWS_RECV_SYNC_AF,       // 第一次辅助同步
    SWS_RECV_DATA,          // 接收数据
    SWS_RECV_END            // 接收完成
} SWS_State_t;

// 错误码枚举
typedef enum {
    ERR_CODE_NONE,
    ERR_CODE_SYNC_INVALID,
    ERR_CODE_SYNC_FAILD,
    ERR_CODE_SYNCA_FAILED,
    ERR_CODE_DECODE,
    ERR_CODE_MAX
} SWS_Error_t;

// 接收状态
static volatile SWS_State_t gSWSState;
// 接收波特率
static volatile uint16_t gSWSBandrate;
// 接收比特时间
static volatile uint16_t gSWSBitPeriod;
// 帧缓存
static volatile uint8_t gSWSRecvBuf[SWS_FRAME_SIZE_MAX];
// 帧大小
static volatile uint8_t gSWSFrameSize;
// 帧字节计数
static volatile uint8_t gSWSRecvCnt;
// 错误码
static volatile uint16_t gSWSErrCode[ERR_CODE_MAX];
// 错误帧
static uint8_t gSWSRecvBufErr[SWS_FRAME_SIZE_MAX];

/*-----------------------------------*/

#define SWS_GET_RX()    ((GPIOA->IDR & GPIO_Pin_10) != 0)
#ifdef SWS_DEBUG
#define SWS_TRIG_0()    GPIOA->BRR = GPIO_Pin_7
#define SWS_TRIG_1()    GPIOA->BSRR = GPIO_Pin_7
#define SWS_DO_0()      GPIOA->BRR = GPIO_Pin_6
#define SWS_DO_1()      GPIOA->BSRR = GPIO_Pin_6
#define SWS_DO_T()      GPIOA->ODR ^= GPIO_Pin_6
#define SWS_ST_0()      GPIOA->BRR = GPIO_Pin_5
#define SWS_ST_1()      GPIOA->BSRR = GPIO_Pin_5
#define SWS_ST_T()      GPIOA->ODR ^= GPIO_Pin_5
#else //SWS_DEBUG
#define SWS_TRIG_0()
#define SWS_TRIG_1()
#define SWS_DO_0()
#define SWS_DO_1()
#define SWS_DO_T()
#define SWS_ST_0()
#define SWS_ST_1()
#define SWS_ST_T()
#endif //SWS_DEBUG

void SWS_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* Enable GPIO Clock */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA , ENABLE);

    /* Configure Rx(PA.10) as input pull-up */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* Configure CE(PA.9) as push-pull output */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

#ifdef SWS_DEBUG
    /* Configure TRIG(PA.7) as push-pull output */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* Configure DO(PA.6) as push-pull output */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* Configure ST(PA.5) as push-pull output */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
#endif //SWS_DEBUG

    // 使能接收模块CE
    GPIO_SetBits(GPIOA, GPIO_Pin_9);
}

void SWS_NVIC_Init(void)
{
    NVIC_InitTypeDef NVIC_InitStructure;

    /* Configure the NVIC Preemption Priority Bits */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);

    /* Enable the TIM1 Capture/Compare Interrupt */
    NVIC_InitStructure.NVIC_IRQChannel = TIM1_CC_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

void SWS_Timer_Init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;
    TIM_ICInitTypeDef TIM_ICInitStructure;
    uint16_t PrescalerValue;

    /* TIM1 clock enable */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);

    /* Compute the prescaler value */
    PrescalerValue = (uint16_t)(SystemCoreClock / SWS_TIMER_FREQ) - 1;

    TIM_DeInit(TIM1);

    /* Time base configuration */
    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.TIM_Period = 0xffff;
    TIM_TimeBaseStructure.TIM_Prescaler = PrescalerValue;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);

    /* Output Compare Timing Mode configuration:
       Channel2
       used to timing bit
    */
    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_Timing;
    TIM_OCInitStructure.TIM_Pulse = SWS_BIT_TIMER_CNT;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC2Init(TIM1, &TIM_OCInitStructure);
    TIM_OC2PreloadConfig(TIM1, TIM_OCPreload_Disable);

    /* Input Capture mode configuration:
       Channel3 (PA.10)
       The Falling edge is used as active edge
    */
    TIM_ICStructInit(&TIM_ICInitStructure);
    TIM_ICInitStructure.TIM_Channel = TIM_Channel_3;
    TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Falling;
    TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
    TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    TIM_ICInitStructure.TIM_ICFilter = 0xf; // (1/(2400000/32))*8=107us
    TIM_ICInit(TIM1, &TIM_ICInitStructure);

#ifdef SWS_DEBUG
    /* Input Capture mode configuration:
       Channel4 (PA.10)
       The Rising edge is used as active edge
    */
    TIM_ICStructInit(&TIM_ICInitStructure);
    TIM_ICInitStructure.TIM_Channel = TIM_Channel_4;
    TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;
    TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_IndirectTI;
    TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    TIM_ICInitStructure.TIM_ICFilter = 0xf; // (1/(2400000/32))*8=107us
    TIM_ICInit(TIM1, &TIM_ICInitStructure);
#endif //SWS_DEBUG

    /* Enable the CC2 and CC3 Interrupt Request */
    TIM_ITConfig(TIM1, TIM_IT_CC2 | TIM_IT_CC3, ENABLE);
#ifdef SWS_DEBUG
    /* Enable the  CC4 Interrupt Request */
    TIM_ITConfig(TIM1, TIM_IT_CC4, ENABLE);
#endif //SWS_DEBUG

    /* TIM enable counter */
    //TIM_Cmd(TIM1, ENABLE);
}

/*-----------------------------------*/

// 根据序号判断帧大小
static uint8_t GetFrameSizeByIndex(uint8_t idx)
{
    uint8_t size;
    
    switch (idx) {
    case SWS_RF_IDX_RUNCNT:
        size = 7;
        break;
    case SWS_RF_IDX_BUILDDATE:
        size = 7;
        break;
    case SWS_RF_IDX_AUTHOR:
        size = 7;
        break;
    default:
        size = 7;
        break;
    }

    return size;
}

// 定时器中断
void TIM1_CC_IRQHandler(void)
{
    static uint8_t synccnt = 0;
    static uint8_t syncbyte = 0;
    static uint8_t capcnt = 0;
    static uint16_t capbase = 0;
    static uint16_t caplast = 0;
    static uint16_t capdiff[4] = {0};
    static uint8_t recvcnt = 0;
    static uint16_t recvword = 0;
    uint16_t ccvalue;

    if (TIM_GetITStatus(TIM1, TIM_IT_CC2) != RESET) {
        SWS_ST_T();
        /* Clear compare interrupt pending bit */
        TIM_ClearITPendingBit(TIM1, TIM_IT_CC2);
        /* Get the Output Compare value */
        ccvalue = TIM_GetCapture2(TIM1);
        /* set next compare value */
        TIM_SetCompare2(TIM1, ccvalue + gSWSBitPeriod);
        if (SWS_RECV_SYNC_OK == gSWSState) {
            // 波特率同步完成状态，判断同步是否有效并进行后续操作
            syncbyte = (syncbyte << 1) | SWS_GET_RX();
            synccnt++;
            if (synccnt >= 4) {
                synccnt = 0;
                // 引导序列最后会有2比特'0'+2比特'1'，以此判断引导序列结束
                if ((syncbyte & 0xf) == 0x3) {
                    gSWSState = SWS_RECV_SYNC_AF;
                    capbase = ccvalue - gSWSBitPeriod / 2;
                } else {
                    gSWSState = SWS_RECV_SYNC;
                    gSWSBitPeriod = SWS_BIT_TIMER_CNT;
                    gSWSErrCode[ERR_CODE_SYNC_INVALID]++;
                }
            }
        } else if (SWS_RECV_SYNC_AF == gSWSState || SWS_RECV_SYNC_A == gSWSState) {
            // 时钟同步状态
            synccnt++;
            if (synccnt >= 2) {
                synccnt = 0;
                if (SWS_RECV_SYNC_AF == gSWSState) {
                    // 第一次时钟同步失败则重新查找引导序列
                    gSWSState = SWS_RECV_SYNC;
                    gSWSBitPeriod = SWS_BIT_TIMER_CNT;
                } else {
                    // 接收数据过程中时钟同步失败则忽略本次同步
                    gSWSState = SWS_RECV_DATA;
                    recvcnt = 0;
                }
                gSWSErrCode[ERR_CODE_SYNCA_FAILED]++;
            }
        } else if (SWS_RECV_DATA == gSWSState) {
            // 数据接收状态
            uint8_t recvbyte;
            uint8_t status;
            
            // 接收数据
            recvword = (recvword << 1) | SWS_GET_RX();
            recvcnt++;
            if (recvcnt >= 16) {
                recvcnt = 0;
                // 解码
                recvword ^= SWS_CODE_XOR;
                status = ecc1608_decode(&recvbyte, recvword);
                if (status == 0) {
                    recvbyte = recvword >> 8;
                    gSWSErrCode[ERR_CODE_DECODE]++;
                }
                gSWSRecvBuf[gSWSRecvCnt++] = recvbyte;
                if (gSWSRecvCnt >= gSWSFrameSize) {
                    // 根据帧大小判断接收完成
                    gSWSState = SWS_RECV_END;
                } else if (gSWSRecvCnt >= 2) {
                    // 根据序号判断帧大小
                    gSWSFrameSize = GetFrameSizeByIndex(gSWSRecvBuf[1]);
                }
                if (SWS_RECV_END != gSWSState) {
                    // 未接收完成时进行辅助时钟同步
                    gSWSState = SWS_RECV_SYNC_A;
                    capbase = ccvalue + gSWSBitPeriod / 2;
                    synccnt = 0;
                }
            }
        }
    } else if (TIM_GetITStatus(TIM1, TIM_IT_CC3) != RESET) {
        /* Falling edge */
        SWS_DO_0();
        /* Clear Capture interrupt pending bit */
        TIM_ClearITPendingBit(TIM1, TIM_IT_CC3);
        /* Get the Input Capture value */
        ccvalue = TIM_GetCapture3(TIM1);
        if (SWS_RECV_SYNC == gSWSState || SWS_RECV_SYNC_OK == gSWSState) {
            // 波特率同步状态
            uint16_t capavg;
            uint32_t cap2avg;
            uint32_t capcov2;

            capdiff[capcnt++] = ccvalue - caplast;
            if (capcnt >= 4) {
                capcnt = 0;
            }
            caplast = ccvalue;
            // 根据4次下降沿计算：
            // 平均值
            capavg = (capdiff[0] + capdiff[1] + capdiff[2] + capdiff[3]) / 4;
            // 平均值在误差允许范围内
            if (capavg >= SWS_CAP_CNT_MIN && capavg <= SWS_CAP_CNT_MAX) {
                // 平方的平均值
                cap2avg = (capdiff[0] * capdiff[0] + capdiff[1] * capdiff[1] + capdiff[2] * capdiff[2] + capdiff[3] * capdiff[3]) / 4;
                // 标准偏离率的平方*N倍, V^2=o^2/E^2=(E(X^2)-E(X)^2)/E(X)^2=(E(X^2)/E(X)^2)-1
                capcov2 = cap2avg / capavg * SWS_CAP_COV2_SCALE / capavg - SWS_CAP_COV2_SCALE;
                // 偏移率在误差允许范围内
                if (capcov2 <= SWS_CAP_COV2_MAX) {
                    // 计算波特率
                    gSWSBitPeriod = capavg / 2;
                    gSWSBandrate = SWS_TIMER_FREQ * 2 / capavg;
                    // 接收同步字，从bit中间采样
                    TIM_SetCompare2(TIM1, ccvalue + gSWSBitPeriod / 2);
                    TIM_ClearITPendingBit(TIM1, TIM_IT_CC2);
                    synccnt = 0;
                    gSWSState = SWS_RECV_SYNC_OK;
                } else {
                    if (SWS_RECV_SYNC_OK == gSWSState) {
                        gSWSErrCode[ERR_CODE_SYNC_FAILD]++;
                    }
                    gSWSBitPeriod = SWS_BIT_TIMER_CNT;
                    gSWSState = SWS_RECV_SYNC;
                }
            } else {
                if (SWS_RECV_SYNC_OK == gSWSState) {
                    gSWSErrCode[ERR_CODE_SYNC_FAILD]++;
                }
                gSWSBitPeriod = SWS_BIT_TIMER_CNT;
                gSWSState = SWS_RECV_SYNC;
            }
        } else if (SWS_RECV_SYNC_AF == gSWSState || SWS_RECV_SYNC_A == gSWSState) {
            // 时钟同步状态
            if ((uint16_t)(ccvalue - capbase) >= gSWSBitPeriod * SWS_SYNCA_CNT_MIN / SWS_SYNCA_CNT_SCALE && 
                (uint16_t)(ccvalue - capbase) <= gSWSBitPeriod * SWS_SYNCA_CNT_MAX / SWS_SYNCA_CNT_SCALE) {
                SWS_TRIG_0();
                // 接收数据，从bit中间采样
                TIM_SetCompare2(TIM1, ccvalue + gSWSBitPeriod + gSWSBitPeriod / 2);
                TIM_ClearITPendingBit(TIM1, TIM_IT_CC2);
                synccnt = 0;
                recvcnt = 0;
                gSWSState = SWS_RECV_DATA;
            } else {
                SWS_TRIG_1();
                //gSWSErrCode[ERR_CODE_SYNCA_FAILED]++;
            }
        }
#ifdef SWS_DEBUG
    } else if (TIM_GetITStatus(TIM1, TIM_IT_CC4) != RESET) {
        /* Rising edge */
        SWS_DO_1();
        /* Clear Capture interrupt pending bit */
        TIM_ClearITPendingBit(TIM1, TIM_IT_CC4);
#endif //SWS_DEBUG
    }
}

void SWS_Recv(void)
{
    char strbuf[64] = "";
    uint8_t len;
    uint8_t checksum;
    uint8_t checksum_recv;
    uint32_t total_ok = 0, total_bad = 0, total_last = 0;
    uint8_t idx_last;
    uint16_t cnt_h;
    uint8_t has_cnt_h = 0;
    uint8_t i;
    uint32_t t_begin;
    uint32_t t_last;
    uint32_t t_past;

    SWS_NVIC_Init();
    SWS_GPIO_Init();
    SWS_Timer_Init();

    LCD_Clear();
    LCD_DrawString("Solar Wireless Sensor", 0, 0, FONT_SIZE_5X8, 0x3);
    LCD_DrawString(" Reciever by eleqian ", 0, 8, FONT_SIZE_5X8, 0x3);
    LCD_DrawPicture8_1Bpp(graphic1, 48, 24 / 8, 32, 32 / 8, 0x3);
    LCD_DrawString("      Waiting...     ", 0, 64, FONT_SIZE_5X8, 0x3);
    LCD_Refresh();

    TIM_Cmd(TIM1, ENABLE);
    
    t_begin = millis();
    t_last = 0;

    while (1) {
        // 开始接收
        gSWSBitPeriod = SWS_BIT_TIMER_CNT;
        gSWSFrameSize = SWS_FRAME_SIZE_MAX;
        gSWSRecvCnt = 0;
        gSWSState = SWS_RECV_SYNC;
        // 等待接收完成
        while (SWS_RECV_END != gSWSState) {
            // 期间进行时间和错误统计刷新
            t_past = millis() / 1000 - t_last;
            sprintf(strbuf, "%01dd %02dh %02dm %02ds +%d", t_last / (3600 * 24), (t_last / 3600) % 24, (t_last / 60) % 60, t_last % 60, t_past);
            LCD_FillRect(0, 56, 128, 8, 0x0);
            LCD_DrawString(strbuf, 0, 56, FONT_SIZE_5X8, 0x3);
            sprintf(strbuf, "%04x %04x %04x %04x", gSWSErrCode[1], gSWSErrCode[2], gSWSErrCode[3], gSWSErrCode[4]);
            LCD_FillRect(0, 72, 128, 8, 0x0);
            LCD_DrawString(strbuf, 0, 72, FONT_SIZE_5X8, 0x3);
            LCD_Refresh();
        }
        // 接收完成进行校验
        checksum = CaclCheckSum(gSWSRecvBuf, gSWSRecvCnt - 1);
        checksum_recv = gSWSRecvBuf[gSWSRecvCnt - 1];
        // 统计丢包数
        if (checksum == checksum_recv) {
            if (total_ok > 0 && (uint8_t)(idx_last + 1) != gSWSRecvBuf[1]) {
                total_last += (uint8_t)(gSWSRecvBuf[1] - idx_last - 1);
            }
            idx_last = gSWSRecvBuf[1];
            total_ok++;
        } else {
            memset(gSWSRecvBufErr, 0x00, SWS_FRAME_SIZE_MAX);
            memcpy(gSWSRecvBufErr, (uint8_t*)gSWSRecvBuf, SWS_FRAME_SIZE_MAX);
            total_bad++;
        }
        // 显示
        LCD_Clear();
        sprintf(strbuf, "ID: 0x%02x  B: %d", gSWSRecvBuf[0], gSWSBandrate);
        LCD_DrawString(strbuf, 0, 0, FONT_SIZE_5X8, 0x3);
        len = sprintf(strbuf, "Index: %d", gSWSRecvBuf[1]);
        if (has_cnt_h) {
            sprintf(strbuf + len, " (0x%04x)", cnt_h);
        }
        LCD_DrawString(strbuf, 0, 8, FONT_SIZE_5X8, 0x3);
        switch (gSWSRecvBuf[1]) {
        case SWS_RF_IDX_RUNCNT:
            if (checksum == checksum_recv) {
                has_cnt_h = 1;
                cnt_h = (gSWSRecvBuf[2] << 8) | gSWSRecvBuf[3];
            }
            sprintf(strbuf, "CountHigh: 0x%04x", (gSWSRecvBuf[2] << 8) | gSWSRecvBuf[3]);
            LCD_DrawString(strbuf, 0, 16, FONT_SIZE_5X8, 0x3);
            break;
        case SWS_RF_IDX_BUILDDATE:
            sprintf(strbuf, "BuildDate: %02x%02x.%02x.%02x", gSWSRecvBuf[2], gSWSRecvBuf[3], gSWSRecvBuf[4], gSWSRecvBuf[5]);
            LCD_DrawString(strbuf, 0, 16, FONT_SIZE_5X8, 0x3);
            break;
        case SWS_RF_IDX_AUTHOR:
            len = sprintf(strbuf, "Designer: ");
            memcpy(strbuf + len, (void *)&gSWSRecvBuf[2], gSWSFrameSize - 3);
            strbuf[len + gSWSFrameSize - 3] = '\0';
            LCD_DrawString(strbuf, 0, 16, FONT_SIZE_5X8, 0x3);
            break;
        default:
            sprintf(strbuf, "T: %d.%dC", (int8_t)gSWSRecvBuf[2], (gSWSRecvBuf[3] >> 4));
            LCD_DrawString(strbuf, 0, 16, FONT_SIZE_5X8, 0x3);
            sprintf(strbuf, "I: %duA  U: %d.%02dV", ((gSWSRecvBuf[3] & 0x0f) << 8) | gSWSRecvBuf[4],
                    gSWSRecvBuf[5] / 100 + 1, gSWSRecvBuf[5] % 100);
            LCD_DrawString(strbuf, 0, 24, FONT_SIZE_5X8, 0x3);
            break;
        }
        sprintf(strbuf, "CKS: 0x%02x %s", checksum_recv, (checksum == checksum_recv ? "OK" : "BAD"));
        LCD_DrawString(strbuf, 0, 32, FONT_SIZE_5X8, 0x3);
        sprintf(strbuf, "Count: %d OK", total_ok);
        LCD_DrawString(strbuf, 0, 40, FONT_SIZE_5X8, 0x3);
        sprintf(strbuf, "  %d BAD, %d LAST", total_bad, total_last);
        LCD_DrawString(strbuf, 0, 48, FONT_SIZE_5X8, 0x3);
        t_past = 0;
        t_last = (millis() - t_begin) / 1000;
        sprintf(strbuf, "%01dd %02dh %02dm %02ds +%d", t_last / (3600 * 24), (t_last / 3600) % 24, (t_last / 60) % 60, t_last % 60, t_past);
        LCD_DrawString(strbuf, 0, 56, FONT_SIZE_5X8, 0x3);
        for (i = 0, len = 0; i < 7; i++) {
            len += sprintf(strbuf + len, "%02x ", gSWSRecvBufErr[i]);
        }
        LCD_DrawString(strbuf, 0, 64, FONT_SIZE_5X8, 0x3);
        sprintf(strbuf, "%04x %04x %04x %04x", gSWSErrCode[1], gSWSErrCode[2], gSWSErrCode[3], gSWSErrCode[4]);
        LCD_DrawString(strbuf, 0, 72, FONT_SIZE_5X8, 0x3);
        LCD_Refresh();
    }
}

#endif //SWS_RF_SYNC

/*-----------------------------------*/

int main(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);

    // 关JTAG，仅使用SWD
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

    TimeBase_Init();
    LED_Init();
    LCD_Init();

    LCD_LightOnOff(TRUE);

    SWS_Recv();

    LCD_Clear();
    LCD_DrawPicture8_1Bpp(graphic1, 16, 1, 32, 4, 0x3);
    LCD_DrawString("Hello World!", 0, 0, FONT_SIZE_5X8, 0x3);
    LCD_Refresh();
    LCD_LightOnOff(TRUE);

    while (1) {
        LED_Light(TRUE);
        delay_ms(250);
        LED_Light(FALSE);
        delay_ms(250);
    }
}

/*-----------------------------------*/

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    /* User can add his own implementation to report the file name and line number,
       ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* Infinite loop */
    while (1) {
    }
}
#endif //USE_FULL_ASSERT
