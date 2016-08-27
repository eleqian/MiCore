/* 示波器采样FIFO模块
 * eleqian 2014-8-22
 */

#include "base.h"
#include <string.h>
#include "sfifo.h"

// FIFO存储空间
static uint16_t gSFIFOBuf[2][SFIFO_SIZE];

// FIFO模式
static uint8_t gSFIFOMode;

// FIFO读写相对地址
static uint16_t gReadAddr;
static uint16_t gWriteAddr;

// 双FIFO模式下读和写FIFO索引
static uint8_t gReadFIFOIdx;
static uint8_t gWriteFIFOIdx;

/*-----------------------------------*/

// 填充16bits数组
// 参数：数组指针， 填充的值，填充个数
static uint16_t *memset16(uint16_t *dest, uint16_t val, size_t n)
{
    uint16_t *p;
    uint16_t *end = dest + n;

    for (p = dest; p < end; p++) {
        *p = val;
    }

    return dest;
}

/*-----------------------------------*/

// 初始化
void SFIFO_Init(void)
{
    gSFIFOMode = SFIFO_MODE_SEPARATE;
    gReadAddr = 0;
    gWriteAddr = 0;
    gReadFIFOIdx = 0;
    gWriteFIFOIdx = 1;
    memset(gSFIFOBuf, 0xff, sizeof(gSFIFOBuf));
}

// 复位
void SFIFO_Reset(void)
{
    gReadAddr = 0;
    gWriteAddr = 0;
    gReadFIFOIdx = 0;
    gWriteFIFOIdx = 1;
    memset(gSFIFOBuf, 0xff, sizeof(gSFIFOBuf));
}

// 写数据
// 参数：数据缓冲区，写入数目
void SFIFO_Write(const uint16_t *buf, uint16_t count)
{
    uint16_t *pWr;
    uint16_t newAddr;
    //#include "timebase.h"
    //volatile uint32_t t1, t2;

    if (count == 0) {
        return;
    }

    if (gSFIFOMode == SFIFO_MODE_NORMAL) {
        pWr = &gSFIFOBuf[0][0];
    } else {
        pWr = &gSFIFOBuf[gWriteFIFOIdx][0];
    }

    //t1 = micros();
    // 复制数据
    if (gWriteAddr + count <= SFIFO_SIZE) {
        memcpy(pWr + gWriteAddr, buf, count << 1);
    } else {
        memcpy(pWr + gWriteAddr, buf, (SFIFO_SIZE - gWriteAddr) << 1);
        memcpy(pWr, (buf + SFIFO_SIZE - gWriteAddr), (gWriteAddr + count - SFIFO_SIZE) << 1);
    }
    /*t2 = micros() - t1;
    if (t2 > 30) {
        t1 = t2;
    }

    if (t2 < 30 && count == 256) {
        t1 = t2;
    }*/

    // 修改写地址
    newAddr = gWriteAddr + count;
    if (newAddr >= SFIFO_SIZE) {
        newAddr -= SFIFO_SIZE;
    }

    // 修改读地址
    if (gSFIFOMode == SFIFO_MODE_NORMAL) {
        if (gWriteAddr == gReadAddr) {
            // FIFO空
            gReadAddr = newAddr + 1;
        } else if (gWriteAddr < gReadAddr) {
            if (gWriteAddr + count >= gReadAddr) {
                gReadAddr = newAddr + 1;
            }
        } else {
            if (newAddr > gReadAddr) {
                gReadAddr = newAddr + 1;
            }
        }

        if (gReadAddr >= SFIFO_SIZE) {
            gReadAddr -= SFIFO_SIZE;
        }
    }

    gWriteAddr = newAddr;
}

