/* ------------------------------------------------------------------
 * 2013 RLC Meter V6 / Neekeetos@yahoo.com
 */

#include "base.h"
#include "string.h"
#include "math.h"
#include "cmath.h"

#define AIRCR_VECTKEY_MASK      ((uint32_t)0x05FA0000)

#define N               500
#define DAC_N           (N/5)
#define OSR             40
#define SINE_OFFSET     900

#define IRQ_ADC_SAMPLE  1
#define IRQ_DAC_INVERSE 2
#define IRQ_DAC_ZERO    4

#define MUL             1073741824.000000

#define ALPHA           0.05
#define ALPHA2          0.1
#define CLOSE_LIMIT     0.05
#define FLT_LEN         5
//#define MOHM_LIMIT    0.02
//#define GOHM_LIMIT    1e6

#define SHUNT           148.0
#define CAL_ROUNDS      64

#define PWR_PIN         (GPIOC->IDR & GPIO_Pin_13)
#define SP_PIN          (GPIOB->IDR & GPIO_Pin_8)
#define REL_PIN         (GPIOB->IDR & GPIO_Pin_7)

#define PWR_BTN         1
#define SP_BTN          2
#define REL_BTN         4

#define PWR_BTNL        0x10
#define SP_BTNL         0x20
#define REL_BTNL        0x40

#define PUSH_MSK        0x000003
#define PUSH_MAP        0x000001
#define LPUSH_MSK       0x00001FFF
#define LPUSH_MAP       0x00000FFF

const uint32_t chn[3] = {1, 2, 7};

const int dig[] = { 1000000000, 100000000, 10000000, 1000000, 100000, 10000, 1000, 100, 10, 1};

const char dp[9] = {'f', 'p', 'n', 'u', 'm', ' ', 'k', 'M', 'G'};

volatile uint32_t irq_request = 0;
volatile uint32_t irq_status = 0;

int16_t sine[N + N / 4];
uint32_t dac_buf[DAC_N];

#define FCNT            5

const int flist[FCNT] = { 1, 9, 25, 49, 97 };
int __attribute__((section(".noinit"))) findex, cstatus;

volatile uint32_t ch = 0;
int freq;

volatile uint16_t adc_dma[N];


//----------------------------------------------------------------------------
cplx mData[3];
cplx mAcc[3];

cplx __attribute__((section(".noinit"))) Zo[FCNT];
cplx __attribute__((section(".noinit"))) Zs[FCNT];  // calibration constants
cplx R;

int buttons();

//----------------------------------------------------------------------------
void fillSine(int freq)
{
    int s, i;
    long long pp;

    for (i = 0; i < N; i++) {
        pp = (((long long)freq * i << 32) / N);    //<<16;
        s = cordic((int)pp);
        s = ((s >> 14) + 1) >> 1;
        sine[i] = s;
    }

    for (i = 0; i < N / 4; i++) {
        pp = (((long long)freq * i << 32) / N);
        s = cordic((int)pp);
        s = ((s >> 14) + 1) >> 1;
        sine[i + N] = s;
    }
}

//----------------------------------------------------------------------------
void  __attribute__((noinline)) print(char *str)
{
    lcd_putstr(str);
    //  uart_tx(str,1);// wait
}

//----------------------------------------------------------------------------
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

//----------------------------------------------------------------------------
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

//----------------------------------------------------------------------------
void putdec(int inp)
{
    char buf[12];
    sputdec(buf, inp);
    print(buf);
}

//----------------------------------------------------------------------------
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
        sputdec(nnn, num + 0.5);

        char *optr = &out[1];
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
            out[i + 8] = 0;
            break;
        }
        out[i + 8] = suffix[i];
    }

    lcd_putnum(x, y, out);
}

//----------------------------------------------------------------------------
void runRound(void) // result in real[3],imag[3]
{
    // GPIOB->BSRR = GPIO_Pin_14;
    irq_request |= IRQ_ADC_SAMPLE;
    while (!(irq_status & IRQ_ADC_SAMPLE)) {
        __NOP();
    }
    __NOP();
    irq_status &= ~IRQ_ADC_SAMPLE;
    // GPIOB->BRR = GPIO_Pin_14; // reset
}

