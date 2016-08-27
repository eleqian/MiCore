#ifndef __VIP_H__
#define __VIP_H__

void vip_frame(void);
void vip_init(void);
void vip_reset(void);
void vip_save_state(void);
void vip_load_state(void);

void vip_chip8_hardreset(void);
void vip_chip8x_hardreset(void);

int vip_stopped(void);

#endif /* __VIP_H__ */
