/* 数控电源
 * eleqian 2014-12-16
 */

#include "base.h"
#include "led.h"

/*-----------------------------------*/

// 芯片校准数据
#define VREFINT_CAL     (*(uint16_t *)0x1FF80078)
#define TS_CAL1         (*(uint16_t *)0x1FF8007A)
#define TS_CAL2         (*(uint16_t *)0x1FF8007E)

// 单片机电源电压（作为ADC和DAC参考电压）(mV)
static uint16_t sDCP_VDD_mV = 3335;
// 电流采样电阻大小(mR)
static uint16_t sDCP_SampleR_mR = 33;
// 输入电压衰减倍数(x10)
static uint16_t sDCP_Vin_Scale_x10 = 110;
// 输出电压衰减倍数(x10)
static uint16_t sDCP_Vout_Scale_x10 = 61;
// 输出电流衰减倍数(x1档)
static uint16_t sDCP_Iout_Scale_x1 = 50;
// 输出电流衰减倍数(x10档)
static uint16_t sDCP_Iout_Scale_x10 = 500;

// 测量通道枚举
enum {
    DCP_MEAS_CH_VOUT = 0,       // 输出电压
    DCP_MEAS_CH_IOUT,           // 输出电流x1档
    DCP_MEAS_CH_IOUT_X10,       // 输出电流x10档
    DCP_MEAS_CH_VIN,            // 输入电压
    DCP_MEAS_CH_TEMP,           // 温度
    DCP_MEAS_CH_VREF,           // 参考电压

    DCP_MEAS_CH_MAX
};

// 电压和电流测量数据
static uint16_t sDCP_Measures[DCP_MEAS_CH_MAX];

/*-----------------------------------*/

static void DCP_DMA_Config(void)
{
    DMA_InitTypeDef DMA_InitStructure;

    // Enable DMA1 clock
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    // DMA1 channel1 configuration (ADC)
    DMA_DeInit(DMA1_Channel1);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)sDCP_Measures;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize = sizeof(sDCP_Measures) / sizeof(sDCP_Measures[0]);
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel1, &DMA_InitStructure);

    DMA_Cmd(DMA1_Channel1, ENABLE);
}

static void DCP_ADC_Config(void)
{
    ADC_InitTypeDef ADC_InitStructure;

    // Enable the HSI oscillator (16Mhz)
    RCC_HSICmd(ENABLE);
    // Check that HSI oscillator is ready
    while (RCC_GetFlagStatus(RCC_FLAG_HSIRDY) == RESET);

    // Enable ADC1 clock
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

    ADC_TempSensorVrefintCmd(ENABLE);

    // ADC1 configuration
    ADC_StructInit(&ADC_InitStructure);
    ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;
    ADC_InitStructure.ADC_ScanConvMode = ENABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
    ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
    //ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T2_CC2;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfConversion = 6;
    ADC_Init(ADC1, &ADC_InitStructure);

    // ADC1 regular channels configuration
    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_96Cycles);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 2, ADC_SampleTime_96Cycles);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_2, 3, ADC_SampleTime_96Cycles);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_3, 4, ADC_SampleTime_96Cycles);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_16, 5, ADC_SampleTime_192Cycles);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_17, 6, ADC_SampleTime_192Cycles);

    //ADC_DiscModeChannelCountConfig(ADC1, 1);
    //ADC_DiscModeCmd(ADC1, ENABLE);

    // Define delay between ADC1 conversions
    ADC_DelaySelectionConfig(ADC1, ADC_DelayLength_Freeze);
    // Enable ADC1 Power Down during Delay
    ADC_PowerDownCmd(ADC1, ADC_PowerDown_Idle_Delay, ENABLE);

    // Enable the request after last transfer for DMA Circular mode
    ADC_DMARequestAfterLastTransferCmd(ADC1, ENABLE);
    // Enable ADC1 DMA
    ADC_DMACmd(ADC1, ENABLE);

    // Enable ADC1
    ADC_Cmd(ADC1, ENABLE);
    // Wait until the ADC1 is ready
    while (ADC_GetFlagStatus(ADC1, ADC_FLAG_ADONS) == RESET);

    // Start ADC1 Software Conversion 
    ADC_SoftwareStartConv(ADC1);
    // Wait until ADC Channel end of conversion
    //while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);
}

static void DCP_DAC_Config(void)
{
    DAC_InitTypeDef DAC_InitStructure;

    // DAC Periph clock enable
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_DAC, ENABLE);

    // DAC channel1 & 2 Configuration
    DAC_StructInit(&DAC_InitStructure);
    DAC_InitStructure.DAC_Trigger = DAC_Trigger_None;
    DAC_InitStructure.DAC_WaveGeneration = DAC_WaveGeneration_None;
    //DAC_InitStructure.DAC_LFSRUnmask_TriangleAmplitude = DAC_TriangleAmplitude_1;
    DAC_InitStructure.DAC_OutputBuffer = DAC_OutputBuffer_Disable;
    DAC_Init(DAC_Channel_1, &DAC_InitStructure);
    DAC_Init(DAC_Channel_2, &DAC_InitStructure);

    // Enable DAC Channel1: PA4 is automatically connected to the DAC converter.
    DAC_Cmd(DAC_Channel_1, ENABLE);
    // Enable DAC Channel2: PA5 is automatically connected to the DAC converter.
    DAC_Cmd(DAC_Channel_2, ENABLE);

    DAC_SetChannel1Data(DAC_Align_12b_R, 0);
    DAC_SetChannel2Data(DAC_Align_12b_R, 0);
}

