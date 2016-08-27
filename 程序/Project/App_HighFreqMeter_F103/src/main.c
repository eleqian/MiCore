#include <stdio.h>
#include "base.h"
#include "diskio.h"
#include "ff.h"
#include "timebase.h"
#include "led.h"
#include "lcd.h"

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

// PWM输出初始化
void PWM_Init_1K(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
    TIM_OCInitTypeDef  TIM_OCInitStructure;
    uint16_t TimerPeriod = 0;
    uint16_t Channel2Pulse = 0, Channel3Pulse = 0;

    /* TIM1, GPIOA and AFIO clocks enable */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1 | RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);

    /* GPIOA Configuration: Channel 2 and 3 as alternate function push-pull */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* TIM1 Configuration ---------------------------------------------------
     Generate 2 PWM signals with 2 different duty cycles:
     TIM1CLK = SystemCoreClock, Prescaler = 0, TIM1 counter clock = SystemCoreClock

     The objective is to generate 2 PWM signal at 1 KHz:
       - TIM1_Period = (SystemCoreClock / 1000) - 1
     The channel 2 duty cycle is set to 50%
     The channel 3 duty cycle is set to 25%
     The Timer pulse is calculated as follows:
       - ChannelxPulse = DutyCycle * (TIM1_Period - 1) / 100
    ----------------------------------------------------------------------- */
    /* Compute the value to be set in ARR regiter to generate signal frequency at 1 KHz */
    TimerPeriod = (SystemCoreClock / 2 / 1000) - 1;
    /* Compute CCR2 value to generate a duty cycle at 50%  for channel 2 */
    Channel2Pulse = (uint16_t)(((uint32_t) 50 * (TimerPeriod - 1)) / 100);
    /* Compute CCR3 value to generate a duty cycle at 25%  for channel 3 */
    Channel3Pulse = (uint16_t)(((uint32_t) 25 * (TimerPeriod - 1)) / 100);

    /* Time Base configuration */
    TIM_TimeBaseStructure.TIM_Prescaler = 1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_Period = TimerPeriod;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;

    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);

    /* Channel 2, 3 Configuration in PWM mode */
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM2;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Disable;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_Low;
    TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_High;
    TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Set;
    TIM_OCInitStructure.TIM_OCNIdleState = TIM_OCIdleState_Reset;

    TIM_OCInitStructure.TIM_Pulse = Channel2Pulse;
    TIM_OC2Init(TIM1, &TIM_OCInitStructure);

    TIM_OCInitStructure.TIM_Pulse = Channel3Pulse;
    TIM_OC3Init(TIM1, &TIM_OCInitStructure);

    /* TIM1 counter enable */
    TIM_Cmd(TIM1, ENABLE);

    /* TIM1 Main Output Enable */
    TIM_CtrlPWMOutputs(TIM1, ENABLE);
}

// Select: ChipSelect pin low
#define SPI_SelectDevice()      GPIO_ResetBits(GPIOA, GPIO_Pin_4)
// Deselect: ChipSelect pin high
#define SPI_DeselectDevice()    GPIO_SetBits(GPIOA, GPIO_Pin_4)

/*
* Initializes the peripherals used by the DSO driver.
*/
void DSO_Init(void)
{
    SPI_InitTypeDef SPI_InitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);

    // 配置SCK, MOSI
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    // 配置MISO
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 配置CS
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    SPI_DeselectDevice();

    // 使能SPI1时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1 , ENABLE);

    // SPI1配置
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;  // 72/8 = 9Mbps
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SPI1, &SPI_InitStructure);

    // 使能SPI1
    SPI_Cmd(SPI1, ENABLE);
}

