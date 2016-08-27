#include "base.h"
#include <stdio.h>
#include "timebase.h"
#include "led.h"
#include "lcd.h"
#include "skey.h"
#include "dcp.h"

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
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOC, ENABLE);

    // 初始化KEY4端口(PC15)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    // KEY4按下时为低电平，表示进入IAP
    isInto = !GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_15);

    // 复位按键端口状态
    GPIO_DeInit(GPIOC);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOC, DISABLE);

    return isInto;
}

// 关JTAG，仅使用SWD
void DisableJTAG(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    //RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA | RCC_AHBPeriph_GPIOB, ENABLE);
    GPIO_StructInit(&GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_4;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
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

void shell_initkeys_io(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOB | RCC_AHBPeriph_GPIOC, ENABLE);

    // 初始化S5端口(PB2)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // 初始化S1~S4端口(PC13~PC15)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    GPIO_SetBits(GPIOC, GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15);
}

BOOL shell_getkeystate(void *pdata)
{
    uint8_t id = (uint8_t)((uintptr_t)pdata & 0xff);

    switch (id) {
    case KEY_ID_S1:
        if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_13) && GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_15)) {
            BOOL res = FALSE;

            GPIO_ResetBits(GPIOC, GPIO_Pin_13);
            if (!GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_15)) {
                res = TRUE;
            }
            GPIO_SetBits(GPIOC, GPIO_Pin_13);
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

#define DCP_VIN_MIN         5000
#define DCP_VIN_MAX         24000
#define DCP_VOUT_MIN        0
#define DCP_VOUT_MAX        20000
#define DCP_VOUT_PCT_MAX    90
#define DCP_IOUT_MIN        0
#define DCP_IOUT_MAX        2000000

// 电压电流测量值
static uint32_t sVin, sVout, sIout;
// 电压电流设定值
static uint32_t sVset, sIset;
// 电源输出状态
static BOOL isPowerOn = FALSE;
// 温度
static int16_t sTemperature;

// 设定状态枚举
enum {
    DCP_XC_VSET,
    DCP_XC_ISET,
    DCP_XC_MAX
};

static const uint8_t cVCIdxMin = 1;
static const uint8_t cVCIdxMax = 4;
static const uint8_t cICIdxMin = 2;
static const uint8_t cICIdxMax = 6;
static const uint32_t cIndexGrade[] = {1, 10, 100, 1000, 10000, 100000, 1000000};

static uint8_t sVCIndex = 1;
static uint8_t sICIndex = 2;
static uint8_t sXCIndex = DCP_XC_VSET;

void DealKeys(void)
{
    if (SKey_CheckFlagAny(shell_id2key(KEY_ID_S1), SKEY_FLAG_PRESS)) {
        sXCIndex++;
        if (sXCIndex >= DCP_XC_MAX) {
            sXCIndex = 0;
        }
    }
    
    if (SKey_CheckFlagAny(shell_id2key(KEY_ID_S2), SKEY_FLAG_PRESS | SKEY_FLAG_REPEAT)) {
        if (sXCIndex == DCP_XC_VSET) {
            sVCIndex++;
            if (sVCIndex > cVCIdxMax) {
                sVCIndex = cVCIdxMin;
            }
        } else if (sXCIndex == DCP_XC_ISET) {
            sICIndex++;
            if (sICIndex > cICIdxMax) {
                sICIndex = cICIdxMin;
            }
        }
    }
    
    if (SKey_CheckFlagAny(shell_id2key(KEY_ID_S3), SKEY_FLAG_PRESS | SKEY_FLAG_REPEAT)) {
        if (sXCIndex == DCP_XC_VSET) {
            sVset += cIndexGrade[sVCIndex];
            if (sVset > DCP_VOUT_MAX) {
                sVset = DCP_VOUT_MAX;
            }
            if (sVset > sVin * DCP_VOUT_PCT_MAX / 100) {
                sVset = sVin * DCP_VOUT_PCT_MAX / 100;
            }
            DCP_SetVout(sVset);
        } else if (sXCIndex == DCP_XC_ISET) {
            sIset += cIndexGrade[sICIndex];
            if (sIset > DCP_IOUT_MAX) {
                sIset = DCP_IOUT_MAX;
            }
            DCP_SetIout(sIset);
        }
    }
    
    if (SKey_CheckFlagAny(shell_id2key(KEY_ID_S4), SKEY_FLAG_PRESS | SKEY_FLAG_REPEAT)) {
        if (sXCIndex == DCP_XC_VSET) {
            sVset -= cIndexGrade[sVCIndex];
            if ((int32_t)sVset < DCP_VOUT_MIN) {
                sVset = DCP_VOUT_MIN;
            }
            DCP_SetVout(sVset);
        } else if (sXCIndex == DCP_XC_ISET) {
            sIset -= cIndexGrade[sICIndex];
            if ((int32_t)sIset < DCP_IOUT_MIN) {
                sIset = DCP_IOUT_MIN;
            }
            DCP_SetIout(sIset);
        }
    }
    
    if (SKey_CheckFlagAny(shell_id2key(KEY_ID_S5), SKEY_FLAG_PRESS)) {
        if (isPowerOn) {
            DCP_PowerOff();
            isPowerOn = FALSE;
        } else {
            DCP_PowerOn();
            isPowerOn = TRUE;
        }
    }
}

