#ifndef __LCD_H__
#define __LCD_H__

#include "base.h"

// LCD基本规格定义
#define LCD_HEIGHT          (80)
#define LCD_WIDTH           (128)
#define LCD_BITS_PER_PIXEL  (2)
#define LCD_PIXEL_PER_PAGE  (8)
#define LCD_PAGES           (LCD_HEIGHT / LCD_PIXEL_PER_PAGE)

// 字体枚举
enum {
    FONT_SIZE_3X6,
    FONT_SIZE_5X8,
    FONT_SIZE_8X16,
    FONT_SIZE_MAX
};

void LCD_Init(void);
void LCD_LightOnOff(BOOL isOn);
void LCD_DisplayOnOff(BOOL isOn);
void LCD_PowerSaveOnOff(BOOL isOn);

void LCD_Clear(void);
void LCD_Refresh(void);

void LCD_DrawPoint(uint8_t x, uint8_t y, uint8_t gray);
void LCD_DrawVLine(uint8_t x, uint8_t y0, uint8_t y1, uint8_t gray);
void LCD_DrawHLine(uint8_t x0, uint8_t x1, uint8_t y, uint8_t gray);
void LCD_DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t gray);
void LCD_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t gray);
void LCD_FillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t gray);

void LCD_DrawChar(char ch, uint8_t x, uint8_t y, uint8_t font, uint8_t gray);
void LCD_DrawString(const char *str, uint8_t x, uint8_t y, uint8_t font, uint8_t gray);
void LCD_DrawChar8(char ch, uint8_t x, uint8_t y8, uint8_t font, uint8_t gray);
void LCD_DrawString8(const char *str, uint8_t x, uint8_t y8, uint8_t font, uint8_t gray);

void LCD_DrawPicture8_1Bpp(const void *pic, uint8_t x, uint8_t y8, uint8_t w, uint8_t h8, uint8_t gray0, uint8_t gray1);
void LCD_DrawPicture8_2Bpp(const void *pic, uint8_t x, uint8_t y8, uint8_t w, uint8_t h8);
void LCD_DrawTransPic8_1Bpp(const void *pic, uint8_t x, uint8_t y8, uint8_t w, uint8_t h8, uint8_t gray);

#endif //__LCD_H__
