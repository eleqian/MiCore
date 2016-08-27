/*
 * ETM12880K1驱动(TL0350)
 * eleqian 2014-9-13
 */

#include "base.h"
#include "bitband.h"
#include "math.h"
#include "timebase.h"
#include "lcd.h"
#include "font.h"
#include <string.h>

/*-----------------------------------*/

// 打开宏时使用快速画线算法
#define LCD_FAST_DRAW_VLINE
#define LCD_FAST_DRAW_HLINE

// 打开宏时使用位带操作LCD缓存
#define LCD_BUF_BITBAND

/*-----------------------------------*/

// LCD缓存
static uint8_t gLCDBuf[LCD_PAGES][LCD_WIDTH][LCD_BITS_PER_PIXEL];

/*-----------------------------------*/
// LCD端口操作定义
// 注意需要和初始化函数一起修改

// CS: PB12
#define LCD_CS_0()      GPIOB->BRR = GPIO_Pin_12
#define LCD_CS_1()      GPIOB->BSRR = GPIO_Pin_12

// RST: 系统RST脚
#define LCD_RST_0()     //GPIOA->BRR = GPIO_Pin_4
#define LCD_RST_1()     //GPIOA->BSRR = GPIO_Pin_4

// RS: PB14
#define LCD_RS_0()      GPIOB->BRR = GPIO_Pin_14
#define LCD_RS_1()      GPIOB->BSRR = GPIO_Pin_14

// BL: PA8
#define LCD_BL_OFF()    GPIOA->BRR = GPIO_Pin_8
#define LCD_BL_ON()     GPIOA->BSRR = GPIO_Pin_8

/*-----------------------------------*/

// 初始化端口
static void LCD_HAL_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    SPI_InitTypeDef SPI_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);

    // SPI2配置
    SPI_InitStructure.SPI_Direction = SPI_Direction_1Line_Tx;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SPI2, &SPI_InitStructure);

    // 使能SPI2
    SPI_Cmd(SPI2, ENABLE);

    // 配置BL(PA8)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    LCD_BL_OFF();

    // 配置CS(PB12)和RS(PB14)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_14;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    LCD_CS_1();
    LCD_RS_0();

    // 配置SCK(PB13)和MOSI(PB15)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    //LCD_RST_1();
}

/*-----------------------------------*/

// 写LCD
#define LCD_Write(dat)    \
    do {    \
        while (!(SPI2->SR & SPI_I2S_FLAG_TXE));     \
        SPI2->DR = dat;    \
    } while (0)

#define LCD_WriteRAM(d)     LCD_Write(d)
#define LCD_WriteCmd(c)     LCD_Write(c)

// 开始写数据
static void LCD_WriteBegin_RAM(void)
{
    LCD_RS_1();
    LCD_CS_0();
}

// 开始写命令
static void LCD_WriteBegin_CMD(void)
{
    LCD_RS_0();
    LCD_CS_0();
}

// 结束写LCD
static void LCD_WriteEnd(void)
{
    // 等待SPI传输完成
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY) == SET);
    //while (SPI2->SR & SPI_I2S_FLAG_BSY);
    LCD_CS_1();
}

/*-----------------------------------*/

