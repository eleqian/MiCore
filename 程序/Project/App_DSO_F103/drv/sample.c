/* 采样模块
 * eleqian 2014-3-9
 * 实现细节：
 * ADC输入通道0(PA0)
 * ADC时钟14MHz，采样时间1.5周期
 * 采样率=2M时使用双ADC快速交叉模式采样，=1M时使用ADC1连续采样，<1M时使用TIM2_CC2触发ADC1采样
 * 采样使用DMA传输，在DMA中断处理
 */

#include "base.h"
#include "sample.h"

// 打开宏时使能定时器触发输出(PA1)
//#define SAMPLE_TRIG_TEST

// ADC模式枚举
enum {
    ADC_MODE_DISABLE,       // 禁用ADC
    ADC_MODE_TWO_FAST_DMA,  // 双ADC快速交叉模式+DMA
    ADC_MODE_ONE_CONT_DMA,  // 单ADC连续转换模式+DMA
    ADC_MODE_ONE_TIM_DMA,   // 单ADC定时器触发模式+DMA
    ADC_MODE_MAX
};

// DMA模式枚举
enum {
    DMA_MODE_DISABLE,   // 禁用DMA
    DMA_MODE_1ADC,      // DMA单ADC模式
    DMA_MODE_2ADC,      // DMA双ADC模式
    DMA_MODE_MAX
};

// 采样方式标志位
#define SAMPLE_FLAG_TIM_TRIG    0x1     // 表示通过定时器触发采样，否则软件触发
#define SAMPLE_FLAG_SWAP_ADC    0x2     // 表示需要交换DMA缓冲区相邻两数据（双ADC时）

// DMA缓冲区最大大小（每个，双缓冲区*2）
#define DMA_BUF_SIZE_MAX        (256)

// 采样率枚举对应的实际值
const uint32_t tblSampleRateValue[SAMPLE_RATE_MAX] = {
    20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000, 1000000, 2000000
};

// 采样率枚举对应的DMA缓冲区大小（每个）
static const uint16_t tblDMABufSize[SAMPLE_RATE_MAX] = {
    1, 1, 1, 1, 1, 1, 2, 4, 8, 16, 32, 64, 128, 256, 256, 256
};

// DMA双缓冲区
static uint16_t gDMABuf[DMA_BUF_SIZE_MAX * 2];

// 实际使用的DMA缓冲区大小（每个）
static uint16_t gDMABufPerSize;

// 标志位
static uint8_t gSampleFlags;

/*-----------------------------------*/

// 设置和清除CONT位，控制连续转换模式的启停，外设库没提供所以在这里实现
void ADC_ContinuousCmd(ADC_TypeDef *ADCx, FunctionalState NewState)
{
    // Check the parameters
    assert_param(IS_ADC_ALL_PERIPH(ADCx));
    assert_param(IS_FUNCTIONAL_STATE(NewState));

    if (NewState != DISABLE) {
        ADCx->CR2 |= ADC_CR2_CONT;
    } else {
        ADCx->CR2 &= ~ADC_CR2_CONT;
    }
}

/*-----------------------------------*/

// 引脚配置
static void Config_Pin(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    // PA0作为ADC输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

#ifdef SAMPLE_TRIG_TEST
    // PA1作为TIM2_CH2输出(调试时测量ADC触发频率)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
#endif //SAMPLE_TRIG_TEST
}

