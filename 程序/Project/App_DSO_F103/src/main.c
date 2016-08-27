#include "base.h"
#include "timebase.h"
#include "task.h"
#include "key.h"
#include "lcd.h"
#include "led.h"
#include "spi_flash.h"
#include "dso.h"

/*-----------------------------------*/

static key_event_t gKeyEvent = KEY_EVENT_NONE;
static key_code_t gKeyCode;

// 接收按键事件
// 这里不直接调用应用任务按键处理，可减少函数嵌套深度和堆栈峰值
void OnKeyEvent(key_event_t event, key_code_t key)
{
    gKeyEvent = event;
    gKeyCode = key;
}

/*-----------------------------------*/

// 软件复位
void SoftReset(void)
{
    __set_FAULTMASK(1);     // 关闭所有中断
    NVIC_SystemReset();     // 复位
}

// 电源开
void PowerOn(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    // 电源控制引脚 PB6
    GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_SetBits(GPIOB, GPIO_Pin_6);
}

// 电源关
void PowerOff(void)
{
    LCD_PowerSaveOnOff(TRUE);
    LCD_LightOnOff(FALSE);
    LED_Light(FALSE);
    GPIO_ResetBits(GPIOB, GPIO_Pin_6);
    while (1) {
    }
}

/*-----------------------------------*/

// PWM输出初始化
void PWM_Init_1k(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
    TIM_OCInitTypeDef  TIM_OCInitStructure;
    uint16_t TimerPeriod = 0;
    uint16_t Channel2Pulse = 0, Channel3Pulse = 0;

    /* TIM1, GPIOA and AFIO clocks enable */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1 | RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);

    /* GPIOA Configuration: Channel 2 and 3 as alternate function push-pull */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* TIM1 Configuration ---------------------------------------------------
     Generate 2 PWM signals with 2 different duty cycles:
     TIM1CLK = SystemCoreClock, Prescaler = 0, TIM1 counter clock = SystemCoreClock
     SystemCoreClock is set to 72 MHz for Low-density, Medium-density, High-density
     and Connectivity line devices and to 24 MHz for Low-Density Value line and
     Medium-Density Value line devices

     The objective is to generate 2 PWM signal at 1 KHz:
       - TIM1_Period = (SystemCoreClock / 1000) - 1
     The channel 2 duty cycle is set to 50%
     The channel 3 duty cycle is set to 25%
     The Timer pulse is calculated as follows:
       - ChannelxPulse = DutyCycle * (TIM1_Period - 1) / 100
    ----------------------------------------------------------------------- */
    /* Compute the value to be set in ARR regiter to generate signal frequency at 1 Khz */
    TimerPeriod = (SystemCoreClock / 1000) - 1;
    /* Compute CCR2 value to generate a duty cycle at 50%  for channel 2 */
    Channel2Pulse = (uint16_t)(((uint32_t) 50 * (TimerPeriod - 1)) / 100);
    /* Compute CCR3 value to generate a duty cycle at 25%  for channel 3 */
    Channel3Pulse = (uint16_t)(((uint32_t) 25 * (TimerPeriod - 1)) / 100);

    /* Time Base configuration */
    TIM_TimeBaseStructure.TIM_Prescaler = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_Period = TimerPeriod;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;

    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);

    /* Channel 2, 3 Configuration in PWM mode */
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM2;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Disable;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_Low;
    TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_High;
    TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Set;
    TIM_OCInitStructure.TIM_OCNIdleState = TIM_OCIdleState_Reset;

    TIM_OCInitStructure.TIM_Pulse = Channel2Pulse;
    TIM_OC2Init(TIM1, &TIM_OCInitStructure);

    TIM_OCInitStructure.TIM_Pulse = Channel3Pulse;
    TIM_OC3Init(TIM1, &TIM_OCInitStructure);

    /* TIM1 counter enable */
    TIM_Cmd(TIM1, ENABLE);

    /* TIM1 Main Output Enable */
    TIM_CtrlPWMOutputs(TIM1, ENABLE);
}

/*-----------------------------------*/

static BOOL gIsPowerOn = FALSE;

// 应用按键处理
void App_KeyEvent(key_event_t event, key_code_t key)
{
    if (event == KEY_EVENT_RELEASE && key == KEY_CODE_M) {
        if (!gIsPowerOn) {
            gIsPowerOn = TRUE;
        } else {
            LED_Light(TRUE);
            delay_ms(100);
            PowerOff();
        }
        return;
    }

    DSO_KeyEvent(event, key);
}

/*-----------------------------------*/

void LCD_Test(void)
{
    uint32_t t1;
    size_t i;

    LCD_Clear();
    t1 = micros();
    for (i = 0; i < 10; i++) {
        LCD_DrawVLine(2, 0, 79, 0x1);
        LCD_DrawVLine(1, 1, 14, 0x3);
        LCD_DrawHLine(0, 127, 77, 0x2);
        LCD_DrawHLine(1, 14, 78, 0x3);
        LCD_DrawString("ABCDEFGHIJKLMNOPQRSTUVWXYZ", 10, 36, FONT_SIZE_3X6, 0x3);
        //LCD_DrawString("abcdefghijklmnopqrstuvwxyz", 10, 44, FONT_SIZE_3X6, 0x3);
    }
    t1 = micros() - t1;
    LCD_Refresh();

    LCD_DisplayAscii5x8(t1 / 1000 + '0', 108, 0);
    LCD_DisplayAscii5x8('.', 114, 0);
    LCD_DisplayAscii5x8(t1 % 1000 / 100 + '0', 120, 0);
    LCD_DisplayAscii5x8('m', 114, 1);
    LCD_DisplayAscii5x8('s', 120, 1);
}

/*-----------------------------------*/

#define TEST_BASE   FLASH_BASE      // 定义基址

// 通过进行读操作确定实际Flash大小
int GetMemSize(void)
{
    int i;
    uint32_t addr = 0;
    uint32_t *p;
    uint32_t val = 0x12345678;

    p = (uint32_t *)TEST_BASE;          // 取基址

    for (i = 1; i <= 1024; i++) {       // 1024*2K
        p += 512;                       // 512字(2K字节)为单位
        if (val != *p) {                // 读出来数据
            val = *p;                   // MemFault时，相当于没读数据
        } else {                        // 如果数据相同
            addr = *(u32 *)0xe000ed34;  // Fault后，保存的数据地址
            if (addr == (u32)p) {       // 数据地址相同与本次访问的地址相同
                break;                  // 跳出
            }
        }
    }

    return (addr - TEST_BASE);             // 首次MemManage_Fault的地址，就是SRAM内存的最大地址
}

/*-----------------------------------*/

int main(void)
{
    PowerOn();

    //GetMemSize();

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    TimeBase_Init();
    Task_Init();
    Key_Init();
    LED_Init();
    LCD_Init();
    DSO_Init();
    
    PWM_Init_1k();

    LED_Flash(300, 200, 3);
    LCD_LightOnOff(TRUE);
    //LCD_Test();

    while (1) {
        // 检查就绪任务
        Task_CheckReady();

        switch (gTaskCur) {
        case TASK_KEY:
            // 按键驱动任务
            Key_Exec();
            break;
        case TASK_LED:
            // LED任务
            LED_Exec();
            break;
        case TASK_DSO:
            // DSO任务
            DSO_Exec();
            break;
        default:
            // 传递按键事件
            if (gKeyEvent != KEY_EVENT_NONE) {
                //LCD_LightOnOff(TRUE);
                App_KeyEvent(gKeyEvent, gKeyCode);
                gKeyEvent = KEY_EVENT_NONE;
            }
            break;
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
