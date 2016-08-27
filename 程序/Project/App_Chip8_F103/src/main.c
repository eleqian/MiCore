#include "base.h"
#include "diskio.h"
#include "ff.h"
#include "timebase.h"
#include "led.h"
#include "lcd.h"
#include "shell.h"

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

// 文件系统数据
static FATFS gFSObj;

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

int main(void)
{
    /*if (IAP_Check()) {
        while (1);
    }*/

    /*while(1) {
        //delay_ms(1);
    }*/
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);

    // 关JTAG，仅使用SWD
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

    TimeBase_Init();
    LED_Init();
    LCD_Init();

    LCD_LightOnOff(TRUE);

    // 初始化存储器
    if (0 != disk_initialize(0)) {
        LCD_Clear();
        LCD_DrawString("Disk init failed!", 0, 0, FONT_SIZE_5X8, 0x3);
        LCD_Refresh();
        while (1);
    }

    // 挂载文件系统对象
    f_mount(&gFSObj, "", 0);

    // 运行chip8模拟器
    shell_main();

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