// ADC配置
static void Config_ADC(uint8_t mode)
{
    static uint8_t last_mode = ADC_MODE_DISABLE;
    ADC_InitTypeDef ADC_InitStructure;

    // 模式相同时不重复配置
    if (mode == last_mode) {
        return;
    }

    // 56MHz / 4 = 14MHz
    RCC_ADCCLKConfig(RCC_PCLK2_Div4);

    // 关闭外设及其时钟，降低功耗
    if (mode == ADC_MODE_DISABLE) {
        ADC_DeInit(ADC1);
        ADC_DeInit(ADC2);
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, DISABLE);
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC2, DISABLE);
        last_mode = mode;
        return;
    } else if (mode != ADC_MODE_TWO_FAST_DMA) {
        ADC_DeInit(ADC2);
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC2, DISABLE);
    }

    /* 配置ADC1 */

    // 第一次配置
    if (last_mode == ADC_MODE_DISABLE) {
        // 打开ADC1时钟
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
    }

    ADC_StructInit(&ADC_InitStructure);
    if (mode == ADC_MODE_TWO_FAST_DMA) {
        // 双ADC快速交叉连续转换
        ADC_InitStructure.ADC_Mode = ADC_Mode_FastInterl;
        ADC_InitStructure.ADC_ScanConvMode = DISABLE;
        ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
        ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
        ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
        ADC_InitStructure.ADC_NbrOfChannel = 1;
    } else if (mode == ADC_MODE_ONE_CONT_DMA) {
        // 单ADC连续转换
        ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
        ADC_InitStructure.ADC_ScanConvMode = DISABLE;
        ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
        ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
        ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
        ADC_InitStructure.ADC_NbrOfChannel = 1;
    } else if (mode == ADC_MODE_ONE_TIM_DMA) {
        // 单ADC定时器触发
        ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
        ADC_InitStructure.ADC_ScanConvMode = DISABLE;
        ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
        ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T2_CC2;
        ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
        ADC_InitStructure.ADC_NbrOfChannel = 1;
    }
    ADC_Init(ADC1, &ADC_InitStructure);

    // ADC1通道0
    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_1Cycles5);

    if (mode == ADC_MODE_ONE_TIM_DMA) {
        // 使能ADC1外部触发
        ADC_ExternalTrigConvCmd(ADC1, ENABLE);
    }

    // 使能ADC1的DMA
    ADC_DMACmd(ADC1, ENABLE);

    // 第一次配置
    if (last_mode == ADC_MODE_DISABLE) {
        // 使能ADC1
        ADC_Cmd(ADC1, ENABLE);

        // 校准ADC1
        ADC_ResetCalibration(ADC1);
        while (ADC_GetResetCalibrationStatus(ADC1));
        ADC_StartCalibration(ADC1);
        while (ADC_GetCalibrationStatus(ADC1));
    }

    /* 配置ADC2 */

    if (mode == ADC_MODE_TWO_FAST_DMA) {
        // 打开ADC2时钟
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC2, ENABLE);
        
        // 双ADC快速交叉连续转换
        ADC_StructInit(&ADC_InitStructure);
        ADC_InitStructure.ADC_Mode = ADC_Mode_FastInterl;
        ADC_InitStructure.ADC_ScanConvMode = DISABLE;
        ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
        ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
        ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
        ADC_InitStructure.ADC_NbrOfChannel = 1;
        ADC_Init(ADC2, &ADC_InitStructure);

        // ADC2通道0
        ADC_RegularChannelConfig(ADC2, ADC_Channel_0, 1, ADC_SampleTime_1Cycles5);

        // 使能ADC2外部触发（由ADC1自动触发）
        ADC_ExternalTrigConvCmd(ADC2, ENABLE);

        // 使能ADC2
        ADC_Cmd(ADC2, ENABLE);

        // 校准ADC2
        ADC_ResetCalibration(ADC2);
        while (ADC_GetResetCalibrationStatus(ADC2));
        ADC_StartCalibration(ADC2);
        while (ADC_GetCalibrationStatus(ADC2));
    }

    last_mode = mode;
}

// DMA配置
static void Config_DMA(uint8_t mode, uint16_t size)
{
    DMA_InitTypeDef DMA_InitStructure;

    // 关闭外设及其时钟，降低功耗
    if (mode == DMA_MODE_DISABLE) {
        DMA_DeInit(DMA1_Channel1);
        RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, DISABLE);
        return;
    }

    // 打开DMA1时钟
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    // 配置前先除能，避免出错
    DMA_Cmd(DMA1_Channel1, DISABLE);

    // DMA1通道1配置
    DMA_StructInit(&DMA_InitStructure);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)gDMABuf;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    if (mode == DMA_MODE_2ADC) {
        // 双ADC模式时ADC1_DR寄存器高16位为ADC2结果，低16位为ADC1结果
        // 注意，这样传输到RAM中是按ADC1_ADC2...的顺序排列，但实际上是ADC2先采样，需要处理顺序
        DMA_InitStructure.DMA_BufferSize = size * 2 / 2;
        DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;
        DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Word;
    } else if (mode == DMA_MODE_1ADC) {
        // 单ADC模式ADC1_DR寄存器只有低16位有效，但需要按32位访问
        DMA_InitStructure.DMA_BufferSize = size * 2;
        DMA_InitStructure.DMA_PeripheralDataSize =  DMA_PeripheralDataSize_Word;
        DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    }
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel1, &DMA_InitStructure);

    // 半满和全满中断，实现双缓冲
    DMA_ITConfig(DMA1_Channel1, DMA_IT_HT | DMA_IT_TC, ENABLE);

    // 使能DMA1通道1
    DMA_Cmd(DMA1_Channel1, ENABLE);
}

// 采样定时器配置
// 参数：采样频率（1Hz~1MHz，=0时关闭）
static void Config_Timer(uint32_t freq)
{
    const uint32_t usTicks = 56;    // 1us定时器时钟数
    static uint32_t last_freq = 0;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;
    uint16_t period;
    uint16_t prescaler;

    // 频率相同时不重复配置
    if (freq == last_freq) {
        return;
    }
    last_freq = freq;

    // 关闭外设及其时钟，降低功耗
    if (freq == 0) {
        TIM_DeInit(TIM2);
        RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, DISABLE);
        return;
    }

    // 小于1k时进行1k预分频，避免超出定时器范围
    if (freq < 1000) {
        prescaler = 1000;
        period = usTicks * 1000 / freq - 1;
    } else {
        prescaler = 0;
        period = usTicks * 1000000 / freq - 1;
    }

    // 打开TIM2时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    // 配置前先除能，避免出错
    TIM_Cmd(TIM2, DISABLE);

    // TIM2时钟
    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.TIM_Period = period;
    TIM_TimeBaseStructure.TIM_Prescaler = prescaler;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    // TIM2 CH2用于触发ADC
    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
