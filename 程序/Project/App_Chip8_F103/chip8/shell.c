
#include "base.h"
#include "sound.h"
#include "input.h"
#include "file.h"
#include "schip.h"
#include "lcd.h"
#include "skey.h"
#include "timebase.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static struct {
    BOOL colour_invert;         /* invert colours 1: yes, 0: no */
    uint8_t keymode;            /* keypad mode: 0: on location, 1: on character */
    BOOL sleep;                 /* 1: sleep while waiting */
    BOOL sound;                 /* (temp, only used for getting settings) 1: sound enabled */

    int rect_width_plus;
    int rect_height_plus;
} shell;

// emulator data
static schip_t schip;

static skey_t keys[5 + 6];

// 按键id枚举
enum {
    KEY_ID_NONE = 0,

    // 游戏扩展板按键id
    KEY_ID_A,
    KEY_ID_B,
    KEY_ID_LEFT,
    KEY_ID_RIGHT,
    KEY_ID_UP,
    KEY_ID_DOWN,

    // 核心板按键id
    KEY_ID_S1,
    KEY_ID_S2,
    KEY_ID_S3,
    KEY_ID_S4,
    KEY_ID_S5,

    KEY_ID_MAX
};

/*
CHIP:
    1 2 3 C
    4 5 6 D
    7 8 9 E
    A 0 B f
*/

// blink游戏按键映射
static const uint8_t keymap_blink[16] = {
    KEY_ID_B, 0, KEY_ID_UP, 0,
    0, 0, KEY_ID_DOWN, 0,
    KEY_ID_LEFT, KEY_ID_RIGHT, 0, 0,
    0, 0, 0, 0
};

/*-----------------------------------*/

static void update_title(void)
{
    char c[64] = {0};
    char s[16] = {0};

    if (schip.set.overclock) {
        sprintf(s, "%2dkHz", (1 + schip.set.speed));
    } else {
        sprintf(s, "%1d.%1dkHz", (1 + schip.set.speed) / 10, (1 + schip.set.speed) % 10);
    }

    sprintf(c, "%s%s%s",
            file_is_open() ? file_get_name() : "",
            file_is_open() ? " - " : " ",
            schip_stopped(&schip) ? "Stopped" : s);

    LCD_DrawString(c, 0, 64, FONT_SIZE_5X8, 0x3);
}

void shell_scr_refresh(void)
{
    uintptr_t addrbase, addr;
    uint8_t pagebuf[128];
    uint8_t point8;
    size_t i, j, pagecnt;

    LCD_Clear();
    // 由于用的LCD扫描方向和chip8中的不一样，这里进行处理，每次8行像素
    for (pagecnt = 0; pagecnt < 8; pagecnt++) {
        // 使用位带提高位操作效率
        addrbase = BITBAND_SRAM((uintptr_t)&schip.vram[pagecnt * 128], 0);
        for (i = 0; i < 128; i++) {
            addr = addrbase + ((i & 0x78) + 7 - (i & 0x7)) * 4;
            point8 = 0;
            for (j = 0; j < 8; j++) {
                point8 |= (MEM_ADDR(addr) << (j));
                addr += 128 * 4;
            }
            pagebuf[i] = point8;
        }
        LCD_DrawPicture8_1Bpp(pagebuf, 0, pagecnt, 128, 1, 0x3);
    }
    update_title();
    LCD_Refresh();
}

static uint8_t shell_rand8(void)
{
    return (uint8_t)(rand() >> 8);
}

static void shell_sound(BOOL isplay)
{
    if (isplay) {
        sound_play();
    } else {
        sound_stop();
    }
}

void shell_initkeys(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC, ENABLE);

    // PA2~PA7: KEY1~KEY6
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // PB2: S5
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // PC13~15: S1~S4
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
}

BOOL shell_getkeystate(void *pdata)
{
    uint8_t id = (uint8_t)((uintptr_t)pdata & 0xff);

    switch (id) {
    case KEY_ID_A:
        return !GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_3);
    case KEY_ID_B:
        return !GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_2);
    case KEY_ID_LEFT:
        return !GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_7);
    case KEY_ID_RIGHT:
        return !GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_5);
    case KEY_ID_UP:
        return !GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_4);
    case KEY_ID_DOWN:
        return !GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_6);
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

int shell_main(void)
{
    uint32_t count, curcount, countfreq; /* timer */

    memset(&shell, 0, sizeof(shell));

    /* standard settings */
    shell.keymode = 0;
    shell.sound = 1;
    shell.sleep = 1;

    schip.set.cheat = 0;
    schip.set.vwrap = 0;
    schip.set.hwrap = 0;
    schip.set.overclock = 0;
    schip.set.speed = 4;
    memset(schip.set.flags, 0, 8);

    //get_settings();

    srand(micros());
    shell_initkeys();
    SKey_InitArrayID(keys, 5 + 6, shell_getkeystate, 1);
    input_init(keymap_blink);
    sound_init();
    file_init();

    sound_set_enabled(shell.sound);

    //file_open("schip/JOUST23");
    file_open("schip/BLINKY");

    schip.cb.loader = file_read;
    schip.cb.rand = shell_rand8;
    schip.cb.sound = shell_sound;
    schip.cb.checkkey = input_is_pressed;
    schip.cb.getkey = input_getkey_pressed;
    schip_init(&schip);

    /* 60 fps (a real VIP runs at 1.7609MHz, which means ~60.009 fps, but let's keep it simple) */
    countfreq = 1000000 / 60;

    count = micros();
    count += countfreq;

    for (;;) {
        /* do frame */
        curcount = micros();
        if ((curcount >= (count + countfreq)) || ((curcount < count) && (curcount + 0x7fffffffL) >= (count + countfreq - 0x7fffffffL))) {
            count = curcount;
            SKey_UpdateArray(keys, sizeof(keys) / sizeof(skey_t));
            if (SKey_CheckFlag(shell_id2key(KEY_ID_S1), SKEY_FLAG_PRESS_IN)) {
                schip.set.overclock = TRUE;
            } else {
                schip.set.overclock = FALSE;
            }
            if (SKey_CheckFlag(shell_id2key(KEY_ID_S4), SKEY_FLAG_PRESS)) {
                schip_reset(&schip);
            }
            if (SKey_CheckFlag(shell_id2key(KEY_ID_S5), SKEY_FLAG_PRESS)) {
                sound_toggle_enabled();
            }
            input_read();
            schip_frame(&schip);
            shell_scr_refresh();
        }
    }

    //put_settings();
}