//----------------------------------------------------------------------------
void measure(cplx *Z , int rounds)
{
    cplx I;

    mAcc[0].Re = 0;
    mAcc[0].Im = 0;
    mAcc[1].Re = 0;
    mAcc[1].Im = 0;
    mAcc[2].Re = 0;
    mAcc[2].Im = 0;

    while (rounds-- > 0) {
        runRound();
        mAcc[0].Re += mData[0].Re;
        mAcc[0].Im += mData[0].Im;
        mAcc[1].Re += mData[1].Re;
        mAcc[1].Im += mData[1].Im;
        mAcc[2].Re += mData[2].Re;
        mAcc[2].Im += mData[2].Im;
    }

    Z->Re = (mAcc[2].Re - mAcc[0].Re); // V
    Z->Im = (mAcc[2].Im - mAcc[0].Im);
    cplxMul(Z, &R);
    I.Re = (mAcc[0].Re - mAcc[1].Re); // I
    I.Im = (mAcc[0].Im - mAcc[1].Im);
    cplxDiv(Z, &I);
}

//----------------------------------------------------------------------------
float filter(int sidx, float new)
{
    static float state[10];
    static int cnt[10];

    float t = (state[sidx] - new);
    float sc = CLOSE_LIMIT * state[sidx];

    if ((t > sc) || (t < -sc)) {
        if (t > 0) {
            cnt[sidx]++;
        } else {
            cnt[sidx]--;
        }
        state[sidx] = state[sidx] * (1 - ALPHA2) + new *ALPHA2;
    } else {
        state[sidx] = state[sidx] * (1 - ALPHA) + new *ALPHA;
        if (t < 0) {
            cnt[sidx]++;
        } else {
            cnt[sidx]--;
        }
    }

    if ((cnt[sidx] > 2 * FLT_LEN) || (cnt[sidx] < -2 * FLT_LEN)) {
        state[sidx] = new;
        cnt[sidx] = 0;
        //if(cnt[sidx]  > 0 ) cnt[sidx] = FLT_LEN; else cnt[sidx] =  -FLT_LEN;
    }

    return state[sidx];
}

//----------------------------------------------------------------------------
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
            if (buttons() & PWR_BTN) {
                powerOff();
            }
        }
    } else {
        z = 0;
        while (z < max * max) {
            measure(&Z, 1);
            z = (Z.Re * Z.Re + Z.Im * Z.Im);
            if (buttons() & PWR_BTN) {
                powerOff();
            }
        }
    }
}

