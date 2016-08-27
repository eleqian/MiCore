#ifndef __INPUT_H__
#define __INPUT_H__

#include "base.h"

void input_init(const uint8_t keymap[16]);
void input_read(void);
BOOL input_is_pressed(uint8_t key);
uint8_t input_getkey_pressed(void);

#endif //__INPUT_H__
