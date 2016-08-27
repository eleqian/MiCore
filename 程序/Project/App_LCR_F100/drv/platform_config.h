#ifndef __PLATFORM_CONFIG_H__
#define __PLATFORM_CONFIG_H__

#ifdef STM32L1XX_MD
#include "stm32l1xx.h"
#define MCU_STM32L151C8
#endif //STM32L1XX_MD

#ifdef STM32F10X_MD
#include "stm32f10x.h"
#define MCU_STM32F103C8
#endif //STM32F10X_MD

#ifdef STM32F10X_MD_VL
#include "stm32f10x.h"
#define MCU_STM32F100C8
#endif //STM32F10X_MD_VL

#endif // __PLATFORM_CONFIG_H__