//----------------------------------------------------------------------------
void calibrate(void)
{
    cplx Z;
    irq_request &= ~IRQ_DAC_INVERSE; // normal out

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

//----------------------------------------------------------------------------
void init(void)
{
    RCC->CFGR &= ~RCC_CFGR_ADCPRE;
    RCC->CFGR |= RCC_CFGR_ADCPRE_DIV2;// 16M adc

    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    
    RCC->APB1ENR = RCC_APB1ENR_TIM2EN | RCC_APB1ENR_TIM3EN | RCC_APB1ENR_DACEN | RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
    RCC->APB2ENR = RCC_APB2ENR_ADC1EN   | RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN  //|RCC_APB2ENR_USART1EN
                   | RCC_APB2ENR_AFIOEN;

    AFIO->MAPR = AFIO_MAPR_TIM2_REMAP_FULLREMAP | AFIO_MAPR_SWJ_CFG_JTAGDISABLE;
    SCB->AIRCR = AIRCR_VECTKEY_MASK | NVIC_PriorityGroup_1;

    NVIC_SetPriority(DMA1_Channel1_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 1, 1)); //adc
    NVIC_SetPriority(DMA1_Channel4_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 0, 0)); //dac

    TIM2->PSC = 0;
    TIM2->ARR = 63;///63;
    TIM2->CR1 = TIM_CR1_ARPE;
    TIM2->CR2 = TIM_CR2_MMS_2 | TIM_CR2_MMS_1; // TRGO trigger = oc3  = DAC TRIGGER
    TIM2->CCR1 = 46;
    TIM2->CCR2 = 62;
    TIM2->CCR3 = 10;
    TIM2->CCR4 = 0;
    TIM2->SMCR = 0;
    TIM2->CCMR1 = TIM_CCMR1_OC1M | TIM_CCMR1_OC1PE | TIM_CCMR1_OC2M | TIM_CCMR1_OC2PE;
    TIM2->CCMR2 = TIM_CCMR2_OC3M | TIM_CCMR2_OC3PE;//|TIM_CCMR2_OC4M | TIM_CCMR2_OC4PE;
    TIM2->CCER =  TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E ;
    TIM2->DIER =  TIM_DIER_CC1DE;
    TIM2->EGR = 0;

    DAC->CR = DAC_CR_TEN1 | DAC_CR_TEN2 | DAC_CR_TSEL2_2 | DAC_CR_TSEL1_2 //tim2_trgo
              | DAC_CR_BOFF1 | DAC_CR_BOFF2
              | DAC_CR_WAVE1_1 | DAC_CR_MAMP1_1 | DAC_CR_MAMP1_0
              | DAC_CR_WAVE2_1 | DAC_CR_MAMP2_1 | DAC_CR_MAMP2_0
              ;

    DMA1_Channel4->CCR = 0;
    DMA1_Channel4->CPAR = (uint32_t)&DAC->DHR12RD;
    DMA1_Channel4->CMAR = (uint32_t) dac_buf;//sine;
    DMA1_Channel4->CNDTR = DAC_N;
    DMA1_Channel4->CCR = DMA_CCR1_MINC | DMA_CCR1_CIRC | DMA_CCR1_DIR | DMA_CCR1_PL_1 |
                         DMA_MemoryDataSize_Word | DMA_PeripheralDataSize_Word
                         | DMA_CCR1_HTIE  | DMA_CCR1_TCIE  ;
    DMA1_Channel4->CCR |= DMA_CCR1_EN;

    DMA1_Channel1->CCR = 0;
    DMA1_Channel1->CPAR = (uint32_t)&ADC1->DR;
    DMA1_Channel1->CMAR = (uint32_t) adc_dma;
    DMA1_Channel1->CNDTR = N;
    DMA1_Channel1->CCR = DMA_CCR1_MINC | DMA_CCR1_PL_0 | DMA_CCR1_CIRC |
                         DMA_MemoryDataSize_HalfWord | DMA_PeripheralDataSize_HalfWord
                         | DMA_CCR1_TCIE | DMA_CCR1_HTIE
                         ;
    DMA1_Channel1->CCR |= DMA_CCR1_EN;

    // TIM2 CH1
    DMA1_Channel5->CCR = 0;
    DMA1_Channel5->CPAR = (uint32_t)&ADC1->SQR3;
    DMA1_Channel5->CMAR = (uint32_t) &ch;
    DMA1_Channel5->CNDTR = 1;
    DMA1_Channel5->CCR = DMA_CCR1_CIRC | DMA_CCR1_DIR | DMA_CCR1_PL |
                         DMA_MemoryDataSize_Word | DMA_PeripheralDataSize_Word    ;
    DMA1_Channel5->CCR |= DMA_CCR1_EN;

    NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    NVIC_EnableIRQ(DMA1_Channel4_IRQn);

    ADC1->CR1 = ADC_CR1_DISCEN;
    ADC1->CR2 = ADC_CR2_EXTSEL_0 | ADC_CR2_EXTSEL_1 | ADC_CR2_EXTTRIG | ADC_CR2_DMA; // tim2 - cc2
    ADC1->SMPR2 = ADC_SMPR2_SMP7_1 | ADC_SMPR2_SMP2_1 | ADC_SMPR2_SMP1_1 | ADC_SMPR2_SMP0_0;
    ADC1->SQR1 = 0;
    ADC1->SQR3 = 1;

    ADC1->CR2 |= ADC_CR2_ADON;
    ADC1->CR2 |= ADC_CR2_RSTCAL;
    while ((ADC1->CR2 & ADC_CR2_RSTCAL) == ADC_CR2_RSTCAL)  {
        ;
    }
    ADC1->CR2 |= ADC_CR2_CAL;
    while ((ADC1->CR2 & ADC_CR2_CAL) == ADC_CR2_CAL)    {
        ;
    }

    DAC->CR |= DAC_CR_EN1 | DAC_CR_EN2;
    DAC->DHR12R1 = 1000;
    DAC->DHR12R2 = 1000;
    DAC->CR |= DAC_CR_DMAEN2;

    TIM2->CR1 |= TIM_CR1_CEN;
}

//----------------------------------------------------------------------------
int buttons()
{
    int out = 0;
    static unsigned int sPWR = 0;
    static unsigned int sSP = 0;
    static unsigned int sREL = 0;

    sPWR <<= 1;
    sSP <<= 1;
    sREL <<= 1;
    sPWR &= 0xFFFFF;
    sSP &= 0xFFFFF;
    sREL &= 0xFFFFF;
    if (!PWR_PIN) {
        sPWR |= 1;
    }
    if (!SP_PIN)  {
        sSP  |= 1;
    }
    if (!REL_PIN) {
        sREL |= 1;
    }

    if ((sPWR & PUSH_MSK) == PUSH_MAP) {
        out |= PWR_BTN;
    }
    if ((sSP & PUSH_MSK) == PUSH_MAP) {
        out |= SP_BTN;
    }
    if ((sREL & PUSH_MSK) == PUSH_MAP) {
        out |= REL_BTN;
    }

    if ((sPWR & LPUSH_MSK) == LPUSH_MAP) {
        out |= PWR_BTNL;
    }
    if ((sSP & LPUSH_MSK) == LPUSH_MAP) {
        out |= SP_BTNL;
    }
    if ((sREL & LPUSH_MSK) == LPUSH_MAP) {
        out |= REL_BTNL;
    }

    return (out);
}

