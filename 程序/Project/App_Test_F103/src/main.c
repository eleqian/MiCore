#include <stdio.h>
#include "base.h"
#include "diskio.h"
#include "ff.h"
#include "timebase.h"
#include "led.h"
#include "lcd.h"
#include "bitband.h"

//#define LIGHT_SENSE

// "ELE“图标(32x32)
// 取模方式：从左到右，从上到下为bit0~bit7
/*static const uint8_t graphic1[] = {
    0x07, 0xFF, 0xFE, 0x1C, 0x1C, 0x38, 0x70, 0xF0, 0xE0, 0x80, 0x40, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x60, 0xF0, 0xF0, 0x38, 0x18, 0x1C, 0x1C, 0xBE, 0x7E,
    0x00, 0xFF, 0xFF, 0x18, 0x18, 0x38, 0xFE, 0xFE, 0x01, 0x01, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x00, 0x00, 0xFF, 0xFF, 0x18, 0x18, 0x38, 0xFF, 0xFF, 0x00,
    0x80, 0xFF, 0xFF, 0xE0, 0x60, 0x30, 0x38, 0x1C, 0x1F, 0x07, 0x0C, 0x0C, 0x0F, 0x07, 0x07, 0x04,
    0x04, 0x04, 0x06, 0x0E, 0x0F, 0x07, 0x08, 0x18, 0x1F, 0x3F, 0x38, 0x70, 0x60, 0xE0, 0xF0, 0xFC,
    0x0F, 0x3F, 0x3E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E,
    0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x3E, 0x3F, 0x1F
};*/

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

#ifdef LIGHT_SENSE
#define LIGHT_CNT_MAX       (10000000)

void TestLightSense(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    char buf[32];
    uint32_t tickbegin, tickend;
    uint32_t cnt;
    
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    // GPIO Configuration: PA2/PA3
    GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_2 | GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    GPIO_ResetBits(GPIOA, GPIO_Pin_2 | GPIO_Pin_3);

    while (1)
    {
        LCD_Clear();
        
        cnt = LIGHT_CNT_MAX;
        // set output mode: PA0
        GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
        GPIO_Init(GPIOA, &GPIO_InitStructure);
        // pull high: PA0
        GPIO_SetBits(GPIOA, GPIO_Pin_0);
        delay_ms(10);
        // set input mode: PA0
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
        GPIO_Init(GPIOA, &GPIO_InitStructure);
        tickbegin = micros();
        // wait low: PA0
        while (((GPIOA->IDR & GPIO_Pin_0) == GPIO_Pin_0) && (cnt > 0))
        {
            cnt--;
        }
        tickend = micros();
        // display
        snprintf(buf, sizeof(buf), "0: t=%u", tickend - tickbegin);
        LCD_DrawString(buf, 0, 0, FONT_SIZE_5X8, 0x3);
        snprintf(buf, sizeof(buf), "   c=%u", LIGHT_CNT_MAX - cnt);
        LCD_DrawString(buf, 0, 8, FONT_SIZE_5X8, 0x3);
        
#if 0
        cnt = LIGHT_CNT_MAX;
        // set output mode: PA1
        GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
        GPIO_Init(GPIOA, &GPIO_InitStructure);
        // pull high: PA1
        GPIO_SetBits(GPIOA, GPIO_Pin_1);
        delay_ms(10);
        // set input mode: PA1
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
        GPIO_Init(GPIOA, &GPIO_InitStructure);
        tickbegin = micros();
        // wait low: PA1
        while (((GPIOA->IDR & GPIO_Pin_1) == GPIO_Pin_1) && (cnt > 0))
        {
            cnt--;
        }
        tickend = micros();
        // display
        snprintf(buf, sizeof(buf), "1: t=%u", tickend - tickbegin);
        LCD_DrawString(buf, 0, 18, FONT_SIZE_5X8, 0x3);
        snprintf(buf, sizeof(buf), "   c=%u", LIGHT_CNT_MAX - cnt);
        LCD_DrawString(buf, 0, 24, FONT_SIZE_5X8, 0x3);
#endif
        
        LCD_Refresh();
        delay_ms(250);
    }
}
#endif

u8 read_buf[10000];

void TestParBus(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    char buf[32] = "";
    u32 tbegin, tend;
    //u32 i;
    //u32 val;
    vu8 *pdin;
    vu32 *pclk;
    u8 *pbuf;
    u8 *pend;
    
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    // GPIO Configuration: PA9
    GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    GPIO_ResetBits(GPIOA, GPIO_Pin_9);
    
    GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    while (1)
    {
        pdin = (vu8 *)&(GPIOA->IDR);
        pclk = (vu32*)BITBAND(GPIOA_ODR_Addr, 11);
        pbuf = read_buf;
        pend = read_buf + sizeof(read_buf);
        LCD_Clear();
        
        tbegin = micros();
        while (pbuf < pend)
        {
            *pclk = 1;
            *pbuf++ = *pdin;
            *pclk = 0;
            *pbuf++ = *pdin;
            *pclk = 1;
            *pbuf++ = *pdin;
            *pclk = 0;
            *pbuf++ = *pdin;
        }
        tend = micros();
        
        snprintf(buf, sizeof(buf), "t=%u", tend - tbegin);
        LCD_DrawString(buf, 0, 18, FONT_SIZE_5X8, 0x3);
        LCD_Refresh();
        
        delay_ms(250);
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
    //LCD_DrawPicture8_1Bpp(graphic1, 16, 1, 32, 4, 0x3);
    //LCD_DrawString("Hello World!", 0, 0, FONT_SIZE_5X8, 0x3);
    LCD_Refresh();
    LCD_LightOnOff(TRUE);
    
#ifdef LIGHT_SENSE
    TestLightSense();
#endif
    
    TestParBus();
    
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