void Display(void)
{
    char buf[32] = "";
    
    LCD_Clear();
    sprintf(buf, "VDDA: %dmV", DCP_GetVDDA());
    LCD_DrawString8(buf, 0, 7, FONT_SIZE_5X8, 0x3);

    sprintf(buf, "OUT: %s", isPowerOn ? "ON" : "OFF");
    LCD_DrawString8(buf, 0, 0, FONT_SIZE_5X8, 0x3);
    sprintf(buf, "Vin: %05dmV", sVin);
    LCD_DrawString8(buf, 0, 1, FONT_SIZE_5X8, 0x3);
    sprintf(buf, "Vout: %05dmV", sVout);
    LCD_DrawString8(buf, 0, 2, FONT_SIZE_5X8, 0x3);
    sprintf(buf, "Iout: %07duA", sIout);
    LCD_DrawString8(buf, 0, 3, FONT_SIZE_5X8, 0x3);
    sprintf(buf, "Vset: %02d.%02dV", sVset / 1000, sVset % 1000 / 10);
    LCD_DrawString8(buf, 0, 4, FONT_SIZE_5X8, 0x3);
    if (sXCIndex == DCP_XC_VSET) {
        const uint8_t xt[] = {60, 54, 42, 36};
        LCD_InvertRect8(xt[sVCIndex - cVCIdxMin] - 1, 4, 7, 1);
    }
    sprintf(buf, "Iset: %04d.%01dmA", sIset / 1000, sIset % 1000 / 100);
    LCD_DrawString8(buf, 0, 5, FONT_SIZE_5X8, 0x3);
    if (sXCIndex == DCP_XC_ISET) {
        const uint8_t xt[] = {66, 54, 48, 42, 36};
        LCD_InvertRect8(xt[sICIndex - cICIdxMin] - 1, 5, 7, 1);
    }
    sprintf(buf, "Temp: %d.%d'C", sTemperature / 10, sTemperature % 10);
    LCD_DrawString8(buf, 0, 6, FONT_SIZE_5X8, 0x3);
    LCD_Refresh();
}

/*-----------------------------------*/

int main(void)
{
    uint32_t ms_key_last, ms_disp_last, ms_now;

    DisableJTAG();

    TimeBase_Init();
    LED_Init();
    LCD_Init();
    DCP_Init();

    shell_initkeys_io();
    SKey_InitArrayID(keys, 5, shell_getkeystate, 1);

    sVset = 1000;    // 1V
    sIset = 10000;   // 10mA
    DCP_SetVout(sVset);
    DCP_SetIout(sIset);

    LCD_Clear();
    //LCD_DrawPicture8_1Bpp(graphic1, 16, 8 / 8, 32, 32 / 8, 0x0, 0x3);
    //LCD_DrawString8("Hello World!", 0, 0, FONT_SIZE_5X8, 0x3);
    LCD_Refresh();
    LCD_LightOnOff(TRUE);

    ms_disp_last = millis();
    ms_key_last = millis();

    while (1) {
        ms_now = millis();
        if (ms_now > ms_key_last + 20) {
            ms_key_last = ms_now;
            SKey_UpdateArray(keys, sizeof(keys) / sizeof(skey_t));
            DealKeys();
        }

        ms_now = millis();
        if (ms_now > ms_disp_last + 200) {
            ms_disp_last = ms_now;
            sVin = DCP_GetVin();
            sVout = DCP_GetVout();
            sIout = DCP_GetIout();
            sTemperature = DCP_GetTemp();
            Display();
        }
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
