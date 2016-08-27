#include "base.h"
#include <stdio.h>
#include "timebase.h"
#include "led.h"
#include "lcd.h"
#include "skey.h"

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

// 电压设定值
static uint32_t sVin, sVout;

// 设置输入电压(mV)
void SetVin(uint32_t mv)
{
    uint16_t val;
    
    // dac = 4096 * (v / scale) / vdd
    //     = 10 * 4096 * v_mv / scale_x10 / vdd_mv
    val = (uint32_t)(4096 * 1.002) * mv / 3335;
    DAC_SetChannel1Data(DAC_Align_12b_R, val);
}

// 设置输出电压(mV)
void SetVout(uint32_t mv)
{
    uint16_t val;
    
    // dac = 4096 * (v / scale) / vdd
    //     = 10 * 4096 * v_mv / scale_x10 / vdd_mv
    mv = mv / 2 + 5;
    val = (uint32_t)(4096 * 0.997) * mv / 3335;
    DAC_SetChannel2Data(DAC_Align_12b_R, val);
}

void DealKeys(void)
{
    if (SKey_CheckFlagAny(shell_id2key(KEY_ID_S1), SKEY_FLAG_PRESS | SKEY_FLAG_REPEAT)) {
        sVin += 100;
        if (sVin > 4000) {
            sVin = 4000;
        }
        SetVin(sVin);
    }
    
    if (SKey_CheckFlagAny(shell_id2key(KEY_ID_S2), SKEY_FLAG_PRESS | SKEY_FLAG_REPEAT)) {
        sVin -= 100;
        if ((int32_t)sVin < 0) {
            sVin = 0;
        }
        SetVin(sVin);
    }
    
    if (SKey_CheckFlagAny(shell_id2key(KEY_ID_S3), SKEY_FLAG_PRESS | SKEY_FLAG_REPEAT)) {
        sVout += 100;
        if (sVout > 4000) {
            sVout = 4000;
        }
        SetVout(sVout);
    }
    
    if (SKey_CheckFlagAny(shell_id2key(KEY_ID_S4), SKEY_FLAG_PRESS | SKEY_FLAG_REPEAT)) {
        sVout -= 100;
        if ((int32_t)sVout < 0) {
            sVout = 0;
        }
        SetVout(sVout);
    }
    
    if (SKey_CheckFlagAny(shell_id2key(KEY_ID_S5), SKEY_FLAG_PRESS)) {
        
    }
}

void Display(void)
{
    char buf[32] = "";
    
    LCD_Clear();
    sprintf(buf, "Vin: %04dmV", sVin);
    LCD_DrawString8(buf, 0, 1, FONT_SIZE_5X8, 0x3);
    sprintf(buf, "Vout: %04dmV", sVout);
    LCD_DrawString8(buf, 0, 2, FONT_SIZE_5X8, 0x3);
    LCD_Refresh();
}

static void DAC_Config(void)
{
    DAC_InitTypeDef DAC_InitStructure;

    // DAC Periph clock enable
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_DAC, ENABLE);

    // DAC channel1 & 2 Configuration
    DAC_StructInit(&DAC_InitStructure);
    DAC_InitStructure.DAC_Trigger = DAC_Trigger_None;
    DAC_InitStructure.DAC_WaveGeneration = DAC_WaveGeneration_None;
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

static void GOIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);

    // DAC输出引脚 PA4(DAC_OUT1), PA5(DAC_OUT2)
    GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_4 | GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

/*-----------------------------------*/

int main(void)
{
    uint32_t ms_key_last, ms_disp_last, ms_now;

    DisableJTAG();

    TimeBase_Init();
    LED_Init();
    LCD_Init();
    DAC_Config();
    GOIO_Config();

    shell_initkeys_io();
    SKey_InitArrayID(keys, 5, shell_getkeystate, 1);

    sVin = 2500;
    sVout = 2000;
    SetVin(sVin);
    SetVout(sVout);

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