// 初始化
void LCD_Init(void)
{
    LCD_HAL_Config();

    //LCD_RST_0();            // 低电平复位
    //delay_us(10);
    //LCD_RST_1();            // 复位完毕
    //delay_us(10);

    //delay_us(10);
    LCD_WriteBegin_CMD();
    LCD_WriteCmd(0xe2);     // 软复位
    LCD_WriteEnd();
    delay_us(10);

    LCD_WriteBegin_CMD();

    LCD_WriteCmd(0xc8);     // 行扫描顺序(SHL=0xc0/0xc8)
    LCD_WriteCmd(0xa1);     // 列扫描顺序(ADC=0xa0/0xa1)

    LCD_WriteCmd(0x40);     // 起始RAM行：0
    LCD_WriteCmd(0x00);

    LCD_WriteCmd(0x44);     // 起始COM：COM24
    LCD_WriteCmd(0x18);

    LCD_WriteCmd(0x48);     // duty ratio：80
    LCD_WriteCmd(0x50);

    /*LCD_WriteCmd(0x4c);     // N-line inversion: K = 5
    LCD_WriteCmd(0x0e);*/

    LCD_WriteCmd(0xab);     // 启动振荡器

    LCD_WriteCmd(0x65);     // DC-DC升压倍数：4

    LCD_WriteCmd(0x26);     // 粗调对比度，可设置范围0x20～0x27

    LCD_WriteCmd(0x81);     // 微调对比度
    LCD_WriteCmd(0x2f);     // 可设置范围0x00～0x3f

    LCD_WriteCmd(0x55);     // 偏压比：1/10(bias)

    LCD_WriteCmd(0x90);     // FRC：4，PWM：9

    LCD_WriteCmd(0x88);     // 白占空比：0/36
    LCD_WriteCmd(0x00);
    LCD_WriteCmd(0x89);
    LCD_WriteCmd(0x00);

    LCD_WriteCmd(0x8a);     // 亮灰占空比：16/36
    LCD_WriteCmd(0x44);
    LCD_WriteCmd(0x8b);
    LCD_WriteCmd(0x44);

    LCD_WriteCmd(0x8c);     // 深灰占空比：27/36
    LCD_WriteCmd(0x99);
    LCD_WriteCmd(0x8d);
    LCD_WriteCmd(0x90);

    LCD_WriteCmd(0x8e);     // 黑占空比：36/36
    LCD_WriteCmd(0x99);
    LCD_WriteCmd(0x8f);
    LCD_WriteCmd(0x99);

    LCD_WriteCmd(0x2c);     // 升压步聚1(VC)
    delay_ms(20);
    LCD_WriteCmd(0x2e);     // 升压步聚2(VR)
    delay_ms(5);
    LCD_WriteCmd(0x2f);     // 升压步聚3(VF)
    delay_ms(5);

    LCD_WriteCmd(0xaf);     // 开显示

    //LCD_WriteCmd(0xa5);     // 全黑

    LCD_WriteEnd();
}

/*-----------------------------------*/

// 背光开关
void LCD_LightOnOff(BOOL isOn)
{
    if (isOn) {
        LCD_BL_ON();
    } else {
        LCD_BL_OFF();
    }
}

// 显示开关
void LCD_DisplayOnOff(BOOL isOn)
{
    LCD_WriteBegin_CMD();
    LCD_WriteCmd(0xAE | (isOn ? 1 : 0));
    LCD_WriteEnd();
}

// 低功耗模式开关
void LCD_PowerSaveOnOff(BOOL isOn)
{
    LCD_WriteBegin_CMD();
    LCD_WriteCmd(0xA9 | (isOn ? 1 : 0));
    LCD_WriteEnd();
}

// 设置起始显示行(垂直滚动)
void LCD_SetStartLine(uint8_t line)
{
    LCD_WriteBegin_CMD();
    LCD_WriteCmd(0x40 | (line & 0x3f));
    LCD_WriteEnd();
}

// 设置显示列(x)
void LCD_SetColumn(uint8_t column)
{
    LCD_WriteBegin_CMD();
    LCD_WriteCmd(0x10 | ((column >> 4) & 0x07));
    LCD_WriteCmd(column & 0x0f);
    LCD_WriteEnd();
}

// 设置显示页(y/8)
void LCD_SetPage(uint8_t page)
{
    LCD_WriteBegin_CMD();
    LCD_WriteCmd(0xb0 | (page & 0x0f));
    LCD_WriteEnd();
}

// 同时设置显示列和显示页
void LCD_GotoXY(uint8_t column, uint8_t page)
{
    LCD_WriteBegin_CMD();
    LCD_WriteCmd(0x10 | ((column >> 4) & 0x07));
    LCD_WriteCmd(column & 0x0f);
    LCD_WriteCmd(0xb0 | (page & 0x0f));
    LCD_WriteEnd();
}

/*-----------------------------------*/

// 显示单色图片，高度必须为8的倍数
// 参数：图片指针，x，y/8，w，h/8
void LCD_DisplayPicture8_1Bpp(const void *pic, uint8_t x, uint8_t y8, uint8_t w, uint8_t h8)
{
    const uint8_t *dptr;
    size_t i, j;

    if ((y8 + h8) > LCD_PAGES) {
        return;
    }
    if ((x + w) > LCD_WIDTH) {
        return;
    }

    dptr = (const uint8_t *)pic;

    for (i = 0; i < h8; i++) {
        LCD_GotoXY(x, y8 + i);
        LCD_WriteBegin_RAM();
        for (j = 0; j < w; j++) {
            LCD_WriteRAM(*dptr);
            LCD_WriteRAM(*dptr++);
        }
        LCD_WriteEnd();
    }
}

