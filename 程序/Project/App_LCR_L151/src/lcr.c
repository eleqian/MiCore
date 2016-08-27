/* LCR测量模块
 * LCRBoard for MiDOS with STM32L151C8T6
 * 包括波形输出、测量、校准和滤波功能
 * 基于RLC Meter V6 / Neekeetos@yahoo.com
 * eleqian 2014-10-25
 */

#include "base.h"
//#include "math.h"
#include "cmath.h"
#include "lcr.h"

// 正弦波点数
#define SINE_N          500

// 每通道采样波形次数
#define OSR             40

// 输出正弦偏移
#define SINE_OFFSET     900

// 测量通道数（测量三个点电压）
#define CH_CNT          3

// 滤波参数
#define FLT_SID_CNT     2
#define FLT_LEN         5
#define FLT_ALPHA       0.05
#define FLT_ALPHA2      0.1
#define FLT_CLOSE_LIMIT 0.05
//#define MOHM_LIMIT    0.02
//#define GOHM_LIMIT    1e6

// 标准电阻阻值
#define SHUNT           149.8

// ADC各测量通道
static const uint32_t adc_ch_table[CH_CNT] = {1, 2, 7};

// ADC当前测量通道
static volatile uint32_t adc_ch = 1;

// 正弦波形表，多出1/4是方便按90°间隔查找
static int16_t sine_table[SINE_N + SINE_N / 4];

// DAC DMA缓存
static uint32_t dac_buf[SINE_N];

// ADC DMA缓存
static volatile uint16_t adc_buf[SINE_N];

// 测量数据
static volatile cplx mAcc[CH_CNT];

// 标准电阻
static cplx R;

// 开路/短路校准数据
static cplx Zo = {0, 0};
static cplx Zs = {0, 0};

// 是否校准测量
static BOOL LCRIsCalibrated;

// 剩余测量次数
static volatile size_t LCRMeasureRounds;

/*-----------------------------------*/
// 各种外设配置

static void LCR_GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    // Enable GPIOA Periph clock
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);

    // Configure PA4 (DAC_OUT1), PA5 (DAC_OUT2) as analog
    GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_4 | GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // Configure PA1 (ADC_IN1), PA2 (ADC_IN2) and PA7 (ADC_IN7) as analog
    GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    // Configure PA3 and PA6 as output
    GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_3 | GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    GPIO_ResetBits(GPIOA, GPIO_Pin_3 | GPIO_Pin_6);

    // Configure PA0 (TIM2 CH1)
    /*GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource0, GPIO_AF_TIM2);*/
}

static void LCR_TIM_Config(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    // Enable TIM2 clock
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    TIM_DeInit(TIM2);

    // Time base configuration
    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.TIM_Period = 63;  // 32MHz / 64 = 500kHz
    TIM_TimeBaseStructure.TIM_Prescaler = 0;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0x0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    TIM_ARRPreloadConfig(TIM2, ENABLE);

    // Channel1 Configuration (ADC Channel)
    /*TIM_OCInitStructure.TIM_Pulse = 46;
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM2;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC1Init(TIM2, &TIM_OCInitStructure);

    TIM_OC1PreloadConfig(TIM2, ENABLE);*/

    // Channel2 Configuration (ADC)
    TIM_OCInitStructure.TIM_Pulse = 62;
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM2;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC2Init(TIM2, &TIM_OCInitStructure);

    TIM_OC2PreloadConfig(TIM2, ENABLE);

    // Channel3 Configuration (DAC)
    TIM_OCInitStructure.TIM_Pulse = 15;
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM2;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC3Init(TIM2, &TIM_OCInitStructure);

    TIM_OC3PreloadConfig(TIM2, ENABLE);

    // TIM2 TRGO selection (DAC)
    TIM_SelectOutputTrigger(TIM2, TIM_TRGOSource_OC3Ref);

    // Enable TIM2
    TIM_Cmd(TIM2, ENABLE);
}

static void LCR_DMA_Config(void)
{
    DMA_InitTypeDef DMA_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    // Enable DMA1 clock
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    // DMA1 channel1 configuration (ADC)
    DMA_DeInit(DMA1_Channel1);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)adc_buf;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize = SINE_N;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel1, &DMA_InitStructure);

    DMA_ITConfig(DMA1_Channel1, DMA_IT_TC | DMA_IT_HT, ENABLE);
    DMA_Cmd(DMA1_Channel1, ENABLE);

    // DMA1 channel3 configuration (DAC)
    DMA_DeInit(DMA1_Channel3);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&DAC->DHR12RD;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)dac_buf;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize = SINE_N;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Word;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel3, &DMA_InitStructure);

    DMA_Cmd(DMA1_Channel3, ENABLE);

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);

    // Enable DMA1 channel1 IRQ Channel
    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

