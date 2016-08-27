#ifndef __CMATH_H__
#define __CMATH_H__

#include "base.h"

typedef struct complex_number {
    float Re;
    float Im;
} cplx;

void cplxDiv(cplx *res, cplx *div);

void cplxMul(cplx *res, cplx *mul);

int32_t cordic_y(int32_t theta);

float absolute(float x);

float square(float x);

#endif //__CMATH_H__