//----------------------------------------------------------------------------
void powerOff(void)
{
    int cnt = 0;

    AFIO->EXTICR[3] = AFIO_EXTICR4_EXTI13_PC;
    EXTI->FTSR = EXTI_FTSR_TR13;
    EXTI->IMR = 0;
    EXTI->EMR = EXTI_EMR_MR13;
    EXTI->PR = 0x0003FFFF;

    DAC->CR  = 0;
    ADC1->CR2 = 0;
    GPIOA->CRH &= ~(
                      GPIO_CRH_CNF9 | GPIO_CRH_MODE9 // usart tx
                      | GPIO_CRH_CNF10 | GPIO_CRH_MODE10 // usart rx
                  );

    lcd_write(COMMAND, 0xA5); // DAL
    lcd_write(COMMAND, 0xAE); // turn off display

    GPIOA->BSRR = GPIO_Pin_3; // ANALOG OFF
    GPIOA->BRR = GPIO_Pin_15; // BACKLIGHT OFF

    SCB->SCR |= SCB_SCR_SLEEPDEEP;
    PWR->CR &= ~(PWR_CR_PDDS | PWR_CR_LPDS);
    PWR->CR |= PWR_CR_LPDS;

    while (!(GPIOC->IDR & GPIO_Pin_13)) ;

    while (cnt < 400000) {
        if (!(GPIOC->IDR & GPIO_Pin_13)) {
            cnt++;
        } else {
            if (cnt > 0) {
                cnt = 0;
            } else {
                cnt--;
            }
            if (cnt < -100) {
                EXTI->PR = 0x0003FFFF;
                __NOP();
                __WFE();
            }
        }
    }
    NVIC_SystemReset();

    //GPIOA->BSRR = GPIO_Pin_15; // BACKLIGHT ON
}

