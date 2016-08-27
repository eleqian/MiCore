#ifndef __LCR_H__
#define __LCR_H__

#include "base.h"
#include "cmath.h"

void LCR_Init(void);

void LCR_SetFreq(int freq_k);

void LCR_SetCalibrate(cplx *cZo, cplx *cZs);

void LCR_StartMeasure(size_t rounds);

BOOL LCR_GetMeasure(cplx *Z);

#endif //__LCR_H__
