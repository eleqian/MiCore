#ifndef __SHELL_H__
#define __SHELL_H__

#include "base.h"
#include "skey.h"

int shell_main(void);

skey_t *shell_id2key(uint8_t id);

#endif // __SHELL_H__
