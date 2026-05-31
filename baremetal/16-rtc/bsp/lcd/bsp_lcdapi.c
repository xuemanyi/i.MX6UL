/***************************************************************
Copyright © zuozhongkai Co., Ltd. 1998-2019. All rights reserved.
文件名  : bsp_lcdapi.c
描述    : LCD API函数文件（优化版，功能保持不变）。
***************************************************************/
#include "bsp_lcdapi.h"
#include "font.h"

static unsigned char lcd_font_width(unsigned char size)
{
    return size / 2;
}

static unsigned char lcd_font_bytes(unsigned char size)
{
    return (unsigned char)((size / 8 + ((size % 8) ? 1 : 0)) * lcd_font_width(size));
}

static const unsigned char *lcd_get_ascii_font(unsigned char ch, unsigned char size)
{
    unsigned char index;

    if ((ch < ' ') || (ch > '~')) {
        return 0;
    }

    index = ch - ' ';
    switch (size) {
    case 12:
        return asc2_1206[index];
    case 16:
        return asc2_1608[index];
    case 24:
        return asc2_2412[index];
    case 32:
        return asc2_3216[index];
    default:
        return 0;
    }
}

static void lcd_draw_circle_points(int x0, int y0, int x, int y)
{
    lcd_drawpoint((unsigned short)(x0 + x), (unsigned short)(y0 + y), tftlcd_dev.forecolor);
    lcd_drawpoint((unsigned short)(x0 + y), (unsigned short)(y0 + x), tftlcd_dev.forecolor);
    lcd_drawpoint((unsigned short)(x0 - x), (unsigned short)(y0 + y), tftlcd_dev.forecolor);
    lcd_drawpoint((unsigned short)(x0 - y), (unsigned short)(y0 + x), tftlcd_dev.forecolor);
    lcd_drawpoint((unsigned short)(x0 - x), (unsigned short)(y0 - y), tftlcd_dev.forecolor);
    lcd_drawpoint((unsigned short)(x0 - y), (unsigned short)(y0 - x), tftlcd_dev.forecolor);
    lcd_drawpoint((unsigned short)(x0 + x), (unsigned short)(y0 - y), tftlcd_dev.forecolor);
    lcd_drawpoint((unsigned short)(x0 + y), (unsigned short)(y0 - x), tftlcd_dev.forecolor);
}

/* 画线函数 */
void lcd_drawline(unsigned short x1, unsigned short y1, unsigned short x2, unsigned short y2)
{
    int xerr = 0;
    int yerr = 0;
    int delta_x = (int)x2 - x1;
    int delta_y = (int)y2 - y1;
    int incx;
    int incy;
    int distance;
    int i;
    int x = x1;
    int y = y1;

    if (delta_x > 0) {
        incx = 1;
    } else if (delta_x == 0) {
        incx = 0;
    } else {
        incx = -1;
        delta_x = -delta_x;
    }

    if (delta_y > 0) {
        incy = 1;
    } else if (delta_y == 0) {
        incy = 0;
    } else {
        incy = -1;
        delta_y = -delta_y;
    }

    distance = (delta_x > delta_y) ? delta_x : delta_y;
    for (i = 0; i <= distance + 1; i++) {
        lcd_drawpoint((unsigned short)x, (unsigned short)y, tftlcd_dev.forecolor);
        xerr += delta_x;
        yerr += delta_y;

        if (xerr > distance) {
            xerr -= distance;
            x += incx;
        }
        if (yerr > distance) {
            yerr -= distance;
            y += incy;
        }
    }
}

/* 画矩形函数 */
void lcd_draw_rectangle(unsigned short x1, unsigned short y1, unsigned short x2, unsigned short y2)
{
    lcd_drawline(x1, y1, x2, y1);
    lcd_drawline(x1, y1, x1, y2);
    lcd_drawline(x1, y2, x2, y2);
    lcd_drawline(x2, y1, x2, y2);
}

