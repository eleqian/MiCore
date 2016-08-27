#ifndef __SOUND_H__
#define __SOUND_H__

#include "base.h"

void sound_init(void);
void sound_stop(void);
void sound_play(void);

void sound_set_enabled(int);
int sound_get_enabled(void);
void sound_toggle_enabled(void);

#endif /* __SOUND_H__ */
