/* 采样模块 for 音频频谱分析
 * eleqian 2016-7-3
 * 实现规划：
 * ADC输入通道：0；GPIO：PA0
 * 使用TIM1_CC1触发ADC1采样，使得采样率为8kHz或32kHz
 * ADC1转换结果由DMA送到内存，达到512或2048次时通知上层处理
 * TIM1时钟72MHz，ADC1时钟12MHz（6分频）
 */

#include "base.h"
#include "sample.h"

// 采样数据，不加volatile是因为DMA和数据处理不会同时进行
uint16_t gSampleValues[SAMPLES_MAX];

/*-----------------------------------*/

// 引脚配置
static void Sample_PinConfig(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    // PA0作为模拟通道0输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // PA8作为TIM1_CH1输出(调试时查看波形)
    //GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
    //GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    //GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    //GPIO_Init(GPIOA, &GPIO_InitStructure);
}

// ADC配置
static void Sample_ADCConfig(void)
{
    ADC_InitTypeDef ADC_InitStructure;

    // 72MHz / 6 = 12MHz
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T1_CC1;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    // 通道0
    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_28Cycles5);

    // Enable ADC1 external trigger
    ADC_ExternalTrigConvCmd(ADC1, ENABLE);

    // Enable ADC1 DMA
    ADC_DMACmd(ADC1, ENABLE);

    // Enable ADC1
    ADC_Cmd(ADC1, ENABLE);

    // 校准
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1));
}

// DMA配置
static void Sample_DMAConfig(uint16_t size)
{
    DMA_InitTypeDef DMA_InitStructure;

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    // DMA1 channel1 configuration
    DMA_DeInit(DMA1_Channel1);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)gSampleValues;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize = size;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel1, &DMA_InitStructure);

    // Enable DMA1 Channel1 Transfer Complete interrupt
    DMA_ITConfig(DMA1_Channel1, DMA_IT_TC, ENABLE);

    // Enable DMA1 channel1
    DMA_Cmd(DMA1_Channel1, ENABLE);
}

// 采样定时器配置
static void Sample_TimerConfig(uint16_t period)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);

    // TIM1时钟72MHz
    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.TIM_Period = period;
    TIM_TimeBaseStructure.TIM_Prescaler = 0x0;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0x0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);

    /* TIM1 channel1 configuration in PWM mode */
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = period / 2;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_Low;
    TIM_OC1Init(TIM1, &TIM_OCInitStructure);

    TIM_CtrlPWMOutputs(TIM1, ENABLE);

    //初始化时不启动定时
    //TIM_Cmd(TIM1, ENABLE);
}

// DMA中断配置
static void Sample_NVICConfig(void)
{
    NVIC_InitTypeDef NVIC_InitStructure;

    /* Enable DMA1 channel1 IRQ Channel */
    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

/*-----------------------------------*/

// 初始化
void Sample_Init(void)
{
    Sample_PinConfig();
    Sample_DMAConfig(SAMPLES_MAX);
    Sample_ADCConfig();
    Sample_TimerConfig(SAMPLE_TIMER_PERIOD_8K);
    Sample_NVICConfig();
}

// 开始采样
void Sample_Start(uint16_t speriod, uint16_t scount)
{
    // 配置DMA传输次数（即ADC采样次数）
    Sample_DMAConfig(scount);

    // 配置定时器周期（即ADC采样速率）
    Sample_TimerConfig(speriod);

    // 开定时器（即开始采样）
    TIM_Cmd(TIM1, ENABLE);
}

/**
  * @brief  This function handles DMA1 Channel 1 interrupt request.
  * @param  None
  * @retval None
  */
void DMA1_Channel1_IRQHandler(void)
{
    /* Test on DMA1 Channel1 Transfer Complete interrupt */
    if (DMA_GetITStatus(DMA1_IT_TC1)) {
        // 关定时器（即停止采样）
        TIM_Cmd(TIM1, DISABLE);

        // 停止DMA（事实上不需要，仅为避免出错）
        DMA_Cmd(DMA1_Channel1, DISABLE);

        /* Clear DMA1 Channel1 Half Transfer, Transfer Complete and Global interrupt pending bits */
        DMA_ClearITPendingBit(DMA1_IT_GL1);

        // 发送事件
        OnSampleComplete();
    }
}