static void LCR_ADC_Config(void)
{
    ADC_InitTypeDef ADC_InitStructure;

    // Enable the HSI oscillator
    RCC_HSICmd(ENABLE);

    // Check that HSI oscillator is ready
    while (RCC_GetFlagStatus(RCC_FLAG_HSIRDY) == RESET);

    // Enable ADC1 clock
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

    // ADC1 configuration
    ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_Rising;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T2_CC2;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfConversion = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    // ADC1 regular channels configuration
    ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 1, ADC_SampleTime_16Cycles);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_2, 2, ADC_SampleTime_16Cycles);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_7, 3, ADC_SampleTime_16Cycles);

    //ADC_DiscModeChannelCountConfig(ADC1, 1);
    //ADC_DiscModeCmd(ADC1, ENABLE);

    // Enable the request after last transfer for DMA Circular mode
    ADC_DMARequestAfterLastTransferCmd(ADC1, ENABLE);

    // Enable ADC1 DMA
    ADC_DMACmd(ADC1, ENABLE);

    // Enable ADC1
    ADC_Cmd(ADC1, ENABLE);

    // Wait until the ADC1 is ready
    while (ADC_GetFlagStatus(ADC1, ADC_FLAG_ADONS) == RESET);
}

static void LCR_DAC_Config(void)
{
    DAC_InitTypeDef DAC_InitStructure;

    // DAC Periph clock enable
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_DAC, ENABLE);

    // DAC channel1 & 2 Configuration
    DAC_InitStructure.DAC_Trigger = DAC_Trigger_T2_TRGO;
    DAC_InitStructure.DAC_WaveGeneration = DAC_WaveGeneration_None;
    //DAC_InitStructure.DAC_WaveGeneration = DAC_WaveGeneration_Triangle;
    DAC_InitStructure.DAC_LFSRUnmask_TriangleAmplitude = DAC_TriangleAmplitude_15;
    DAC_InitStructure.DAC_OutputBuffer = DAC_OutputBuffer_Enable;
    DAC_Init(DAC_Channel_1, &DAC_InitStructure);
    DAC_Init(DAC_Channel_2, &DAC_InitStructure);

    // Enable DAC Channel1: PA4 is automatically connected to the DAC converter.
    DAC_Cmd(DAC_Channel_1, ENABLE);
    // Enable DAC Channel2: PA5 is automatically connected to the DAC converter.
    DAC_Cmd(DAC_Channel_2, ENABLE);

    DAC_SetDualChannelData(DAC_Align_12b_R, 1000, 1000);

    // Enable DMA for DAC Channel2
    DAC_DMACmd(DAC_Channel_2, ENABLE);
}

/*-----------------------------------*/

// LCR初始化
void LCR_Init(void)
{
    R.Re = SHUNT;
    R.Im = 0;
    LCRIsCalibrated = FALSE;

    LCR_GPIO_Config();
    LCR_ADC_Config();
    LCR_DAC_Config();
    LCR_DMA_Config();
    LCR_TIM_Config();
}

// 设置测量频率
// 参数：正弦频率
void LCR_SetFreq(int freq_k)
{
    const uint32_t k = (SINE_OFFSET | (SINE_OFFSET << 16));
    int32_t pp;
    int32_t s;
    size_t i;

    // 填充正弦波形表
    for (i = 0; i < SINE_N; i++) {
        pp = (int32_t)(((int64_t)freq_k * i << 32) / SINE_N);    //<<16;
        s = cordic_y(pp);
        s = ((s >> 14) + 1) >> 1;
        sine_table[i] = s;
    }

    // 填充重复的1/4波形
    for (i = 0; i < SINE_N / 4; i++) {
        sine_table[SINE_N + i] = sine_table[i];
    }

    // 填充DAC波形缓存
    for (i = 0; i < SINE_N; i++) {
        s = (sine_table[i] >> 7) & 0xffff;
        dac_buf[i] = k + s - (s << 16);
    }

    // 每次切换频率后需要重新设置校准数据
    LCRIsCalibrated = FALSE;
}

// 设置校准数据
// 参数：开路阻抗，短路阻抗
void LCR_SetCalibrate(cplx *cZo, cplx *cZs)
{
    Zo = *cZo;
    Zs = *cZs;
    LCRIsCalibrated = TRUE;
}

/*-----------------------------------*/

// 滤波
static float LCR_Filter(size_t sidx, float new_in)
{
    static float state[FLT_SID_CNT];
    static int cnt[FLT_SID_CNT];
    float t = (state[sidx] - new_in);
    float sc = FLT_CLOSE_LIMIT * state[sidx];

    if ((t > sc) || (t < -sc)) {
        if (t > 0) {
            cnt[sidx]++;
        } else {
            cnt[sidx]--;
        }
        state[sidx] = state[sidx] * (1 - FLT_ALPHA2) + new_in * FLT_ALPHA2;
    } else {
        state[sidx] = state[sidx] * (1 - FLT_ALPHA) + new_in * FLT_ALPHA;
        if (t < 0) {
            cnt[sidx]++;
        } else {
            cnt[sidx]--;
        }
    }

    if ((cnt[sidx] > 2 * FLT_LEN) || (-cnt[sidx] > 2 * FLT_LEN)) {
        state[sidx] = new_in;
        cnt[sidx] = 0;
        /*if(cnt[sidx] > 0) {
            cnt[sidx] = FLT_LEN;
        } else {
            cnt[sidx] = -FLT_LEN;
        }*/
    }

    return state[sidx];
}