/*-----------------------------------*/

#define USE_SAVE_STATE  0

#if USE_SAVE_STATE
#define STATE_MAX 0x2000
static uint8_t state[STATE_MAX] = {0}; /* savestate data */
#endif // USE_SAVE_STATE

#if USE_SAVE_STATE
/* save state/load state */
void schip_save_state(schip_t *schip)
{
    state[STATE_MAX - 1] = 1; /* set save state flag */

    memcpy(state, schip->ram, 0x1000);             /* schip->ram */
    memcpy(state + 0x1000, schip->vram, 0x400);        /* schip->vram */
    memcpy(state + 0x1400, schip->set.flags, 8);     /* flags */
    memcpy(state + 0x1408, &schip->vm, sizeof(schip->vm)); /* vm data */
}

void schip_load_state(schip_t *schip)
{
    if (state[STATE_MAX - 1] == 0) {
        return;
    }

    memcpy(schip->ram, state, 0x1000);             /* schip->ram */
    memcpy(schip->vram, state + 0x1000, 0x400);        /* schip->vram */
    memcpy(schip->set.flags, state + 0x1400, 8);     /* flags */
    memcpy(&schip->vm, state + 0x1408, sizeof(schip->vm)); /* vm data */
}
#endif // USE_SAVE_STATE

/*-----------------------------------*/

#if 0
/* settings */
static void get_settings(void)
{
    HKEY key;
    DWORD data, type, size = 4;

    if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Fish 'N' Chips emulator\\", 0, KEY_ALL_ACCESS, &key) != ERROR_SUCCESS) {
        RegCloseKey(key);
        return;
    }

    /* get+correct settings from registry */
#define GET_REG(x) RegQueryValueEx(key,x,0,&type,(BYTE*)&data,&size)==ERROR_SUCCESS&&type==REG_DWORD

    if (GET_REG("mode")) {
        shell.mode = data % 3;
    }
    if (GET_REG("keymode")) {
        shell.keymode = data & 1;
    }
    if (GET_REG("sleep")) {
        shell.sleep = data & 1;
    }
    if (GET_REG("sound")) {
        shell.sound = data & 1;
    }
    if (GET_REG("colour_invert")) {
        shell.colour_invert = data & 1;
    }

    if (GET_REG("schip_speed")) {
        schip_set_speed(data % 20);
    }
    if (GET_REG("schip_overclock")) {
        schip.set.overclock = data & 1;
    }
    if (GET_REG("schip_flags0123")) {
        schip.set.flags[0] = data & 0xff;
        schip.set.flags[1[ = data >> 8 & 0xff;
                           schip.set.flags[2] = data >> 16 & 0xff;
                           schip.set.flags[3] = data >> 24 & 0xff;
    }
    if (GET_REG("schip_flags4567")) {
        schip.set.flags[4] = data & 0xff;
        schip.set.flags[5[ = data >> 8 & 0xff;
                           schip.set.flags[6] = data >> 16 & 0xff;
                           schip.set.flags[7] = data >> 24 & 0xff;
    }
    if (GET_REG("schip_cheat")) {
        schip.set.cheat = data & 1;
    }
    if (GET_REG("schip_vwrap")) {
        schip.set.vwrap = data & 1;
    }
    if (GET_REG("schip_hwrap")) {
        schip.set.hwrap = data & 1;
    }

    RegCloseKey(key);
}

static void put_settings(void)
{
    HKEY key;
    DWORD data;

    if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Fish 'N' Chips emulator\\", 0, KEY_ALL_ACCESS, &key) != ERROR_SUCCESS) {
        if (RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\Fish 'N' Chips emulator\\", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL) != ERROR_SUCCESS) {
            RegCloseKey(key);
            return;
        }
    }

    /* put settings into registry */
#define PUT_REG(x,y) data=y; RegSetValueEx(key,x,0,REG_DWORD,(BYTE*)&data,4)

    PUT_REG("mode", shell.mode);
    PUT_REG("keymode", shell.keymode);
    PUT_REG("schip_speed", schip.set.speed);
    PUT_REG("schip_overclock", schip.set.overclock);
    PUT_REG("schip_flags0123", schip.set.flags[0] | (schip.set.flags[1] << 8) | (schip.set.flags[2] << 16) | (schip.set.flags[3] << 24));
    PUT_REG("schip_flags4567", schip.set.flags[4] | (schip.set.flags[5] << 8) | (schip.set.flags[6] << 16) | (schip.set.flags[7] << 24));
    PUT_REG("schip_cheat", schip.set.cheat);
    PUT_REG("schip_vwrap", schip.set.vwrap);
    PUT_REG("schip_hwrap", schip.set.hwrap);
    PUT_REG("sleep", shell.sleep);
    PUT_REG("sound", sound_get_enabled());
    PUT_REG("colour_invert", shell.colour_invert);

    RegCloseKey(key);
}
#endif
