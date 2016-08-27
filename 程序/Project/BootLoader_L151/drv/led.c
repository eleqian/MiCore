/*
 * LED驱动模块 for stm32f103c8 & stm32l151c8
 * LED引脚 PB0
 * eleqian 2014-9-10
 */

#include "base.h"
#include "timebase.h"
#include "led.h"

// LED模式枚举
enum {
    LED_MODE_NORMAL,    // 普通模式（亮/灭）
    LED_MODE_FLASH,     // 闪烁模式
    LED_MODE_BREATH,    // 呼吸灯模式

    LED_MODE_MAX
} LED_Mode;

// LED模式
static uint8_t gLEDMode;

// 上次更新时间(ms)
static uint32_t gLEDLastTime;

// LED计时(ms)
static uint16_t gLEDTimer;

// LED闪烁周期(ms)
static uint16_t gLEDPeriod;

// LED每闪烁周期点亮时间(ms)
static uint16_t gLEDPulse;

// LED闪烁次数
static uint16_t gLEDCycleCnt;

/*-----------------------------------*/

// 开/关LED
#define LED_ON()        TIM_SetCompare3(TIM3, 1000)
#define LED_OFF()       TIM_SetCompare3(TIM3, 0)

// 调整LED亮度，这里用平方粗略模拟伽马校正
#define LED_LEVEL(l)    TIM_SetCompare3(TIM3, (l) * (l) / 1000)

// LED定时器初始化
static void LED_TimerConfig(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    /* -----------------------------------------------------------------------
      TIM3 Configuration:
      The TIM3CLK frequency is set to SystemCoreClock (Hz), to get TIM3 counter
      clock at 8 MHz the Prescaler is computed as following:
       - Prescaler = (TIM3CLK / TIM3 counter clock) - 1
      The TIM3 is running at 8 KHz: 
       - TIM3 Frequency = TIM3 counter clock/(ARR + 1) = 8 MHz / 1000 = 8 KHz
       - TIM3 Channel3 duty cycle = (TIM3_CCR3 / TIM3_ARR)* 100
    ----------------------------------------------------------------------- */

    // Time base configuration
    TIM_TimeBaseStructure.TIM_Period = 999;
    TIM_TimeBaseStructure.TIM_Prescaler = (uint16_t)(SystemCoreClock / 8000000) - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;

    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

    // PWM1 Mode configuration: Channel3
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 0;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC3Init(TIM3, &TIM_OCInitStructure);
    TIM_OC3PreloadConfig(TIM3, TIM_OCPreload_Enable);

    TIM_ARRPreloadConfig(TIM3, ENABLE);

    // TIM3 enable counter
    TIM_Cmd(TIM3, ENABLE);
}

// LED端口初始化
static void LED_GPIOConfig(void)
{
   GPIO_InitTypeDef GPIO_InitStructure;

#ifdef MCU_STM32L151C8
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOB, ENABLE);

    // GPIOB Configuration: Pin 0
    GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_PinAFConfig(GPIOB, GPIO_PinSource0, GPIO_AF_TIM3);
#endif //MCU_STM32L151C8

#ifdef MCU_STM32F103C8
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    // GPIOB Configuration: Pin 0
    GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
#endif //MCU_STM32F103C8
}

/*-----------------------------------*/

// LED初始化
void LED_Init(void)
{
    LED_GPIOConfig();
    LED_TimerConfig();

    gLEDLastTime = millis();
    gLEDMode = LED_MODE_NORMAL;
}

// LED开关
void LED_Light(BOOL isLight)
{
    gLEDMode = LED_MODE_NORMAL;
    if (isLight) {
        LED_ON();
    } else {
        LED_OFF();
    }
}

// LED闪烁
// 参数：周期(ms)，点亮时间(ms)，闪烁次数(=0时一直闪烁)
void LED_Flash(uint16_t period, uint16_t pulse, uint16_t cycles)
{
    gLEDMode = LED_MODE_FLASH;
    gLEDPeriod = period;
    gLEDPulse = pulse;
    gLEDCycleCnt = cycles;
    gLEDTimer = 0;
    gLEDLastTime = millis();
    LED_OFF();
}

// LED呼吸灯
// 参数：周期(ms)，点亮时间(ms)，呼吸次数(=0时一直呼吸)
void LED_Breath(uint16_t period, uint16_t pulse, uint16_t cycles)
{
    gLEDMode = LED_MODE_BREATH;
    gLEDPeriod = period;
    gLEDPulse = pulse;
    gLEDCycleCnt = cycles;
    gLEDTimer = 0;
    gLEDLastTime = millis();
    LED_LEVEL(0);
}

/*-----------------------------------*/

// LED定时任务
void LED_Exec(void)
{
    uint32_t t;

    switch (gLEDMode) {
    case LED_MODE_FLASH:
        t = millis();
        gLEDTimer += (t - gLEDLastTime);
        gLEDLastTime = t;
        if (gLEDTimer >= gLEDPulse) {
            LED_OFF();
        }
        if (gLEDTimer >= gLEDPeriod) {
            if (gLEDCycleCnt > 0) {
                gLEDCycleCnt--;
                if (gLEDCycleCnt == 0) {
                    gLEDMode = LED_MODE_NORMAL;
                    LED_OFF();
                    return;
                }
            }
            gLEDTimer = 0;
            LED_ON();
        }
        break;
    case LED_MODE_BREATH:
        t = millis();
        gLEDTimer += (t - gLEDLastTime);
        gLEDLastTime = t;
        if (gLEDTimer >= gLEDPulse) {
            LED_LEVEL(1000 * (gLEDPeriod - gLEDTimer) / (gLEDPeriod - gLEDPulse));
        } else {
            LED_LEVEL(1000 * gLEDTimer / gLEDPulse);
        }
        if (gLEDTimer >= gLEDPeriod) {
            if (gLEDCycleCnt > 0) {
                gLEDCycleCnt--;
                if (gLEDCycleCnt == 0) {
                    gLEDMode = LED_MODE_NORMAL;
                    LED_LEVEL(0);
                    return;
                }
            }
            gLEDTimer = 0;
            LED_LEVEL(0);
        }
        break;
    default:
        break;
    }
}