// 显示4色图片，高度必须为8的倍数
// 参数：图片指针，x，y/8，w，h/8
void LCD_DisplayPicture8_2Bpp(const void *pic, uint8_t x, uint8_t y8, uint8_t w, uint8_t h8)
{
    const uint8_t *dptr;
    size_t i, j;

    if ((y8 + h8) > LCD_PAGES) {
        return;
    }
    if ((x + w) > LCD_WIDTH) {
        return;
    }

    dptr = (const uint8_t *)pic;

    for (i = 0; i < h8; i++) {
        LCD_GotoXY(x, y8 + i);
        LCD_WriteBegin_RAM();
        for (j = 0; j < w; j++) {
            LCD_WriteRAM(*dptr++);
            LCD_WriteRAM(*dptr++);
        }
        LCD_WriteEnd();
    }
}

/*-----------------------------------*/

// 显示5*8英文字符
// 参数：Ascii字符，x，y/8
void LCD_DisplayAscii5x8(char ch, uint8_t x, uint8_t y8)
{
    const uint8_t *dptr;

    if (ch < ' ') {
        ch = ' ';
    } else if (ch > '~') {
        return;
    }

    dptr = &AsciiFont_5x8[(ch - ' ') * 5];

    LCD_DisplayPicture8_1Bpp(dptr, x, y8, 5, 1);
}

// 显示字高8的字符串
// 参数：字符串，x，y/8
void LCD_DisplayString8(const char *str, uint8_t x, uint8_t y8)
{
    while (*str != '\0') {
        LCD_DisplayAscii5x8(*str++, x, y8);
        x += 5;

        if (x >= LCD_WIDTH) {
            x = 0;
            y8++;
            if (y8 >= LCD_PAGES) {
                y8 = 0;
            }
        }
    }
}

/*-----------------------------------*/
// LCD缓存模式

// 刷新帧
void LCD_Refresh(void)
{
    size_t i, j;
    uint8_t *pBuf;

    pBuf = (uint8_t *)gLCDBuf;
    for (i = 0; i < LCD_PAGES; i++) {
        LCD_GotoXY(0, i);
        LCD_WriteBegin_RAM();
        for (j = 0; j < LCD_WIDTH; j++) {
            LCD_WriteRAM(*pBuf++);
            LCD_WriteRAM(*pBuf++);
        }
        LCD_WriteEnd();
    }
}

// 清屏
void LCD_Clear(void)
{
    memset(gLCDBuf, 0x0, sizeof(gLCDBuf));
}

// 绘制点
void LCD_DrawPoint(uint8_t x, uint8_t y, uint8_t gray)
{
#ifdef LCD_BUF_BITBAND
    uintptr_t addr;
#else
    uint8_t *pBuf;
    uint8_t yBit;
#endif  //LCD_BUF_BITBAND

    // 暂时去掉以提高画点效率
    /*if (x >= LCD_WIDTH || y >= LCD_HEIGHT) {
        return;
    }*/

#ifdef LCD_BUF_BITBAND
    addr = BITBAND_SRAM((uintptr_t)gLCDBuf[y >> 3][x], y & 0x7);
    MEM_ADDR(addr) = gray >> 1;
    MEM_ADDR(addr + 32) = gray & 0x1;
#else
    yBit = y & 0x7;
    pBuf = &gLCDBuf[y >> 3][x][0];
    *pBuf = (*pBuf & ~(0x1 << yBit)) | ((gray >> 1) << yBit);
    pBuf++;
    *pBuf = (*pBuf & ~(0x1 << yBit)) | ((gray & 0x1) << yBit);
#endif //LCD_BUF_BITBAND
}

