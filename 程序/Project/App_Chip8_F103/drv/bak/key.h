#ifndef __KEY_H__
#define __KEY_H__

#include "base.h"

// 开启宏时支持外部按键扫描扩展
//#define SUPPORT_KEY_EX

// 定义专用变量类型
typedef uint8_t key_code_t;
typedef uint8_t key_state_t;
typedef uint8_t key_event_t;
typedef uint16_t key_delay_t;

// 按键状态标志
#define KEY_STATE_NONE      0x0
#define KEY_STATE_PRESS     0x1
#define KEY_STATE_ENSURE    0x2
#define KEY_STATE_HOLD      0x4
#define KEY_STATE_REPEAT    0x8
#define KEY_STATE_BLOCK     0x10

// 按键延时
#define KEY_DELAY_ENSURE    20
#define KEY_DELAY_HOLD      800
#define KEY_DELAY_REPEAT    150
#define KEY_DELAY_SCAN      10

// 按键事件
enum {
    KEY_EVENT_NONE = 0,
    KEY_EVENT_PRESS,
    KEY_EVENT_RELEASE,
    KEY_EVENT_HOLD,
    KEY_EVENT_REPEAT,

    KEY_EVENT_MAX
};

// 按键值枚举
enum {
    KEY_CODE_NONE = 0,

    KEY_CODE_M,
    KEY_CODE_UP,
    KEY_CODE_DOWM,
    KEY_CODE_LEFT,
    KEY_CODE_RIGHT,

#ifdef SUPPORT_KEY_EX
#endif //SUPPORT_KEY_EX

    KEY_CODE_MAX
};

// 初始化按键模块
void Key_Init(void);

// 执行按键任务
void Key_Exec(void);

// 阻塞此次按键事件
void Key_BlockOnce(void);

// 按键事件通知函数，需外部实现
// 参数：事件，按键值
extern void OnKeyEvent(key_event_t event, key_code_t key);

#ifdef SUPPORT_KEY_EX
// 获取当前按键值，需外部实现
extern key_code_t Key_GetKeyEx(void);
#endif //SUPPORT_KEY_EX

#endif  // __KEY_H__