/* 画圆函数 */
void lcd_draw_Circle(unsigned short x0, unsigned short y0, unsigned char r)
{
    int x = 0;
    int y = r;
    int d = 1 - r;

    while (y > x) {
        lcd_draw_circle_points(x0, y0, x, y);
        if (d < 0) {
            d += 2 * x + 3;
        } else {
            d += 2 * (x - y) + 5;
            y--;
        }
        x++;
    }
}

/* 在指定位置显示一个字符 */
void lcd_showchar(unsigned short x, unsigned short y, unsigned char num, unsigned char size, unsigned char mode)
{
    const unsigned char *font = lcd_get_ascii_font(num, size);
    unsigned char csize;
    unsigned char byte_index;
    unsigned char bit_index;
    unsigned char temp;
    unsigned short y0 = y;

    if (font == 0) {
        return;
    }

    csize = lcd_font_bytes(size);
    for (byte_index = 0; byte_index < csize; byte_index++) {
        temp = font[byte_index];
        for (bit_index = 0; bit_index < 8; bit_index++) {
            if (temp & 0x80) {
                lcd_drawpoint(x, y, tftlcd_dev.forecolor);
            } else if (mode == 0) {
                lcd_drawpoint(x, y, tftlcd_dev.backcolor);
            }

            temp <<= 1;
            y++;
            if (y >= tftlcd_dev.height) {
                return;
            }
            if ((y - y0) == size) {
                y = y0;
                x++;
                if (x >= tftlcd_dev.width) {
                    return;
                }
                break;
            }
        }
    }
}

/* 计算m的n次方 */
unsigned int lcd_pow(unsigned char m, unsigned char n)
{
    unsigned int result = 1;

    while (n--) {
        result *= m;
    }

    return result;
}

/* 显示指定的数字，高位为0的话不显示 */
void lcd_shownum(unsigned short x, unsigned short y, unsigned int num, unsigned char len, unsigned char size)
{
    unsigned char i;
    unsigned char temp;
    unsigned char enshow = 0;
    unsigned short x_step = lcd_font_width(size);

    for (i = 0; i < len; i++) {
        temp = (num / lcd_pow(10, len - i - 1)) % 10;
        if ((enshow == 0) && (i < (len - 1))) {
            if (temp == 0) {
                lcd_showchar(x + x_step * i, y, ' ', size, 0);
                continue;
            }
            enshow = 1;
        }
        lcd_showchar(x + x_step * i, y, temp + '0', size, 0);
    }
}

/* 显示指定的数字，高位为0还是显示 */
void lcd_showxnum(unsigned short x, unsigned short y, unsigned int num, unsigned char len, unsigned char size, unsigned char mode)
{
    unsigned char i;
    unsigned char temp;
    unsigned char enshow = 0;
    unsigned short x_step = lcd_font_width(size);
    unsigned char fill_zero = mode & 0x80;
    unsigned char overlay = mode & 0x01;

    for (i = 0; i < len; i++) {
        temp = (num / lcd_pow(10, len - i - 1)) % 10;
        if ((enshow == 0) && (i < (len - 1))) {
            if (temp == 0) {
                lcd_showchar(x + x_step * i, y, fill_zero ? '0' : ' ', size, overlay);
                continue;
            }
            enshow = 1;
        }
        lcd_showchar(x + x_step * i, y, temp + '0', size, overlay);
    }
}

/* 显示一串字符串 */
void lcd_show_string(unsigned short x, unsigned short y,
                     unsigned short width, unsigned short height,
                     unsigned char size, char *p)
{
    unsigned short x0 = x;
    unsigned short x_end = x + width;
    unsigned short y_end = y + height;
    unsigned short x_step = lcd_font_width(size);

    if (p == 0) {
        return;
    }

    while ((*p <= '~') && (*p >= ' ')) {
        if (x >= x_end) {
            x = x0;
            y += size;
        }
        if (y >= y_end) {
            break;
        }

        lcd_showchar(x, y, (unsigned char)*p, size, 0);
        x += x_step;
        p++;
    }
}
