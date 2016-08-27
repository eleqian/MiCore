#ifndef __DCP_H__
#define __DCP_H__

#include "base.h"

void DCP_Init(void);

void DCP_PowerOn(void);

void DCP_PowerOff(void);

void DCP_SetVout(uint32_t mv);

void DCP_SetIout(uint32_t ua);

uint32_t DCP_GetVin(void);

uint32_t DCP_GetVout(void);

uint32_t DCP_GetIout(void);

int16_t DCP_GetTemp(void);

uint32_t DCP_GetVDDA(void);

#endif //__DCP_H__
