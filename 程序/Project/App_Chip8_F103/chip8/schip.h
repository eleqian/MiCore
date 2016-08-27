#ifndef __SCHIP_H__
#define __SCHIP_H__

#include "base.h"

// schip RAM size = 4kB
#define SCHIP_RAM_SIZE      0x1000
// schip VRAM size = 1kB(128*64bit)
#define SCHIP_VRAM_SIZE     0x400

/* settings data */
typedef struct {
    BOOL cheat;         /* 1: disable collision */
    BOOL hwrap;         /* sprite horizontal wrap */
    BOOL vwrap;         /* sprite vertical wrap */
    BOOL overclock;     /* 1: overclock *10 */
    uint8_t speed;      /* speed mul-1 */
    uint8_t flags[8];   /* hp48 flags (vm+set related, won't be cleared at reset) */
} schip_set_t;

/* virtual machine data */
typedef struct {
    uint8_t v[16];          /* 16 v registers */
    uint16_t stack[16];     /* 16 level stack */
    uint16_t pc_init;       /* initial programcounter */
    uint16_t pc;            /* programcounter (12 bit) */
    uint16_t i;             /* index register (12 bit) */
    uint8_t sp;             /* stack pointer (nybble) */
    uint8_t st;             /* sound timer */
    uint8_t dt;             /* delay timer */
    uint8_t stopped;        /* bit 0: interpreter has quit, bit 1: tight infinite loop */
    uint8_t drawmode;       /* 1: schip, 2: chip-8 */
} schip_vm_t;

// schip program loader
// return: byte read, 0 for failed
typedef uint16_t (*schip_loader_t)(uint8_t *buf, uint16_t maxsize);

// generates a random number on [0,0xff]
typedef uint8_t (*schip_rand_t)(void);

// play or stop sound
typedef void (*schip_sound_t)(BOOL isplay);

// check if the key is pressed
typedef BOOL (*schip_checkkey_t)(uint8_t key);

// get the pressed key
typedef uint8_t (*schip_getkey_t)(void);

/* schip callback functions */
typedef struct {
    schip_loader_t loader;
    schip_rand_t rand;
    schip_sound_t sound;
    schip_checkkey_t checkkey;
    schip_getkey_t getkey;
} schip_cb_t;

/* schip emu data */
typedef struct {
    uint8_t vram[SCHIP_VRAM_SIZE];
    uint8_t ram[SCHIP_RAM_SIZE];
    schip_vm_t vm;
    schip_set_t set;
    schip_cb_t cb;
} schip_t;

void schip_init(schip_t *schip);
void schip_reset(schip_t *schip);
void schip_frame(schip_t *schip);

int schip_stopped(schip_t *schip);

#endif //__SCHIP_H__