/*
* Description : Sends a byte through the SPI interface and return the byte received from the SPI bus.
* Input       : byte : byte to send.
* Output      : None
* Return      : The value of the received byte.
*/
uint8_t DSO_TransmitByte(uint8_t byte)
{
    /* Loop while DR register in not emplty */
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET);
    /* Send byte through the SPI peripheral */
    SPI_I2S_SendData(SPI1, byte);
    /* Wait to receive a byte */
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);
    /* Return the byte read from the SPI bus */
    return SPI_I2S_ReceiveData(SPI1);
}

// 写一字节数据
void DSO_WriteByte(uint8_t addr, uint8_t dat)
{
    SPI_SelectDevice();
    DSO_TransmitByte((addr << 1) | 0x1);
    DSO_TransmitByte(dat);
    SPI_DeselectDevice();
}

// 读一字节数据
uint8_t DSO_ReadByte(uint8_t addr)
{
    uint8_t dat;
    
    SPI_SelectDevice();
    DSO_TransmitByte((addr << 1) | 0x0);
    dat = DSO_TransmitByte(0x00);
    SPI_DeselectDevice();
    
    return dat;
}

// CPLD时钟频率(50MHz)
#define CPLD_CLK_FREQ       (50000000)

void DSO_Test(void)
{
    uint32_t fs_cnt;
    uint32_t fx_cnt;
    uint8_t status;
    float freqx;
    uint32_t cnt;
    char buff[32] = "";
    
    while (1) {
        // 清零
        DSO_WriteByte(0x0, 0x1);
        DSO_WriteByte(0x0, 0x0);
        // 启动
        DSO_WriteByte(0x0, 0x2);
        cnt = 0xffff;
        do {
            status = DSO_ReadByte(0x0);
        } while (!(status & 0x4) && cnt--);
        // 延时
        delay_ms(10);
        // 结束
        DSO_WriteByte(0x0, 0x0);
        do {
            status = DSO_ReadByte(0x0);
        } while (status & 0x4);
        
        // 读取标准频率计数
        fs_cnt = DSO_ReadByte(0x3);
        fs_cnt <<= 8;
        fs_cnt |= DSO_ReadByte(0x2);
        fs_cnt <<= 8;
        fs_cnt |= DSO_ReadByte(0x1);
        // 读取待测频率计数
        fx_cnt = DSO_ReadByte(0x6);
        fx_cnt <<= 8;
        fx_cnt |= DSO_ReadByte(0x5);
        fx_cnt <<= 8;
        fx_cnt |= DSO_ReadByte(0x4);
        
        if ((0 == fs_cnt) || (0 == fx_cnt)) {
            freqx = 0;
        } else {
            freqx = (float)fx_cnt * CPLD_CLK_FREQ / fs_cnt;
        }
        
        LCD_Clear();
        snprintf(buff, sizeof(buff), "status: %u", status);
        LCD_DrawString(buff, 0, 0, FONT_SIZE_5X8, 0x3);
        snprintf(buff, sizeof(buff), "fs_cnt: %u", fs_cnt);
        LCD_DrawString(buff, 0, 8, FONT_SIZE_5X8, 0x3);
        snprintf(buff, sizeof(buff), "fx_cnt: %u", fx_cnt);
        LCD_DrawString(buff, 0, 16, FONT_SIZE_5X8, 0x3);
        snprintf(buff, sizeof(buff), "freqx : %g", freqx);
        LCD_DrawString(buff, 0, 24, FONT_SIZE_5X8, 0x3);
        LCD_Refresh();
    }
}

/*-----------------------------------*/

int main(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);

    // 关JTAG，仅使用SWD
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

    TimeBase_Init();
    LED_Init();
    LCD_Init();

    LCD_Clear();
    LCD_DrawPicture8_1Bpp(graphic1, 16, 1, 32, 4, 0x3);
    //LCD_DrawString("Hello World!", 0, 0, FONT_SIZE_5X8, 0x3);
    LCD_Refresh();
    LCD_LightOnOff(TRUE);
    
    PWM_Init_1K();
    DSO_Init();
    
    DSO_Test();
    
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