static void DPC_IO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA | RCC_AHBPeriph_GPIOB, ENABLE);

    // 电源控制引脚 PB6
    GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    // 电流量程选择引脚 PB11
    GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // DAC输出引脚 PA4(DAC_OUT1), PA5(DAC_OUT2)
    GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_4 | GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // ADC输入引脚 PA0(ADC_IN0), PA1(ADC_IN1), PA2(ADC_IN2) and PA3(ADC_IN3)
    GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

/*-----------------------------------*/

// 初始化
void DCP_Init(void)
{
    DPC_IO_Config();
    DCP_DMA_Config();
    DCP_ADC_Config();
    DCP_DAC_Config();
}

// 电源开
void DCP_PowerOn(void)
{
    LED_Light(TRUE);
    GPIO_SetBits(GPIOB, GPIO_Pin_6);
}

// 电源关
void DCP_PowerOff(void)
{
    LED_Light(FALSE);
    GPIO_ResetBits(GPIOB, GPIO_Pin_6);
}

// 设置输出电压(mV)
void DCP_SetVout(uint32_t mv)
{
    uint16_t val;
    
    // dac = 4096 * (v / scale) / vdd
    //     = 10 * 4096 * v_mv / scale_x10 / vdd_mv
    val = 10 * 4096 * mv / sDCP_Vout_Scale_x10 / sDCP_VDD_mV;
    DAC_SetChannel2Data(DAC_Align_12b_R, val);
}

// 设置输出电流(uA)
void DCP_SetIout(uint32_t ua)
{
    uint16_t val;
    
    // dac = 4096 * (i * r * scale / vdd)
    //     = 4096 * (i_ua / 1000000) * r_mr * scale / vdd_mv
    //     = i_ua * r_mr / vdd_mv * 4096 * scale / 1000000
    if (ua > 200000) {
        val = ua * sDCP_SampleR_mR / sDCP_VDD_mV * 4096 * sDCP_Iout_Scale_x1 / 1000000;
        GPIO_SetBits(GPIOB, GPIO_Pin_11);   // x1
        DAC_SetChannel1Data(DAC_Align_12b_R, val);
    } else {
        val = ua * sDCP_SampleR_mR / sDCP_VDD_mV * 4096 * sDCP_Iout_Scale_x10 / 1000000;
        GPIO_ResetBits(GPIOB, GPIO_Pin_11); // x10
        DAC_SetChannel1Data(DAC_Align_12b_R, val);
    }
}

// 获取实际VDDA(mV)
uint32_t DCP_GetVDDA(void)
{
    uint32_t vdda_mv;

    // vdda = 3V * VREFINT_CAL / VREFINT_DATA
    vdda_mv = 3000 * VREFINT_CAL / sDCP_Measures[DCP_MEAS_CH_VREF];

    // 自动更新电压值供其它功能使用
    sDCP_VDD_mV = vdda_mv;

    return vdda_mv;
}

// 获取输入电压(mV)
uint32_t DCP_GetVin(void)
{
    uint32_t vin;
    
    vin = sDCP_Vin_Scale_x10 * sDCP_VDD_mV * sDCP_Measures[DCP_MEAS_CH_VIN] / 4096 / 10;
    
    return vin;
}

// 获取输出电压(mV)
uint32_t DCP_GetVout(void)
{
    uint32_t vout;
    
    vout = sDCP_Vout_Scale_x10 * sDCP_VDD_mV * sDCP_Measures[DCP_MEAS_CH_VOUT] / 4096 / 10;
    
    return vout;
}

// 获取输出电流(uA)
uint32_t DCP_GetIout(void)
{
    uint16_t adcval, adcval_x10;
    uint32_t iout;
    
    adcval = sDCP_Measures[DCP_MEAS_CH_IOUT];
    adcval_x10 = sDCP_Measures[DCP_MEAS_CH_IOUT_X10];
    // i = v / scale / r
    //   = (vdd * adc / 4096) / scale / r
    //   = (vdd * adc / 4096) / scale / r;
    //   = vdd_mv * adc / 4096 / scale / r_mr
    if (adcval_x10 < 4000 && adcval < 400) {
        iout = (uint64_t)(1000 * 1000) * sDCP_VDD_mV * adcval_x10 / 4096 / sDCP_Iout_Scale_x10 / sDCP_SampleR_mR;
    } else {
        iout = (uint64_t)(1000 * 1000) * sDCP_VDD_mV * adcval / 4096 / sDCP_Iout_Scale_x1 / sDCP_SampleR_mR;
    }
    
    return iout;
}

// 获取芯片温度(0.1C)
int16_t DCP_GetTemp(void)
{
    int16_t temp_x10;
    int32_t val, val1;

    val = sDCP_Measures[DCP_MEAS_CH_TEMP];
    // 转换成3V VDDA时测量值
    val1 = val * sDCP_VDD_mV / 3000;
    // temperature = (110 - 30) / (TS_CAL2 - TS_CAL1) * (TS_DATA - TS_CAL1) + 30
    temp_x10 = 10 * (110 - 30) * (val1 - TS_CAL1) / (TS_CAL2 - TS_CAL1) + 10 * 30;

    return temp_x10;
}
