/*
CHIP-8/SCHIP interpreter based on Fish 'N' Chips 2.1.5
eleqian 2014-9-28
*/

#include "base.h"
#include "schip.h"
#include <string.h>

/*-----------------------------------*/

/* 'internal' schip program as fish n chips logo */
static const uint8_t fishie[160] = {
    0x00, 0xe0, 0xa2, 0x20, 0x62, 0x08, 0x60, 0xf8, 0x70, 0x08, 0x61, 0x10, 0x40, 0x20, 0x12, 0x0e,
    0xd1, 0x08, 0xf2, 0x1e, 0x71, 0x08, 0x41, 0x30, 0x12, 0x08, 0x12, 0x10, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x3c, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x3e, 0x3f, 0x3f, 0x3b, 0x39, 0x38, 0x38, 0x38, 0x00, 0x00, 0x80, 0xc1, 0xe7, 0xff, 0x7e, 0x3c,
    0x00, 0x1f, 0xff, 0xf9, 0xc0, 0x80, 0x03, 0x03, 0x00, 0x80, 0xe0, 0xf0, 0x78, 0x38, 0x1c, 0x1c,
    0x38, 0x38, 0x39, 0x3b, 0x3f, 0x3f, 0x3e, 0x3c, 0x78, 0xfc, 0xfe, 0xcf, 0x87, 0x03, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x80, 0xe3, 0xff, 0x7f, 0x1c, 0x38, 0x38, 0x70, 0xf0, 0xe0, 0xc0, 0x00,
    0x3c, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* font, made a screenshot of a dosbox in raster font, size 4*6 and 8*12 and 'redrawn' them: */

/* chip-8 font, 4(msb)*5. 5*16=80bytes (start: 160) */
static const uint8_t font8[80] = {
    /* 0 */ 0x60, 0xa0, 0xa0, 0xa0, 0xc0,
    /* 1 */ 0x40, 0xc0, 0x40, 0x40, 0xe0,
    /* 2 */ 0xc0, 0x20, 0x40, 0x80, 0xe0,
    /* 3 */ 0xc0, 0x20, 0x40, 0x20, 0xc0,
    /* 4 */ 0x20, 0xa0, 0xe0, 0x20, 0x20,
    /* 5 */ 0xe0, 0x80, 0xc0, 0x20, 0xc0,
    /* 6 */ 0x40, 0x80, 0xc0, 0xa0, 0x40,
    /* 7 */ 0xe0, 0x20, 0x60, 0x40, 0x40,
    /* 8 */ 0x40, 0xa0, 0x40, 0xa0, 0x40,
    /* 9 */ 0x40, 0xa0, 0x60, 0x20, 0x40,
    /* A */ 0x40, 0xa0, 0xe0, 0xa0, 0xa0,
    /* B */ 0xc0, 0xa0, 0xc0, 0xa0, 0xc0,
    /* C */ 0x60, 0x80, 0x80, 0x80, 0x60,
    /* D */ 0xc0, 0xa0, 0xa0, 0xa0, 0xc0,
    /* E */ 0xe0, 0x80, 0xc0, 0x80, 0xe0,
    /* F */ 0xe0, 0x80, 0xc0, 0x80, 0x80
};

/* schip font, 8*10. 10*16=160bytes (start: 0) */
static const uint8_t fontS[160] = {
    /* 0 */ 0x7c, 0xc6, 0xce, 0xde, 0xd6, 0xf6, 0xe6, 0xc6, 0x7c, 0x00,
    /* 1 */ 0x10, 0x30, 0xf0, 0x30, 0x30, 0x30, 0x30, 0x30, 0xfc, 0x00,
    /* 2 */ 0x78, 0xcc, 0xcc, 0xc,  0x18, 0x30, 0x60, 0xcc, 0xfc, 0x00,
    /* 3 */ 0x78, 0xcc, 0x0c, 0x0c, 0x38, 0x0c, 0x0c, 0xcc, 0x78, 0x00,
    /* 4 */ 0x0c, 0x1c, 0x3c, 0x6c, 0xcc, 0xfe, 0x0c, 0x0c, 0x1e, 0x00,
    /* 5 */ 0xfc, 0xc0, 0xc0, 0xc0, 0xf8, 0x0c, 0x0c, 0xcc, 0x78, 0x00,
    /* 6 */ 0x38, 0x60, 0xc0, 0xc0, 0xf8, 0xcc, 0xcc, 0xcc, 0x78, 0x00,
    /* 7 */ 0xfe, 0xc6, 0xc6, 0x06, 0x0c, 0x18, 0x30, 0x30, 0x30, 0x00,
    /* 8 */ 0x78, 0xcc, 0xcc, 0xec, 0x78, 0xdc, 0xcc, 0xcc, 0x78, 0x00,
    /* 9 */ 0x7c, 0xc6, 0xc6, 0xc6, 0x7c, 0x18, 0x18, 0x30, 0x70, 0x00,
    /* A */ 0x30, 0x78, 0xcc, 0xcc, 0xcc, 0xfc, 0xcc, 0xcc, 0xcc, 0x00,
    /* B */ 0xfc, 0x66, 0x66, 0x66, 0x7c, 0x66, 0x66, 0x66, 0xfc, 0x00,
    /* C */ 0x3c, 0x66, 0xc6, 0xc0, 0xc0, 0xc0, 0xc6, 0x66, 0x3c, 0x00,
    /* D */ 0xf8, 0x6c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x6c, 0xf8, 0x00,
    /* E */ 0xfe, 0x62, 0x60, 0x64, 0x7c, 0x64, 0x60, 0x62, 0xfe, 0x00,
    /* F */ 0xfe, 0x66, 0x62, 0x64, 0x7c, 0x64, 0x60, 0x60, 0xf0, 0x00
};

/*-----------------------------------*/

#define X       (opcode >> 8 & 0xf)
#define Y       (opcode >> 4 & 0xf)
#define N       (opcode & 0xf)
#define KK      (opcode & 0xff)
#define MMM     (opcode & 0xfff)
#define V       schip->vm.v
#define VX      V[X]
#define VY      V[Y]
#define STACK   schip->vm.stack
#define SP      schip->vm.sp
#define PC      schip->vm.pc
#define I       schip->vm.i
#define ST      schip->vm.st
#define DT      schip->vm.dt
#define RAM     schip->ram
#define VRAM    schip->vram
#define INC_PC()    PC = ((PC + 2) & 0xfff)

/*-----------------------------------*/

int schip_stopped(schip_t *schip)
{
    return schip->vm.stopped;
}

void schip_reset(schip_t *schip)
{
    memset(VRAM, 0, SCHIP_VRAM_SIZE); /* cls */
    schip->vm.stopped = 0;
    PC = schip->vm.pc_init; /* jump to start */
}

void schip_init(schip_t *schip)
{
    memset(&schip->vm, 0, sizeof(schip->vm));
    schip->vm.drawmode = 2;     // chip-8
    schip->vm.pc_init = 0x200;

    memset(RAM, 0, SCHIP_RAM_SIZE);
    /* load font */
    memcpy(RAM, fontS, sizeof(fontS));
    memcpy(RAM + 160, font8, sizeof(font8));
    /* load program */
    if (schip->cb.loader == NULL || schip->cb.loader(RAM + schip->vm.pc_init, SCHIP_RAM_SIZE - schip->vm.pc_init) == 0) {
        /* load fishie program */
        memcpy(RAM + 0x200, fishie, sizeof(fishie));
    }

    schip_reset(schip);
}

static void draw_sprite(schip_t *schip, uint16_t opcode)
{
    uint16_t index, offset;
    uint32_t spriteline;
    uint8_t nlines, linecount;
    uint8_t sb, result;
    uint8_t x, y;

    x = (VX * schip->vm.drawmode) & 127;
    y = (VY * schip->vm.drawmode) & 63;

    index = (y << 4) | (x >> 3); /* (y*16)+(x/8): 16 bytes per row, 8 bits per offset */

    nlines = N;
    if (nlines == 0) {
        nlines = 16;
    }

    V[0xf] = 0;

    /* draw lines 1 by 1 */
    for (linecount = 0; linecount < nlines; linecount++) {
        offset = index + ((schip->vm.drawmode << 4) * linecount);
        if (schip->set.vwrap) {
            /* go to top if overflow */
            offset &= 0x3ff;
        } else {
            /* not draw if overflow */
            if (offset & 0x400) {
                break;
            }
        }

        if (schip->vm.drawmode == 2) {
            /* chip-8 */
            size_t i;
            uint16_t spriteline_temp = 0;

            spriteline = RAM[(I + linecount) & 0xfff];
            for (i = 8; i > 0; i--) {
                spriteline_temp |= ((spriteline & (1 << (i - 1))) << i);    /* broaden the line to make compatible with high res */
            }
            spriteline = spriteline_temp | (spriteline_temp >> 1);
        } else {
            /* schip */
            if (nlines < 16) {
                spriteline = RAM[(I + linecount) & 0xfff] << 8;    /* 8*x */
            } else {
                spriteline = ((RAM[(I + (linecount << 1)) & 0xfff]) << 8) | RAM[(I + 1 + (linecount << 1)) & 0xfff];    /* 16*16 */
            }
        }

        spriteline <<= (8 - (x & 7)); /* correct x position */

        /* unrolled a small loop here for speed */
        /* column 1/3 */
        sb = (spriteline >> 16) & 0xff;
        result = VRAM[offset] ^ sb;
        if (!V[0xf]) {
            V[0xf] = result != (VRAM[offset] | sb);    /* check collision */
        }
        VRAM[offset] = result;
        if (schip->vm.drawmode == 2) {
            VRAM[offset + 16] = result;    /* +another line for chip8 */
        }

        /* column 2/3 */
        offset++;
        sb = (spriteline >> 8) & 0xff;
        if (!(offset & 0xf)) {
            if (!schip->set.hwrap) {
                continue;    /* there will be no row drawing if there's no h-wrap and it wrapped */
            }
            offset -= 16; /* set hor wrap correction, -16=1 line back up */
        }
        result = VRAM[offset] ^ sb;
        if (!V[0xf]) {
            V[0xf] = result != (VRAM[offset] | sb);
        }
        VRAM[offset] = result;
        if (schip->vm.drawmode == 2) {
            VRAM[offset + 16] = result;
        }

        /* column 3/3 */
        offset++;
        sb = spriteline & 0xff;
        if (!sb) {
            continue;    /* no drawing if it's empty, since this is the last byte */
        }
        if (!(offset & 0xf)) {
            if (!schip->set.hwrap) {
                continue;
            }
            offset -= 16;
        }
        result = VRAM[offset] ^ sb;
        if (!V[0xf]) {
            V[0xf] = result != (VRAM[offset] | sb);
        }
        VRAM[offset] = result;
        if (schip->vm.drawmode == 2) {
            VRAM[offset + 16] = result;
        }
    }

    if (schip->set.cheat) {
        V[0xf] = 0;    /* no collision */
    }
}

static void schip_execute(schip_t *schip, uint32_t cycles)
{
    uint16_t opcode;

    while (cycles--) {
        opcode = RAM[PC] << 8 | RAM[(PC + 1) & 0xfff];
        INC_PC();

        switch (opcode & 0xf000) {
        case 0x0000:
            switch (opcode) {
                /* no $c0 */
            case 0x00c1:
            case 0x00c2:
            case 0x00c3:
            case 0x00c4:
            case 0x00c5:
            case 0x00c6:
            case 0x00c7:
            case 0x00c8:
            case 0x00c9:
            case 0x00ca:
            case 0x00cb:
            case 0x00cc:
            case 0x00cd:
            case 0x00ce:
            case 0x00cf:        /* scd n (schip) */
                memmove(VRAM + (N << 4), VRAM, 0x400 - (N << 4));
                memset(VRAM, 0, N << 4);
                break;
            case 0x00e0:        /* cls */
                memset(VRAM, 0, 0x400);
                break;
            case 0x00ee:        /* ret */
                SP--;
                SP &= 0xf;
                PC = STACK[SP];
                break;
            case 0x00fb: {      /* scr */
                size_t x, y;
                for (y = 0; y < 0x400; y += 16) {
                    for (x = 15; x > 0; x--) {
                        VRAM[x + y] = (VRAM[x + y] >> 4) | (VRAM[x - 1 + y] << 4);
                    }
                    VRAM[y] >>= 4;
                }
                break;
            }
            case 0x00fc: {      /* scl */
                size_t x, y;
                for (y = 0; y < 0x400; y += 16) {
                    for (x = 0; x < 15; x++) {
                        VRAM[x + y] = (VRAM[x + y] << 4) | (VRAM[x + 1 + y] >> 4);
                    }
                    VRAM[15 + y] <<= 4;
                }
                break;
            }
            case 0x00fd:        /* exit (schip) */
                schip->vm.stopped |= 1;
                cycles = ST = 0;
                break;
            case 0x00fe:        /* low (schip) */
                schip->vm.drawmode = 2;
                break;
            case 0x00ff:        /* high (schip) */
                schip->vm.drawmode = 1;
                break;
            default:            /* sys mmm (1802) */
                break;
            }
            break;
        case 0x1000:            /* jp mmm */
            if (((PC + 0xffe) & 0xfff) == MMM) {
                schip->vm.stopped |= 2;
            }
            PC = MMM;
            break;
        case 0x2000:            /* call mmm */
            STACK[SP] = PC;
            SP++;
            SP &= 0xf;
            PC = MMM;
            break;
        case 0x3000:            /* sne vx=kk */
            if (VX == KK) {
                INC_PC();
            }
            break;
        case 0x4000:            /* sne vx!=kk */
            if (VX != KK) {
                INC_PC();
            }
            break;
        case 0x5000:            /* sne vx=vy */
            if (VX == VY) {
                INC_PC();
            }
            break;
        case 0x6000:            /* ld vx,kk */
            VX = KK;
            break;
        case 0x7000:            /* add vx,kk */
            VX += KK;
            break;
        case 0x8000:
            switch (opcode & 0xf00f) {
            case 0x8000:        /* ld vx,vy */
                VX = VY;
                break;
            case 0x8001:        /* or vx,vy */
                VX |= VY;
                break;
            case 0x8002:        /* and vx,vy */
                VX &= VY;
                break;
            case 0x8003:        /* xor vx,vy */
                VX ^= VY;
                break;
            case 0x8004: {      /* add vx,vy */
                uint8_t c = (VX + VY) >> 8;
                VX += VY;
                V[0xf] = c;
                break;
            }
            case 0x8005: {      /* sub vx,vy */
                uint8_t c = VX >= VY;
                VX -= VY;
                V[0xf] = c;
                break;
            }
            case 0x8006: {      /* shr vx(,vy) */
                uint8_t c = VX & 1;
                VX >>= 1;
                V[0xf] = c;
                break;
            }
            case 0x8007: {      /* subn vx,vy */
                uint8_t c = VY >= VX;
                VX = VY - VX;
                V[0xf] = c;
                break;
            }
            case 0x800e: {      /* shl vx(,vy) */
                uint8_t c = VX >> 7;
                VX <<= 1;
                V[0xf] = c;
                break;
            }
            default:            /* illegal */
                break;
            }
            break;
        case 0x9000:            /* sne vx!=vy */
            if (VX != VY) {
                INC_PC();
            }
            break;
        case 0xa000:            /* ld i,mmm */
            I = MMM;
            break;
        case 0xb000:            /* jp v0,mmm */
            PC = (MMM + V[0]) & 0xfff;
            break;
        case 0xc000:            /* rnd vx,kk */
            VX = schip->cb.rand() & KK;
            break;
        case 0xd000:            /* drw vx,vy,n */
            draw_sprite(schip, opcode);
            break;
        case 0xe000:
            switch (opcode & 0xf0ff) {
            case 0xe09e:        /* skp vx */
                if (schip->cb.checkkey(VX)) {
                    INC_PC();
                }
                break;
            case 0xe0a1:        /* sknp vx */
                if (!schip->cb.checkkey(VX)) {
                    INC_PC();
                }
                break;
            default:            /* illegal */
                break;
            }
            break;
        case 0xf000:            /* ld vx,dt */
            switch (opcode & 0xf0ff) {
            case 0xf007:
                VX = DT;
                break;
            case 0xf00a: {      /* ld vx,k */
                uint8_t k = schip->cb.getkey();
                if (k == 16) {
                    PC = (PC + 0xffe) & 0xfff;
                } else {
                    VX = k;
                }
                break;
            }
            case 0xf015:        /* ld dt,vx */
                DT = VX;
                break;
            case 0xf018:        /* ld st,vx */
                ST = VX;
                break;
            case 0xf01e:        /* add i,vx */
                I = (I + VX) & 0xfff;
                break;
            case 0xf029:        /* ld f,vx */
                I = 160 + (VX & 0xf) * 5;
                break;
            case 0xf030:        /* ld hf,vx (schip) */
                I = (VX & 0xf) * 10;
                break;
            case 0xf033:        /* ld b,vx */
                RAM[(I + 2) & 0xfff] = VX % 10;
                RAM[(I + 1) & 0xfff] = (VX % 100) / 10;
                RAM[I] = VX / 100;
                break;
            case 0xf055: {      /* ld [i],vx */
                size_t x = X + 1;
                while (x--) {
                    RAM[(I + x) & 0xfff] = V[x];
                }
                break;
            }
            case 0xf065: {      /* ld vx,[i] */
                size_t x = X + 1;
                while (x--) {
                    V[x] = RAM[(I + x) & 0xfff];
                }
                break;
            }
            case 0xf075:        /* ld r,vx (schip) */
                memcpy(schip->set.flags, V, (X & 7) + 1);
                break;
            case 0xf085:        /* ld vx,r (schip) */
                memcpy(V, schip->set.flags, (X & 7) + 1);
                break;
            default:            /* illegal */
                break;
            }
            break;
        }
    } /* big loop */
}

void schip_frame(schip_t *schip)
{
    if (!schip->vm.stopped) {
        uint32_t cycles;

        cycles = (uint32_t)(((((schip->set.speed + 1) * 100) / 60.0) * (schip->set.overclock ? 10 : 1)) + 1);
        schip_execute(schip, cycles);
    }

    if (ST) {
        schip->cb.sound(TRUE);
    } else {
        schip->cb.sound(FALSE);
    }
    DT -= DT > 0;
    ST -= ST > 0;
}
