#ifndef BSP_LCDAPI_H
#define BSP_LCDAPI_H
/***************************************************************
Copyright © zuozhongkai Co., Ltd. 1998-2019. All rights reserved.
文件名  : bsp_lcdapi.h
描述    : LCD显示API函数头文件（优化版，功能保持不变）。
***************************************************************/
#include "imx6ul.h"
#include "bsp_lcd.h"

void lcd_drawline(unsigned short x1, unsigned short y1, unsigned short x2, unsigned short y2);
void lcd_draw_rectangle(unsigned short x1, unsigned short y1, unsigned short x2, unsigned short y2);
void lcd_draw_Circle(unsigned short x0, unsigned short y0, unsigned char r);
void lcd_showchar(unsigned short x, unsigned short y, unsigned char num, unsigned char size, unsigned char mode);
unsigned int lcd_pow(unsigned char m, unsigned char n);
void lcd_shownum(unsigned short x, unsigned short y, unsigned int num, unsigned char len, unsigned char size);
void lcd_showxnum(unsigned short x, unsigned short y, unsigned int num, unsigned char len, unsigned char size, unsigned char mode);
void lcd_show_string(unsigned short x, unsigned short y, unsigned short width, unsigned short height, unsigned char size, char *p);

#endif
