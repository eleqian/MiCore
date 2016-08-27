#include <stdio.h>
#include "arm_math.h"
#include "base.h"
#include "diskio.h"
#include "ff.h"
#include "timebase.h"
#include "led.h"
#include "lcd.h"
#include "bitband.h"

// "ELE“图标(32x32)
// 取模方式：从左到右，从上到下为bit0~bit7
/*static const uint8_t graphic_ele[] = {
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

arm_status arm_rfft_init_q15_1024(
    arm_rfft_instance_q15 *S,
    uint32_t fftLenReal,
    uint32_t ifftFlagR,
    uint32_t bitReverseFlag);

#define FFT_POINT       1024

arm_rfft_instance_q15 g_fft_inst;
q15_t g_fft_in[FFT_POINT];
q15_t g_fft_out[FFT_POINT];

void math_test(void)
{
    char buf[32] = "";
    u32 t;
    int i;
    
    for (i = 0; i < FFT_POINT; i = i + 4) {
        g_fft_in[i + 0] = 16384;
        g_fft_in[i + 1] = 16384;
        g_fft_in[i + 2] = -16384;
        g_fft_in[i + 3] = -16384;
    }

    if (ARM_MATH_SUCCESS != arm_rfft_init_q15_1024(&g_fft_inst, FFT_POINT, 0, 1)) {
        return;
    }

    t = micros();
    arm_rfft_q15(&g_fft_inst, g_fft_in, g_fft_out);
    t = micros() - t;
    
    snprintf(buf, sizeof(buf), "FFT t=%dus", t);
    LCD_Clear();
    LCD_DrawString(buf, 0, 8, FONT_SIZE_5X8, 0x3);
    LCD_Refresh();

    while (1);
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
    //LCD_DrawPicture8_1Bpp(graphic_ele, 16, 1, 32, 4, 0x3);
    LCD_DrawString("Hello World!", 0, 0, FONT_SIZE_5X8, 0x3);
    LCD_Refresh();
    LCD_LightOnOff(TRUE);

    math_test();

    while (1) {
        LED_Light(TRUE);
        delay_ms(100);
        LED_Light(FALSE);
        delay_ms(900);
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