//#ifdef SAMPLE_TRIG_TEST
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
//#else
    //TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Disable;
//#endif //SAMPLE_TRIG_TEST
    TIM_OCInitStructure.TIM_Pulse = period / 2;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_Low;
    TIM_OC2Init(TIM2, &TIM_OCInitStructure);

    // 启动即开始采样，配置时不启动
    //TIM_Cmd(TIM2, ENABLE);
}

// 中断配置
static void Config_NVIC(void)
{
    NVIC_InitTypeDef NVIC_InitStructure;

    // DMA1通道1中断（ADC）
    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

/*-----------------------------------*/

// 初始化
void Sample_Init(void)
{
    gSampleFlags = 0;
    Config_Pin();
    Config_NVIC();
    Config_Timer(0);
    Config_ADC(ADC_MODE_DISABLE);
    Config_DMA(DMA_MODE_DISABLE, 0);
}

// 设置采样率
// 参数：采样率枚举（20Hz~2MHz，以1-2-5递增）
void Sample_SetRate(sampleRate_t rate)
{
    uint32_t bufsize;

    bufsize = tblDMABufSize[rate];
    gDMABufPerSize = bufsize;
    
    //Config_Timer(0);
    Config_ADC(ADC_MODE_DISABLE);
    //Config_DMA(DMA_MODE_DISABLE, 0);

    if (rate == SAMPLE_RATE_2M) {
        // 2M时使用双ADC快速交叉采样
        gSampleFlags = SAMPLE_FLAG_SWAP_ADC;
        Config_ADC(ADC_MODE_TWO_FAST_DMA);
        Config_DMA(DMA_MODE_2ADC, bufsize);
        Config_Timer(0);
    } else if (rate == SAMPLE_RATE_1M) {
        // 1M时使用ADC1连续采样
        gSampleFlags = 0;
        Config_ADC(ADC_MODE_ONE_CONT_DMA);
        Config_DMA(DMA_MODE_1ADC, bufsize);
        Config_Timer(0);
    } else {
        // <1M时使用定时器触发ADC1采样
        gSampleFlags = SAMPLE_FLAG_TIM_TRIG;
        Config_ADC(ADC_MODE_ONE_TIM_DMA);
        Config_DMA(DMA_MODE_1ADC, bufsize);
        Config_Timer(tblSampleRateValue[rate]);
    }
}

// 开始采样
void Sample_Start(void)
{
    if (gSampleFlags & SAMPLE_FLAG_TIM_TRIG) {
        // 定时器触发
        TIM_Cmd(TIM2, ENABLE);
    } else {
        // 软件触发，连续转换模式
        ADC_ContinuousCmd(ADC1, ENABLE);
        ADC_ContinuousCmd(ADC2, ENABLE);
        ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    }
}

// 停止采样
void Sample_Stop(void)
{
    if (gSampleFlags & SAMPLE_FLAG_TIM_TRIG) {
        // 定时器触发
        TIM_Cmd(TIM2, DISABLE);
    } else {
        // 软件触发，取消连续转换模式即可停止转换
        ADC_ContinuousCmd(ADC1, DISABLE);
        ADC_ContinuousCmd(ADC2, DISABLE);
    }
}

/*-----------------------------------*/

// DMA中断处理
void DMA1_Channel1_IRQHandler(void)
{
    uint16_t *pBuf;

    if (DMA_GetITStatus(DMA1_IT_HT1)) {
        DMA_ClearITPendingBit(DMA1_IT_HT1);
        pBuf = gDMABuf;
    } else if (DMA_GetITStatus(DMA1_IT_TC1)) {
        DMA_ClearITPendingBit(DMA1_IT_TC1);
        pBuf = gDMABuf + gDMABufPerSize;
    } else {
        DMA_ClearITPendingBit(DMA1_IT_GL1);
        return;
    }

    // 交换ADC1和ADC2的结果，使其按时间顺序排列
    if (gSampleFlags & SAMPLE_FLAG_SWAP_ADC) {
        uint32_t *pBuf32;
        uint32_t tmp;
        size_t bufsize;

        pBuf32 = (uint32_t *)pBuf;
        bufsize = gDMABufPerSize >> 1;;
        while (bufsize--) {
            tmp = *pBuf32;
            tmp = (tmp >> 16) | (tmp << 16);
            *pBuf32++ = tmp;
        }
    }

    // 发送事件
    OnSampleComplete(pBuf, gDMABufPerSize);
}
