#ifndef __DSO_H__
#define __DSO_H__

#include "base.h"
#include "key.h"

void DSO_Init(void);

void DSO_Exec(void);

void DSO_KeyEvent(key_event_t event, key_code_t key);

#endif // __DSO_H__
