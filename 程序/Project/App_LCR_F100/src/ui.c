#include "base.h"
#include "string.h"
#include "math.h"
#include "cmath.h"
#include "lcr.h"
#include "lcd.h"
#include "ui.h"

// 校准次数
#define CAL_ROUNDS      64

// 测量频率点数
#define FCNT            5

// 各频率点对应频率，单位kHz
const int flist[FCNT] = { 1, 9, 25, 49, 97 };

const int dig[] = { 1000000000, 100000000, 10000000, 1000000, 100000, 10000, 1000, 100, 10, 1};

const char dp[9] = {'f', 'p', 'n', 'u', 'm', ' ', 'k', 'M', 'G'};

// Open/Short校准数据
cplx Zo[FCNT];
cplx Zs[FCNT];

// 校准状态
int cstatus;

// 频率序号
int findex;

// 测量频率
int freq;

/*-----------------------------------*/

void print(char *str)
{
    lcd_putstr(str);
    //  uart_tx(str,1);// wait
}

// 显示hex值
void puthex(int inp, short size)
{
    char buf[9];
    int j;

    for (j = size - 1; j >= 0; j--) {
        char dig = (inp & 0xf);
        if (dig < 10) {
            buf[j] = 0x30 + dig;
        } else {
            buf[j] = 0x41 + dig - 10;
        }
        inp >>= 4;
    }

    buf[size] = 0;
    print(buf);
}

// 转换十进制值到字符串
void sputdec(char *buf, int inp)
{
    int ptr = 0;
    int s, startflag = 1;
    int digit;

    for (ptr = 0; ptr < 12; ptr++) {
        buf[ptr] = 0;
    }
    ptr = 0;

    if (inp < 0) {
        inp = -inp;
        buf[ptr++] = '-';
    }

    for (s = 0; s < 10; s++) {
        digit = 0;
        while (inp >= dig[s]) {
            inp -= dig[s];
            digit++;
        }

        if (digit != 0) {
            startflag = 0;
        }
        if (s == 9) {
            startflag = 0;
        }
        if (startflag == 0) {
            buf[ptr++] = 0x30 + digit;
        }
    }
}

// 显示十进制值
void putdec(int inp)
{
    char buf[12];

    sputdec(buf, inp);
    print(buf);
}

// 显示浮点值
void  printFloat(int x , int y, float num, char *suffix)
{
    char out[20];
    char nnn[20];
    int i, dot;

    out[0] = ' ';
    if (num < 0) {
        num = -num;
        out[0] = '-';
    }
    if (num > 1e10) {
        num = 1e10;
    }
    if (num < 1e-15) {
        num = 1e-15;
    }

    int exp = 19;

    if (num < 10000.0) {
        while (num < 10000.0) {
            num *= 10.0;
            exp--;
        }
    } else {
        while (num > 99999.4) {
            num /= 10.0;
            exp++;
        }
    }

    dot = (exp + 30) % 3;
    exp = (exp) / 3;

    if (exp < 0 || exp > 8) {
        for (i = 0; i < 6; i++) {
            out[i + 1] = '-';
        }
    } else {
        char *optr = &out[1];

        sputdec(nnn, num + 0.5);

        for (i = 0; i < 6; i++) {
            *optr++ = nnn[i];
            if (i == dot) {
                *optr++  = '.';
            }
        }
    }

    out[7] = dp[exp];

    for (i = 0; i < 6; i++) {
        if (suffix[i] == 0) {
            out[8 + i] = 0;
            break;
        }
        out[8 + i] = suffix[i];
    }

    lcd_putnum(x, y, out);
}

/*------------------------------------*/

// 等待Z值到某个范围
void waitz(float lim)
{
    float min = -lim , max = lim;
    float z;
    cplx Z;

    if (lim < 0) { // minimum
        z = min * 2;
        while (z > min) {
            measure(&Z, 1);
            z = absolute(Z.Re);
        }
    } else {
        z = 0;
        while (z < max * max) {
            measure(&Z, 1);
            z = (Z.Re * Z.Re + Z.Im * Z.Im);
        }
    }
}

void calibrate(void)
{
    cplx Z;

    lcd_gotoxy(0, 0);
    print("Open leads      ");
    waitz(1000);

    measure(&Z, 6);
    lcd_gotoxy(0, 0);
    print("Open cal        ");
    measure(&Zo[findex], CAL_ROUNDS);

    lcd_gotoxy(0, 0);
    print("Close leads      ");
    waitz(-1);

    measure(&Z, 6);
    lcd_gotoxy(0, 0);
    print("Short cal        ");

    measure(&Zs[findex], CAL_ROUNDS);
}

