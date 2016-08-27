#include "base.h"
#include "timebase.h"
#include "led.h"
#include "lcd.h"
#include "lcr.h"
#include "stdio.h"
#include "skey.h"
#include <math.h>

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

// 关JTAG，仅使用SWD
void DisableJTAG(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);

    // 关JTAG，仅使用SWD
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
}

/*-----------------------------------*/

static skey_t keys[5];

// 按键id枚举
enum {
    KEY_ID_NONE = 0,

    // 核心板按键id
    KEY_ID_S1,
    KEY_ID_S2,
    KEY_ID_S3,
    KEY_ID_S4,
    KEY_ID_S5,

    KEY_ID_MAX
};

void shell_initkeys(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC, ENABLE);

    // 初始化S5端口(PB2)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // 初始化S1~S4端口(PC13~PC15)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
}

BOOL shell_getkeystate(void *pdata)
{
    uint8_t id = (uint8_t)((uintptr_t)pdata & 0xff);

    switch (id) {
    case KEY_ID_S1:
        if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_13) && GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_15)) {
            GPIO_InitTypeDef GPIO_InitStructure;
            BOOL res = FALSE;

            GPIO_ResetBits(GPIOC, GPIO_Pin_13);
            GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
            GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
            GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
            GPIO_Init(GPIOC, &GPIO_InitStructure);
            if (!GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_15)) {
                res = TRUE;
            }
            GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
            GPIO_Init(GPIOC, &GPIO_InitStructure);
            return res;
        } else {
            return FALSE;
        }
    case KEY_ID_S2:
        return !GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_13);
    case KEY_ID_S3:
        return !GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_14);
    case KEY_ID_S4:
        return !GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_15);
    case KEY_ID_S5:
        return !GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_2);
    default:
        return FALSE;
    }
}

skey_t *shell_id2key(uint8_t id)
{
    if (id == KEY_ID_NONE) {
        return NULL;
    }

    return keys + id - 1;
}

/*-----------------------------------*/

// 电源开
void PowerOn(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    // 电源控制引脚 PB11
    GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_SetBits(GPIOB, GPIO_Pin_11);
}

// 电源关
void PowerOff(void)
{
    LCD_PowerSaveOnOff(TRUE);
    LCD_LightOnOff(FALSE);
    LED_Light(FALSE);
    GPIO_ResetBits(GPIOB, GPIO_Pin_11);
    while (1) {
    }
}

/*-----------------------------------*/

int main(void)
{
    int freq = 1;
    cplx Z;
    char buf[32] = "";
    
    PowerOn();

    /*if (IAP_Check()) {
        while (1);
    }*/

    /*while(1) {
        //delay_ms(1);
    }*/

    DisableJTAG();

    TimeBase_Init();
    LED_Init();
    LCD_Init();

    LCR_Init();
    LCR_SetFreq(freq);

    //LCD_Clear();
    //LCD_DrawPicture8_1Bpp(graphic1, 16, 8 / 8, 32, 32 / 8, 0x0, 0x3);
    //LCD_DrawString8("Hello World!", 0, 0, FONT_SIZE_5X8, 0x3);
    //LCD_Refresh();
    LCD_LightOnOff(TRUE);

    shell_initkeys();
    SKey_InitArrayID(keys, 5, shell_getkeystate, 1);

    while (1) {
        float ls, cs;
        float d, q;
        //float d2;

        SKey_UpdateArray(keys, sizeof(keys) / sizeof(skey_t));
        if (SKey_CheckFlag(shell_id2key(KEY_ID_S1), SKEY_FLAG_PRESS)) {
            switch (freq) {
            case 1:
                freq = 9;
                break;
            case 9:
                freq = 25;
                break;
            case 25:
                freq = 49;
                break;
            case 49:
                freq = 97;
                break;
            case 97:
                freq = 1;
                break;
            default:
                break;
            }
            LCR_SetFreq(freq);
        }
        
        if (!SKey_CheckFlag(shell_id2key(KEY_ID_S5), SKEY_FLAG_PRESS_IN) & SKey_CheckFlag(shell_id2key(KEY_ID_S5), SKEY_FLAG_PRESS)) {
            static BOOL gIsPowerOn = FALSE;
            
            if (!gIsPowerOn) {
                gIsPowerOn = TRUE;
            } else {
                LED_Light(TRUE);
                delay_ms(100);
                PowerOff();
            }
            return 0;
        }

        //LED_Light(TRUE);
        LCR_StartMeasure(1);
        while (!LCR_GetMeasure(&Z));
        //LED_Light(FALSE);

        ls = Z.Im / 6.283185307 / (freq * 1000.0);
        cs = -1.0 / 6.283185307 / (freq * 1000.0) / Z.Im;
        d = fabs(Z.Re / Z.Im);
        //d2 = d * d;
        q = 1 / d;

        sprintf(buf, "F : %dkHz", freq);
        LCD_FillRect(0, 2 * 8, 128, 8, 0x0);
        LCD_DrawString(buf, 0, 2 * 8, FONT_SIZE_5X8, 0x3);
        
        sprintf(buf, "Re: %f", Z.Re);
        LCD_FillRect(0, 3 * 8, 128, 8, 0x0);
        LCD_DrawString(buf, 0, 3 * 8, FONT_SIZE_5X8, 0x3);
        
        sprintf(buf, "Im: %f", Z.Im);
        LCD_FillRect(0, 4 * 8, 128, 8, 0x0);
        LCD_DrawString(buf, 0, 4 * 8, FONT_SIZE_5X8, 0x3);
        
        if (cs >= 0 && cs < 1) {
            sprintf(buf, "C : %guF", cs * 1000000);
            LCD_FillRect(0, 5 * 8, 128, 8, 0x0);
            LCD_DrawString(buf, 0, 5 * 8, FONT_SIZE_5X8, 0x3);
        } else {
            LCD_DrawString("C*:", 0, 5 * 8, FONT_SIZE_5X8, 0x3);
        }
        
        if (ls >= 0 && ls < 1) {
            sprintf(buf, "H : %.3guH", ls * 1000000);
            LCD_FillRect(0, 6 * 8, 128, 8, 0x0);
            LCD_DrawString(buf, 0, 6 * 8, FONT_SIZE_5X8, 0x3);
        } else {
            LCD_DrawString("H*:", 0, 6 * 8, FONT_SIZE_5X8, 0x3);
        }
        
        sprintf(buf, "D: %f", d);
        LCD_FillRect(0, 7 * 8, 128, 8, 0x0);
        LCD_DrawString(buf, 0, 7 * 8, FONT_SIZE_5X8, 0x3);
        
        if (q > 9999) {
            q = 9999;
        }
        sprintf(buf, "Q: %f", q);
        LCD_FillRect(0, 8 * 8, 128, 8, 0x0);
        LCD_DrawString(buf, 0, 8 * 8, FONT_SIZE_5X8, 0x3);
        
        LCD_Refresh();

        //delay_ms(250);
        //delay_ms(250);
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