// 填充数据
// 参数：要填充为的数据，填充数目
void SFIFO_Fill(const uint16_t val, uint16_t count)
{
    uint16_t *pWr;
    uint16_t newAddr;

    if (count == 0) {
        return;
    }

    if (gSFIFOMode == SFIFO_MODE_NORMAL) {
        pWr = &gSFIFOBuf[0][0];
    } else {
        pWr = &gSFIFOBuf[gWriteFIFOIdx][0];
    }

    // 复制数据
    if (gWriteAddr + count <= SFIFO_SIZE) {
        memset16(pWr + gWriteAddr, val, count << 1);
    } else {
        memset16(pWr + gWriteAddr, val, (SFIFO_SIZE - gWriteAddr) << 1);
        memset16(pWr, val, (gWriteAddr + count - SFIFO_SIZE) << 1);
    }

    // 修改写地址
    newAddr = gWriteAddr + count;
    if (newAddr >= SFIFO_SIZE) {
        newAddr -= SFIFO_SIZE;
    }

    // 修改读地址
    if (gSFIFOMode == SFIFO_MODE_NORMAL) {
        if (gWriteAddr == gReadAddr) {
            // FIFO空
            gReadAddr = newAddr + 1;
        } else if (gWriteAddr < gReadAddr) {
            if (gWriteAddr + count >= gReadAddr) {
                gReadAddr = newAddr + 1;
            }
        } else {
            if (newAddr > gReadAddr) {
                gReadAddr = newAddr + 1;
            }
        }

        if (gReadAddr >= SFIFO_SIZE) {
            gReadAddr -= SFIFO_SIZE;
        }
    }

    gWriteAddr = newAddr;
}

// 直接读数据，不改变FIFO指针
// 参数：偏移（相对于当前读指针），数据缓冲区，读取数目
void SFIFO_ReadDirect(uint16_t offset, uint16_t *buf, uint16_t count)
{
    uint16_t *pRd;
    uint16_t rdAddr;

    if (gSFIFOMode == SFIFO_MODE_NORMAL) {
        pRd = &gSFIFOBuf[0][0];
    } else {
        pRd = &gSFIFOBuf[gReadFIFOIdx][0];
    }

    rdAddr = (gReadAddr + offset) % SFIFO_SIZE;

    if (rdAddr + count <= SFIFO_SIZE) {
        memcpy(buf, (pRd + rdAddr), count << 1);
    } else {
        memcpy(buf, (pRd + rdAddr), (SFIFO_SIZE - rdAddr) << 1);
        memcpy((buf + SFIFO_SIZE - rdAddr), pRd, (rdAddr + count - SFIFO_SIZE) << 1);
    }
}

// 设置FIFO模式
// 参数：模式（单FIFO或双FIFO）
void SFIFO_SetMode(uint8_t mode)
{
    gSFIFOMode = mode;
    SFIFO_Reset();
}

// 交换“读FIFO”和“写FIFO”
void SFIFO_Switch_RW(void)
{
    uint8_t tmpIdx;

    if (gSFIFOMode == SFIFO_MODE_NORMAL) {
        return;
    }

    // 交换读写FIFO索引
    tmpIdx = gReadFIFOIdx;
    gReadFIFOIdx = gWriteFIFOIdx;
    gWriteFIFOIdx = tmpIdx;

    gReadAddr = gWriteAddr;
    gWriteAddr = 0;
}

// 从“读FIFO”末尾复制数据到“写FIFO”开头
// 参数：复制的数据个数
void SFIFO_Copy_R2W(uint16_t count)
{
    uint16_t *pRd, *pWr;

    if (gSFIFOMode == SFIFO_MODE_NORMAL) {
        return;
    }

    pRd = &gSFIFOBuf[gReadFIFOIdx][0];
    pWr = &gSFIFOBuf[gWriteFIFOIdx][0];

    if (gReadAddr >= count) {
        memcpy((pWr + SFIFO_SIZE - count), (pRd + gReadAddr - count), count << 1);
    } else {
        memcpy((pWr + SFIFO_SIZE - gReadAddr), pRd, gReadAddr << 1);
        memcpy((pWr + SFIFO_SIZE - count), (pRd + SFIFO_SIZE + gReadAddr - count), (count - gReadAddr) << 1);
    }
}