/*-----------------------------------*/

int main(void)
{
    int bl_status = 1, rsrp = 0, rel = 0, btn, round = 0;
    cplx Z, base;
    char s[16];

    if ((cstatus & 0xFFFFFF00) != 0x80000000) {
        cstatus = 0x80000000;
        findex = 0;
    }

    if (findex < 0 || findex >= FCNT) {
        findex = 0;
    }
    freq = flist[findex];
    LCR_SetFreq(freq);

    measure(&Z, 16);

    lcd_clear();

    while (1) {
        btn = buttons();

        if (btn & PWR_BTN) {
            bl_status = !bl_status;
        }
        if (btn & SP_BTN) {
            rsrp = ! rsrp;
        }
        if (btn & REL_BTN) {
            rel = !rel;
        }

        if (btn & SP_BTNL) {
            findex++;
            if (findex >= FCNT) {
                findex = 0;
            }
            freq = flist[findex];
            LCR_SetFreq(freq);
            if ((cstatus & (1 << findex))) {

            }
           rsrp = ! rsrp;
        }

        if (btn & REL_BTNL) {
            calibrate();
            rel = 0;
            cstatus |= (1 << findex);
        }
        if (btn & PWR_BTNL) {
            lcd_clear();
            lcd_gotoxy(0, 4);
            print(" Bye Bye..");
            powerOff();
        }

        if (bl_status) {
            GPIOA->BSRR = GPIO_Pin_15; // BACKLIGHT ON
        } else {
            GPIOA->BRR = GPIO_Pin_15; // BACKLIGHT OFF
        }

        lcd_gotoxy(0, 0);
        lcd_putstr("                ");

        sputdec(s, freq);
        lcd_gotoxy(0, 0);
        lcd_putstr(s);
        lcd_putstr("k  ");

        if (rsrp) {
            lcd_gotoxy(13 * 6, 0);
            lcd_putstr("PAR");
        } else {
            lcd_gotoxy(13 * 6, 0);
            lcd_putstr("SER");
        }

        if (rel) {
            lcd_gotoxy(9 * 6, 0);
            lcd_putstr(">.<");
        } else {
            lcd_gotoxy(9 * 6, 0);
            lcd_putstr("   ");
        }

        if ((cstatus & (1 << findex))) {
            lcd_gotoxy(6 * 5, 0);
            lcd_putstr("CAL");
        } else {
            lcd_gotoxy(6 * 5, 0);
            lcd_putstr("---");
        }

        measure(&Z, 1);

        if (rel) {
            Z.Re -= base.Re;
            Z.Im -= base.Im;
        } else {
            base.Re = Z.Re ;
            base.Im = Z.Im;
        }

        float ls = Z.Im / 6.283185307 / (freq * 1000.0);
        float cs = -1 / 6.283185307 / (freq * 1000.0) / Z.Im;
        float d = Z.Re / Z.Im;
        float d2 = d * d;
        float q = 1 / d;

        if (rsrp == 1) {
            ls = ls * (1 + d2);
            cs = cs / (1 + d2);
            Z.Re = Z.Re * (1.0 + d2) / d2;
        }

        round++;
        if (round > 1) {
            round = 0;

            printFloat(12, 2, Z.Re, "Ohm");
            lcd_gotoxy(0, 2);
            lcd_putstr("  ");
            lcd_gotoxy(0, 3);
            lcd_putstr("R>");

            if (Z.Im > 0) {
                printFloat(12, 4, ls, "H  ");
                lcd_gotoxy(0, 4);
                lcd_putstr("   ");
                lcd_gotoxy(0, 5);
                lcd_putstr("L>");
            } else {
                printFloat(12, 4, cs, "F  ");
                lcd_gotoxy(0, 4);
                lcd_putstr("   ");
                lcd_gotoxy(0, 5);
                lcd_putstr("C>");
            }

            int qq = 10000 * (1.0 + absolute(d));
            if (qq > 19999) {
                qq = 19999;
            }
            sputdec(s, qq);
            lcd_gotoxy(0, 7);
            lcd_putstr("D .");
            lcd_putstr(&s[1]);
            lcd_putstr("    ");

            qq = absolute(q);
            if (qq > 9999) {
                qq = 9999;
            }
            sputdec(s, qq);
            lcd_gotoxy(8 * 6, 7);
            lcd_putstr("Q ");
            lcd_putstr(s);
            lcd_putstr("    ");

            lcd_gotoxy(0, 1);
            lcd_putstr("________________");
            lcd_gotoxy(0, 6);
            lcd_putstr("----------------");
        }
    }
}