// 绘制竖直线
void LCD_DrawVLine(uint8_t x, uint8_t y0, uint8_t y1, uint8_t gray)
{
#ifdef LCD_FAST_DRAW_VLINE
    // 快速算法，合并同一字节内位操作
    uint8_t *pBuf;
    size_t remPixels;           // 剩余要绘制的pixel数
    size_t yBit;                // 非字节对齐的bit数
    uint8_t pixel8mask;         // 不需修改的bits掩膜
    uint8_t pixel8h, pixel8l;   // 一列8个pixel的新值

    if (y0 > y1) {
        uint8_t temp;

        temp = y1;
        y1 = y0;
        y0 = temp;
    }
    
    pixel8h = -(gray >> 1);
    pixel8l = -(gray & 0x1);
    remPixels = y1 - y0 + 1;
    pBuf = &gLCDBuf[y0 >> 3][x][0];

    // 绘制开始的非字节对齐部分
    yBit = y0 & 0x7;
    if (yBit > 0) {
        pixel8mask = (0xff >> (8 - yBit));
        if (yBit + remPixels < 8) {
            pixel8mask |= (0xff << (yBit + remPixels));
            remPixels = 0;
        } else {
            remPixels -= (8 - yBit);
        }
        *pBuf = (*pBuf & pixel8mask) | (pixel8h & ~pixel8mask);
        *(pBuf + 1) = (*(pBuf + 1) & pixel8mask) | (pixel8l & ~pixel8mask);
        pBuf += LCD_WIDTH * LCD_BITS_PER_PIXEL;
    }

    if (remPixels > 0) {
        size_t numBytes = remPixels >> 3;

        // 绘制字节对齐的部分
        if (numBytes > 0) {
            remPixels -= numBytes << 3;
            while (numBytes--) {
                *pBuf = pixel8h;
                *(pBuf + 1) = pixel8l;
                pBuf += LCD_WIDTH * LCD_BITS_PER_PIXEL;
            }
        }

        // 绘制结束的非字节对齐部分
        pixel8mask = (0xff << remPixels);
        *pBuf = (*pBuf & pixel8mask) | (pixel8h & ~pixel8mask);
        *(pBuf + 1) = (*(pBuf + 1) & pixel8mask) | (pixel8l & ~pixel8mask);
    }
#else //!LCD_FAST_DRAW_VLINE
    // 标准算法
    if (y0 > y1) {
        uint8_t temp;

        temp = y1;
        y1 = y0;
        y0 = temp;
    }

    do {
        LCD_DrawPoint(x, y0++, gray);
    } while (y1 >= y0);
#endif //LCD_FAST_DRAW_VLINE
}

// 绘制水平线
void LCD_DrawHLine(uint8_t x0, uint8_t x1, uint8_t y, uint8_t gray)
{
#ifdef LCD_FAST_DRAW_HLINE
    // 快速算法，合并同一页的位操作
    uint16_t *pBuf;
    size_t remPixels;
    size_t yBit;
    uint16_t pixelmask;
    uint16_t pixel;

    if (x0 > x1) {
        uint8_t temp;

        temp = x1;
        x1 = x0;
        x0 = temp;
    }

    remPixels = x1 - x0 + 1;
    pBuf = (uint16_t *)&gLCDBuf[y >> 3][x0][0];
    yBit = y & 0x7;
    pixelmask =  ~((0x1 << yBit) | (0x1 << (yBit + 8)));
    pixel = ((gray >> 1) << yBit) | ((gray & 0x1) << (yBit + 8));

    do {
        *pBuf = (*pBuf & pixelmask) | pixel;
        pBuf++;
    } while (remPixels-- > 0);
#else //!LCD_FAST_DRAW_HLINE
    // 标准算法
    if (x0 > x1) {
        uint8_t temp;

        temp = x1;
        x1 = x0;
        x0 = temp;
    }

    do {
        LCD_DrawPoint(x0++, y, gray);
    } while (x1 >= x0);
#endif //LCD_FAST_DRAW_HLINE
}

// 绘制线段
void LCD_DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t gray)
{
    int8_t dx, dy;              // 直线x轴,y轴差值变量
    int8_t dx_sym, dy_sym;      // x轴,y轴增长方向，为-1时减值方向，为1时增值方向
    int8_t dx_2, dy_2;          // dx*2,dy*2值变量，用于加快运算速度
    int8_t di;                  // 决策变量

    dx = x1 - x0;
    dy = y1 - y0;

    if (dx < 0) {
        dx_sym = -1;
    } else if (dx > 0) {
        dx_sym = 1;
    } else {
        LCD_DrawVLine(x0, y0, y1, gray);
        return;
    }

    if (dy > 0) {
        dy_sym = 1;
    } else if (dy < 0) {
        dy_sym = -1;
    } else {
        LCD_DrawHLine(x0, x1, y0, gray);
        return;
    }

    dx = dx_sym * dx;
    dy = dy_sym * dy;

    dx_2 = dx * 2;
    dy_2 = dy * 2;

    if (dx >= dy) {
        di = dy_2 - dx;
        while (x0 != x1) {
            LCD_DrawPoint(x0, y0, gray);
            x0 += dx_sym;
            if (di < 0) {
                di += dy_2;
            } else {
                di += dy_2 - dx_2;
                y0 += dy_sym;
            }
        }
        LCD_DrawPoint(x0, y0, gray);
    } else {
        di = dx_2 - dy;
        while (y0 != y1) {
            LCD_DrawPoint(x0, y0, gray);
            y0 += dy_sym;
            if (di < 0) {
                di += dx_2;
            } else {
                di += dx_2 - dy_2;
                x0 += dx_sym;
            }
        }
        LCD_DrawPoint(x0, y0, gray);
    }
}

// 绘制矩形框
void LCD_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t gray)
{
    LCD_DrawHLine(x, x + w, y, gray);
    LCD_DrawHLine(x, x + w, y + h, gray);
    LCD_DrawVLine(x, y, y + h, gray);
    LCD_DrawVLine(x + w, y, y + h, gray);
}

