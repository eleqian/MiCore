#include "base.h"
#include "sound.h"

static struct {
    BOOL isplaying;
    BOOL enabled;
} sound;

/*-----------------------------------*/

void sound_set_enabled(int v)
{
    sound.enabled = v;
}
int sound_get_enabled(void)
{
    return sound.enabled;
}
void sound_toggle_enabled(void)
{
    sound.enabled = !sound.enabled;
}

/*-----------------------------------*/

// 声音定时器初始化
static void SND_TimerConfig(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    /* -----------------------------------------------------------------------
      TIM2 Configuration:
      The TIM2CLK frequency is set to SystemCoreClock (Hz), to get TIM2 counter
      clock at 8 MHz the Prescaler is computed as following:
       - Prescaler = (TIM2CLK / TIM2 counter clock) - 1
      The TIM2 is running at 8 KHz: 
       - TIM2 Frequency = TIM3 counter clock/(ARR + 1) = 8 MHz / 1000 = 8 KHz
       - TIM2 Channel1 duty cycle = (TIM2_CCR1 / TIM2_ARR)* 100
    ----------------------------------------------------------------------- */

    // Time base configuration
    TIM_TimeBaseStructure.TIM_Period = (1000 - 1);
    //TIM_TimeBaseStructure.TIM_Prescaler = (uint16_t)(SystemCoreClock / 8000000) - 1;
    TIM_TimeBaseStructure.TIM_Prescaler = (uint16_t)(SystemCoreClock / 1000000) - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;

    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    // PWM1 Mode configuration: Channel1
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 0;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC1Init(TIM2, &TIM_OCInitStructure);
    TIM_OC1PreloadConfig(TIM2, TIM_OCPreload_Enable);

    TIM_ARRPreloadConfig(TIM2, ENABLE);

    // TIM2 enable counter
    TIM_Cmd(TIM2, ENABLE);
}

// 声音端口初始化
static void SND_GPIOConfig(void)
{
   GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    // GPIOA Configuration: Pin 0
    GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

/*-----------------------------------*/

void sound_init(void)
{
    sound.enabled = TRUE;
    sound.isplaying = FALSE;
    SND_GPIOConfig();
    SND_TimerConfig();
}

void sound_play(void)
{
    if (!sound.enabled) {
        return;
    }

    if (!sound.isplaying) {
        TIM_SetCompare1(TIM2, 500);
        sound.isplaying = TRUE;
    }
}

void sound_stop(void)
{
    if (sound.isplaying) {
        TIM_SetCompare1(TIM2, 0);
        sound.isplaying = FALSE;
    }
}
