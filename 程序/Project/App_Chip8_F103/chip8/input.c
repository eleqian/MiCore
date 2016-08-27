#include "base.h"
#include "skey.h"
#include "shell.h"
#include "input.h"
#include <string.h>

// 按键输入数据
static struct {
    uint8_t keymap[16];         // 0~F按键对应实际按键码
    BOOL is_pressed[16];        // 0~F按键当前状态
    BOOL is_pressed_old[16];    // 0~F按键上个状态
} input;

static const uint8_t keymap_conv[16] = {
    0x1, 0x2, 0x3, 0xc,
    0x4, 0x5, 0x6, 0xd,
    0x7, 0x8, 0x9, 0xe,
    0xa, 0x0, 0xb, 0xf
};

/*-----------------------------------*/

void input_init(const uint8_t keymap[16])
{
    uint8_t i;

    memset(&input, 0, sizeof(input));
    //memcpy(input.keymap, keymap, 16);
    for (i = 0; i < 16; i++) {
        input.keymap[keymap_conv[i]] = keymap[i];
    }
}

void input_read(void)
{
    uint8_t i;

    memcpy(input.is_pressed_old, input.is_pressed, 16 * sizeof(BOOL));

    for (i = 0; i < 16; i++) {
        input.is_pressed[i] = SKey_CheckFlag(shell_id2key(input.keymap[i]), SKEY_FLAG_PRESS_IN);
    }
}

BOOL input_is_pressed(uint8_t key)
{
    return input.is_pressed[key & 0xf];
}

uint8_t input_getkey_pressed(void)
{
    uint8_t i;

    for (i = 0; i < 16; i++) {
        if ((input.is_pressed_old[i] ^ 0x1) & input.is_pressed[i]) {
            break;
        }
    }
    return i;
}