// 填充矩形区域
void LCD_FillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t gray)
{
    uint8_t x1, y1;

    x1 = x + w - 1;
    y1 = y + h - 1;

    while (x <= x1) {
        LCD_DrawVLine(x, y, y1, gray);
        x++;
    }
}

// 透明显示单色图片，高度必须为8的倍数
// 参数：图片指针，x，y/8，w，h/8，灰度
void LCD_DrawPicture8_1Bpp(const void *pic, uint8_t x, uint8_t y8, uint8_t w, uint8_t h8, uint8_t gray)
{
    const uint8_t *dptr;
    uint8_t *pbuf;
    uint8_t gray8h, gray8l;
    size_t i, j;

    if ((y8 + h8) > LCD_PAGES) {
        return;
    }
    if ((x + w) > LCD_WIDTH) {
        return;
    }

    gray8h = -(gray >> 1);
    gray8l = -(gray & 0x1);

    dptr = (const uint8_t *)pic;

    for (i = 0; i < h8; i++) {
        pbuf = &gLCDBuf[y8 + i][x][0];
        for (j = 0; j < w; j++) {
            *pbuf = (*pbuf & ~*dptr) | (gray8h & *dptr);
            pbuf++;
            *pbuf = (*pbuf & ~*dptr) | (gray8l & *dptr);
            pbuf++;
            dptr++;
        }
    }
}

// 绘制单色图片
// 参数：图片指针，x，y，w，h， 灰度
void LCD_DrawPicture_1Bpp(const void *pic, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t gray)
{

}

// 绘制4灰度图片
// 参数：图片指针，x，y，w，h
void LCD_DrawPicture_2Bpp(const void *pic, uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{

}

/*-----------------------------------*/

// 绘制字符(仅透明覆盖模式)
// 参数：字符，x，y，字体，灰度
void LCD_DrawChar(char ch, uint8_t x, uint8_t y, uint8_t font, uint8_t gray)
{
    const uint8_t *pFont;
#ifdef LCD_BUF_BITBAND
    uintptr_t addr;
#else
    uint8_t *pBuf;
    size_t yBit;
#endif //LCD_BUF_BITBAND
    uint8_t grayh, grayl;
    uint8_t chw, chh;
    uint8_t x1, y1;
    size_t i, j;
    uint8_t dot8;

    grayh = (gray >> 1);
    grayl = (gray & 0x1);

    switch (font) {
    case FONT_SIZE_3X6:
        chw = 3;
        chh = 8;
        pFont = &AsciiFont_3x6[(ch - 0x20) * 3];
        break;
    case FONT_SIZE_5X8:
        chw = 5;
        chh = 8;
        pFont = &AsciiFont_5x8[(ch - 0x20) * 5];
        break;
    default:
        return;
    }

    x1 = x + chw - 1;
    y1 = y + chh - 1;

    for (i = x; i <= x1; i++) {
        dot8 = *pFont++;
        for (j = y; j <= y1; j++) {
            if (dot8 & 0x1) {
#ifdef LCD_BUF_BITBAND
                addr = BITBAND_SRAM((uintptr_t)gLCDBuf[j >> 3][i], j & 0x7);
                MEM_ADDR(addr) = grayh;
                MEM_ADDR(addr + 32) = grayl;
#else
                yBit = j & 0x7;
                pBuf = (uint8_t *)gLCDBuf[j >> 3][i];
                *pBuf = (*pBuf & ~(0x1 << yBit)) | (grayh << yBit);
                pBuf++;
                *pBuf = (*pBuf & ~(0x1 << yBit)) | (grayl << yBit);
#endif //LCD_BUF_BITBAND
            }
            dot8 >>= 1;
        }
    }
}

// 绘制字符串(仅透明覆盖模式)
// 参数：字符，x，y，字体，灰度
void LCD_DrawString(const char *str, uint8_t x, uint8_t y, uint8_t font, uint8_t gray)
{
    uint8_t dx, dy;

    switch (font) {
    case FONT_SIZE_3X6:
        dx = 4;
        dy = 8;
        break;
    case FONT_SIZE_5X8:
        dx = 6;
        dy = 8;
        break;
    default:
        return;
    }

    while (*str != '\0') {
        LCD_DrawChar(*str++, x, y, font, gray);
        x += dx;

        if (x >= LCD_WIDTH) {
            x = 0;
            y += dy;
            if (y >= LCD_HEIGHT) {
                y = 0;
            }
        }
    }
}
