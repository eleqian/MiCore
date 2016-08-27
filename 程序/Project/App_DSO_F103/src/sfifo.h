#ifndef __SFIFO_H__
#define __SFIFO_H__

#include "base.h"

// 采样FIFO大小（采样深度）
#define SFIFO_SIZE              1000

// FIFO模式枚举
enum {
    SFIFO_MODE_NORMAL,      // 普通模式，同普通FIFO基本一致
    SFIFO_MODE_SEPARATE,    // 双FIFO模式，将FIFO空间分为两部分，一个读一个写
    SFIFO_MODE_MAX
};

void SFIFO_Init(void);

void SFIFO_Reset(void);

void SFIFO_SetMode(uint8_t mode);

void SFIFO_Write(const uint16_t *buf, uint16_t count);

void SFIFO_Fill(const uint16_t val, uint16_t count);

void SFIFO_ReadDirect(uint16_t offset, uint16_t *buf, uint16_t count);

void SFIFO_Switch_RW(void);

void SFIFO_Copy_R2W(uint16_t count);

#endif // __SFIFO_H__