//----------------------------------------------------------------------------
int main(void)
{
    int bl_status = 1, rsrp = 0, rel = 0, btn, round = 0;
    cplx Z, base;
    char s[16];

    init();

    GPIOA->CRL = GPIO_CRL_MODE3_1; // analog switch
    GPIOA->CRH = GPIO_CRH_CNF10_0 //usart rx
                 | GPIO_CRH_CNF9_1 | GPIO_CRH_MODE9_1 // usart tx
                 | GPIO_CRH_MODE15_1; // backlight
    //|GPIO_CRH_CNF15_1|GPIO_CRH_MODE15_1 // TIM2 CH1
    // MCO : |GPIO_CRH_MODE8_0|GPIO_CRH_CNF8_1  RCC->CFGR |= RCC_CFGR_MCO_SYSCLK;

    GPIOA->BRR = GPIO_Pin_0; // GUARD ON
    GPIOA->BRR = GPIO_Pin_3; // ANALOG ON
    GPIOA->BRR = GPIO_Pin_15; // BACKLIGHT OFF

    GPIOB->CRL =
        GPIO_CRL_CNF7_1  // BUTTON 1 PU
        | GPIO_CRL_MODE3 //SCK
        | GPIO_CRL_MODE4 //MOSI
        | GPIO_CRL_MODE5 // CS
        | GPIO_CRL_MODE6 //RES
        ;
    GPIOB->CRH =
        GPIO_CRH_CNF8_1 // BUTTON 2
        | GPIO_CRH_MODE13_1 // diag 1
        | GPIO_CRH_MODE14_1 // diag 2
        ;

    GPIOB->BSRR = GPIO_Pin_8 | GPIO_Pin_7; // BUTTON 1,2 PU
    GPIOB->BRR = GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6
                 | GPIO_Pin_13 | GPIO_Pin_14; // 0 diag 1,2 outputs

    GPIOC->CRL = 0;
    GPIOC->CRH = GPIO_CRH_CNF13_1;//GPIO_CRH_CNF13_1; // BUTTON 3
    GPIOC->BSRR = GPIO_Pin_13;// BUTTON 3 PU

    lcd_init();
    GPIOA->BSRR = GPIO_Pin_15; // BACKLIGHT ON

    lcd_clear();

    lcd_gotoxy(0, 0);
    lcd_putstr("  RLC ver 6.03  ");
    lcd_gotoxy(0, 2);
    lcd_putstr("   Neekeetos    ");
    lcd_gotoxy(0, 3);
    lcd_putstr("   @yahoo.com   ");
    lcd_gotoxy(0, 4);
    lcd_putstr(" big thanks to  ");
    lcd_gotoxy(0, 5);
    lcd_putstr(" TESLight, Link ");
    lcd_gotoxy(0, 7);
    lcd_putstr("for  radiokot.ru");

    while (buttons()  != 0); // wait BUTTON release

    if ((cstatus & 0xFFFFFF00) != 0x80000000) {
        cstatus = 0x80000000;
        findex = 0;
    }

    if (findex < 0 || findex >= FCNT) {
        findex = 0;
    }
    freq = flist[findex];
    fillSine(freq);

    measure(&Z, 16);

    lcd_clear();

    R.Re = SHUNT;
    R.Im = 0;

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
            fillSine(freq);
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

        // zx = zom * (zsm-zxm)/(zxm-zom)

        if ((cstatus & (1 << findex))) {
            cplx tmp1, tmp2;

            tmp1.Re = Zs[findex].Re - Z.Re;
            tmp1.Im = Zs[findex].Im - Z.Im;
            tmp2.Re = Zo[findex].Re;
            tmp2.Im = Zo[findex].Im;

            cplxMul(&tmp2, &tmp1);
            tmp1.Re = Z.Re - Zo[findex].Re;
            tmp1.Im = Z.Im - Zo[findex].Im;

            cplxDiv(&tmp2, &tmp1);
            Z.Re = tmp2.Re;
            Z.Im = tmp2.Im;
        }

        Z.Re = filter(0, Z.Re);
        Z.Im = filter(1, Z.Im);

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

void  __attribute__((optimize("-O3"))) DMA1_Channel1_IRQHandler(void)
{
    static int process = 0;
    static int j = 0, k = 0;
    static long long mreal[3];
    static long long mimag[3];
    int i;

    //  GPIOB->BSRR = GPIO_Pin_14; // set

    if (process > 0) {
        i = 0 ;
        while (i++ < N / 2) {
            int dat = adc_dma[j] - SINE_OFFSET;
            mreal[k] += ((int)sine[j + N / 4] * dat);
            mimag[k] -= ((int)sine[j] * dat);
            j++;
        }
        if (j >= N) {
            j = 0;
        }
    }

    if (!(DMA1->ISR & DMA_ISR_HTIF1))  {
        process++;
        j = 0;
    }

    DMA1->IFCR = DMA1_IT_GL1;

    if (process > OSR) {
        process = 0;

        k++;
        if (k > 2) {
            k = 0;
        }
        ch = chn[k];
        j = 0;

        if ((k == 0) && (irq_request & IRQ_ADC_SAMPLE)) {
            irq_request &= ~IRQ_ADC_SAMPLE;
            mData[0].Re = mreal[0];
            mData[0].Im = mimag[0];
            mData[1].Re = mreal[1];
            mData[1].Im = mimag[1];
            mData[2].Re = mreal[2];
            mData[2].Im = mimag[2];
            irq_status |= IRQ_ADC_SAMPLE;
        }
        mreal[k] = 0;
        mimag[k] = 0;
    }
    //  GPIOB->BRR = GPIO_Pin_14; // reset
}

void  __attribute__((optimize("-O3")))  DMA1_Channel4_IRQHandler(void)
{
    int j;
    static int max = (N / DAC_N);
    static uint32_t *dptr = dac_buf;
    static int sptr = 0;
    const uint32_t k = (SINE_OFFSET | (SINE_OFFSET << 16));

    //GPIOB->BSRR = GPIO_Pin_13; // set

    if (DMA1->ISR & DMA_ISR_HTIF4) {
        dptr = dac_buf;
        max--;
        if (max <= 0) {
            max = (N / DAC_N);
            sptr = 0;
        }

    }
    DMA1->IFCR = DMA1_IT_GL4;

    j = DAC_N / 2;

    while (j-- > 0)  {
        uint32_t sig = ((sine[sptr]) >> 7) & 0xffff;
        *dptr++ = k + sig - (sig << 16)  ;
        sptr++;
    }
    //GPIOB->BRR = GPIO_Pin_13; // reset
}
