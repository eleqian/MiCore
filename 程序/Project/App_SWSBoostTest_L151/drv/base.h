#ifndef __BASE_H__
#define __BASE_H__

#include "platform_config.h"

#include <stddef.h>

/*typedef signed char     int8_t;
typedef unsigned char   uint8_t;
typedef signed short    int16_t;
typedef unsigned short  uint16_t;
typedef signed long     int32_t;
typedef unsigned long   uint32_t;*/

#ifndef INLINE
#define INLINE
#endif

#ifndef TRUE
typedef unsigned char   BOOL;
#define TRUE        1
#define FALSE       0
#endif

#ifndef NULL
#define NULL        ((void*)0)
#endif

#ifndef MIN
#define MIN(a,b)    ((a)<=(b)?(a):(b))
#endif

#ifndef MAX
#define MAX(a,b)    ((a)>=(b)?(a):(b))
#endif

#define BITS_SET(v,b)   ((v) |= (b))
#define BITS_CLR(v,b)   ((v) &= ~(b))
#define BITS_CHK(v,b)   (((v) & (b)) == (b))

// 位带操作
#define MEM_ADDR(addr)              \
    *((volatile unsigned long *)(addr))
#define BITBAND(addr, bitnum)       \
    (((addr) & 0xF0000000) + 0x2000000 + (((addr) & 0xFFFFF) << 5) + ((bitnum) << 2))
#define BIT_ADDR(addr, bitnum)      \
    MEM_ADDR(BITBAND(addr, bitnum))
// SRAM简化版，提高效率
#define BITBAND_SRAM(addr, bitnum)  \
    (0x22000000 + (((addr) - 0x20000000) << 5) + ((bitnum) << 2))
#define BIT_ADDR_SRAM(addr, bitnum)      \
    MEM_ADDR(BITBAND_SRAM(addr, bitnum))

#endif  // __BASE_H__
