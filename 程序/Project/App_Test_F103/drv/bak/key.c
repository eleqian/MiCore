/*
 * 按键驱动模块 for stm32f103
 * 支持长按键和自动重复，仅支持同时单按键
 * eleqian 2013-2-16
 */

#include "base.h"
#include "task.h"
#include "key.h"

static key_state_t gKeyState;
static key_code_t gKeyCode;
static key_code_t gKeyCodeNew;
static key_delay_t gKeyDelay;

/*-----------------------------------*/

static void Key_PinConfig(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC, ENABLE);

    // PB7~8: KEY1~KEY2
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // PC13~15: KEY3~KEY5
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
}

static key_code_t Key_GetKey(void)
{
    if (!GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_7)) {
        return KEY_CODE_M;
    }
    if (!GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_8)) {
        return KEY_CODE_RIGHT;
    }
    if (!GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_13)) {
        return KEY_CODE_DOWM;
    }
    if (!GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_14)) {
        return KEY_CODE_LEFT;
    }
    if (!GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_15)) {
        return KEY_CODE_UP;
    }

    return KEY_CODE_NONE;
}

/*-----------------------------------*/

void Key_Init(void)
{
    Key_PinConfig();
    gKeyCode = KEY_CODE_NONE;
    gKeyCodeNew = KEY_CODE_NONE;
    gKeyState = KEY_STATE_NONE;
    gKeyDelay = 0;
}

void Key_Exec(void)
{
    key_code_t key;

    key = Key_GetKey();
#ifdef SUPPORT_KEY_EX
    if (key == KEY_CODE_NONE) {
        key = Key_GetKeyEx();
    }
#endif //SUPPORT_KEY_EX

    if (gKeyState & KEY_STATE_ENSURE) {        // 按键消抖状态
        if (gKeyDelay >= KEY_DELAY_SCAN) {
            gKeyDelay -= KEY_DELAY_SCAN;
            goto next;
        }

        gKeyState &= ~KEY_STATE_ENSURE;

        // 确认按下按键
        if (key == gKeyCodeNew) {
            gKeyCode = gKeyCodeNew;
            gKeyState |= KEY_STATE_PRESS;

            // 发送事件
            if (!(gKeyState & KEY_STATE_BLOCK)) {
                OnKeyEvent(KEY_EVENT_PRESS, gKeyCode);
            }

            // 进入长按键前延时
            gKeyDelay = KEY_DELAY_HOLD;
        }
    } else if (gKeyState & KEY_STATE_PRESS) {   // 按键按下状态
        // 释放按键
        if (key == KEY_CODE_NONE) {
            // 发送事件
            if (!(gKeyState & KEY_STATE_BLOCK)) {
                OnKeyEvent(KEY_EVENT_RELEASE, gKeyCode);
            }
            gKeyCode = KEY_CODE_NONE;
            gKeyState = KEY_STATE_NONE;
            gKeyDelay = 0;
            goto next;
        }

        if (gKeyDelay >= KEY_DELAY_SCAN) {
            gKeyDelay -= KEY_DELAY_SCAN;
            goto next;
        }

        // 未进入长按键时进入
        if (!(gKeyState & KEY_STATE_HOLD)) {
            gKeyState |= KEY_STATE_HOLD;
            // 发送事件
            if (!(gKeyState & KEY_STATE_BLOCK)) {
                OnKeyEvent(KEY_EVENT_HOLD, gKeyCode);
            }
            // 进行重复触发按键前延时
            gKeyDelay = KEY_DELAY_REPEAT;
            goto next;
        }

        // 重复触发按键
        gKeyState |= KEY_STATE_REPEAT;
        // 发送事件
        if (!(gKeyState & KEY_STATE_BLOCK)) {
            OnKeyEvent(KEY_EVENT_REPEAT, gKeyCode);
        }
        gKeyDelay = KEY_DELAY_REPEAT;
    } else {            // 无按键按下状态
        // 按下按键
        if (key != KEY_CODE_NONE) {
            gKeyCodeNew = key;
            gKeyState |= KEY_STATE_ENSURE;
            gKeyDelay = KEY_DELAY_ENSURE;
        }
    }

next:
    Task_Delay(KEY_DELAY_SCAN);
}

void Key_BlockOnce(void)
{
    if (gKeyState != KEY_STATE_NONE) {
        gKeyState |= KEY_STATE_BLOCK;
    }
}