// 开始LCR测量
// 参数：测量次数
void LCR_StartMeasure(size_t rounds)
{
    mAcc[0].Re = 0;
    mAcc[0].Im = 0;
    mAcc[1].Re = 0;
    mAcc[1].Im = 0;
    mAcc[2].Re = 0;
    mAcc[2].Im = 0;
    LCRMeasureRounds = rounds;
}

// 获取测量结果
// 参数：测量的阻抗值（输出）
// 返回：是否成功，未测量完成时返回失败
BOOL LCR_GetMeasure(cplx *Z)
{
    cplx Vr;

    if (LCRMeasureRounds > 0) {
        return FALSE;
    }

    // Z = Vz/Iz = Vz/(Vr/R) = Vz*R/Vr
    Z->Re = (mAcc[2].Re - mAcc[0].Re); // Vz
    Z->Im = (mAcc[2].Im - mAcc[0].Im);
    cplxMul(Z, &R);
    Vr.Re = (mAcc[0].Re - mAcc[1].Re); // Vr
    Vr.Im = (mAcc[0].Im - mAcc[1].Im);
    cplxDiv(Z, &Vr);

    // 如果有校准数据则进行校准
    if (LCRIsCalibrated) {
        cplx tmp1, tmp2;

        // Zx = Zo * (Zs - Zx) / (Zx - Zo)
        tmp1.Re = Zs.Re - Z->Re;
        tmp1.Im = Zs.Im - Z->Im;
        tmp2.Re = Zo.Re;
        tmp2.Im = Zo.Im;

        cplxMul(&tmp2, &tmp1);
        tmp1.Re = Z->Re - Zo.Re;
        tmp1.Im = Z->Im - Zo.Im;

        cplxDiv(&tmp2, &tmp1);
        Z->Re = tmp2.Re;
        Z->Im = tmp2.Im;
    }

    // 滤波
    //Z->Re = LCR_Filter(0, Z->Re);
    //Z->Im = LCR_Filter(1, Z->Im);

    return TRUE;
}

/*-----------------------------------*/

// ADC DMA中断
void DMA1_Channel1_IRQHandler(void)
{
    static int64_t mreal[CH_CNT];
    static int64_t mimag[CH_CNT];
    static size_t process = 0;     // 采样次数
    static size_t smp_idx = 0;     // 采样点序号
    static size_t ch_idx = 0;      // 测量通道序号

    if (process > 0) {
        int32_t smp;
        size_t i;

        // 将采样数据与正弦波相乘得到实部和虚部
        for (i = 0; i < SINE_N / 2; i++) {
            smp = adc_buf[smp_idx] - SINE_OFFSET;
            mreal[ch_idx] += ((int32_t)sine_table[smp_idx + SINE_N / 4] * smp);
            mimag[ch_idx] -= ((int32_t)sine_table[smp_idx] * smp);
            smp_idx++;
        }

        if (smp_idx >= SINE_N) {
            smp_idx = 0;
        }
    }

    // 当有DMA传输完成中断算一次采样完成
    if (DMA_GetITStatus(DMA1_IT_HT1)) {
        DMA_ClearITPendingBit(DMA1_IT_HT1);
    } else if (DMA_GetITStatus(DMA1_IT_TC1)) {
        process++;
        smp_idx = 0;
        DMA_ClearITPendingBit(DMA1_IT_TC1);
    } else {
        DMA_ClearITPendingBit(DMA1_IT_GL1);
    }

    // 达到一定采样次数
    if (process > OSR) {
        process = 0;

        // 测量下一个通道
        ch_idx++;
        if (ch_idx >= CH_CNT) {
            ch_idx = 0;
        }
        adc_ch = adc_ch_table[ch_idx];
        ADC1->SQR5 = adc_ch;
        smp_idx = 0;

        // 当所有通道测量完成，并且达到指定次数时结束
        if ((ch_idx == 0) && (LCRMeasureRounds > 0)) {
            mAcc[0].Re += mreal[0];
            mAcc[0].Im += mimag[0];
            mAcc[1].Re += mreal[1];
            mAcc[1].Im += mimag[1];
            mAcc[2].Re += mreal[2];
            mAcc[2].Im += mimag[2];
            LCRMeasureRounds--;
        }

        mreal[ch_idx] = 0;
        mimag[ch_idx] = 0;
    }
}
