#include "bsp_lcd.h"
#include "bsp_gpio.h"
#include "bsp_delay.h"
#include "stdio.h"

#define LCD_CTRL_DOTCLK_MODE   ((1U << 19) | (1U << 17) | (3U << 10) | (3U << 8) | (1U << 5))
#define LCD_CTRL1_ARGB8888     (0x7U << 16)
#define LCD_VDCTRL0_COMMON(vspw) ((0U << 29) | (1U << 28) | (0U << 27) | \
                                  (0U << 26) | (1U << 21) | (1U << 20) | (vspw))

typedef struct {
    unsigned short id;
    unsigned short width;
    unsigned short height;
    unsigned short vspw;
    unsigned short vbpd;
    unsigned short vfpd;
    unsigned short hspw;
    unsigned short hbpd;
    unsigned short hfpd;
    unsigned char loopDiv;
    unsigned char prediv;
    unsigned char div;
    unsigned char dotclk_rising;
    unsigned char enable_active_high;
} lcd_panel_config_t;

/* 液晶屏参数结构体 */
struct tftlcd_typedef tftlcd_dev;

static const lcd_panel_config_t lcd_panel_configs[] = {
    /* id,      width, height, vspw, vbpd, vfpd, hspw, hbpd, hfpd, loop, pre, div, dotclk, en */
    { ATK4342,   480,    272,    1,    8,    8,    1,   40,    5,   27,   8,   8,      0,  1 },
    { ATK4384,   800,    480,    3,   32,   13,   48,   88,   40,   42,   4,   8,      0,  1 },
    { ATK7084,   800,    480,    1,   23,   22,    1,   46,  210,   30,   3,   7,      0,  1 },
    { ATK7016,  1024,    600,    3,   20,   12,   20,  140,  160,   32,   3,   5,      0,  1 },
    { ATK1018,  1280,    800,    3,   10,   10,   10,   80,   70,   35,   3,   5,      0,  1 },
    { ATKVGA,   1366,    768,    3,   24,    3,  143,  213,   70,   32,   3,   3,      1,  0 },
};

static const lcd_panel_config_t *lcd_find_panel_config(unsigned short lcdid)
{
    unsigned int i;

    for (i = 0; i < sizeof(lcd_panel_configs) / sizeof(lcd_panel_configs[0]); i++) {
        if (lcd_panel_configs[i].id == lcdid) {
            return &lcd_panel_configs[i];
        }
    }

    return 0;
}

static void lcd_apply_panel_config(const lcd_panel_config_t *cfg)
{
    if (cfg == 0) {
        return;
    }

    tftlcd_dev.width = cfg->width;
    tftlcd_dev.height = cfg->height;
    tftlcd_dev.vspw = cfg->vspw;
    tftlcd_dev.vbpd = cfg->vbpd;
    tftlcd_dev.vfpd = cfg->vfpd;
    tftlcd_dev.hspw = cfg->hspw;
    tftlcd_dev.hbpd = cfg->hbpd;
    tftlcd_dev.hfpd = cfg->hfpd;

    lcdclk_init(cfg->loopDiv, cfg->prediv, cfg->div);
}

static void lcdif_init_registers(const lcd_panel_config_t *cfg)
{
    unsigned int dotclk_polarity = 0;
    unsigned int enable_polarity = 1;

    if (cfg != 0) {
        dotclk_polarity = cfg->dotclk_rising ? 1U : 0U;
        enable_polarity = cfg->enable_active_high ? 1U : 0U;
    }

    LCDIF->CTRL |= LCD_CTRL_DOTCLK_MODE;
    LCDIF->CTRL1 = LCD_CTRL1_ARGB8888;
    LCDIF->TRANSFER_COUNT = ((unsigned int)tftlcd_dev.height << 16) | tftlcd_dev.width;

    LCDIF->VDCTRL0 = LCD_VDCTRL0_COMMON(tftlcd_dev.vspw) |
                     (dotclk_polarity << 25) |
                     (enable_polarity << 24);
    LCDIF->VDCTRL1 = tftlcd_dev.height + tftlcd_dev.vspw + tftlcd_dev.vfpd + tftlcd_dev.vbpd;
    LCDIF->VDCTRL2 = ((unsigned int)tftlcd_dev.hspw << 18) |
                     (tftlcd_dev.width + tftlcd_dev.hspw + tftlcd_dev.hfpd + tftlcd_dev.hbpd);
    LCDIF->VDCTRL3 = ((unsigned int)(tftlcd_dev.hbpd + tftlcd_dev.hspw) << 16) |
                     (tftlcd_dev.vbpd + tftlcd_dev.vspw);
    LCDIF->VDCTRL4 = (1U << 18) | tftlcd_dev.width;
    LCDIF->CUR_BUF = (unsigned int)tftlcd_dev.framebuffer;
    LCDIF->NEXT_BUF = (unsigned int)tftlcd_dev.framebuffer;
}

/*
 * @description : 初始化LCD
 * @param       : 无
 * @return      : 无
 */
void lcd_init(void)
{
    const lcd_panel_config_t *cfg;
    unsigned short lcdid;

    lcdid = lcd_read_panelid();
    tftlcd_dev.id = lcdid;
    printf("LCD ID=%#X\r\n", lcdid);

    lcdgpio_init();
    lcd_reset();
    delayms(10);
    lcd_noreset();

    cfg = lcd_find_panel_config(lcdid);
    lcd_apply_panel_config(cfg);

    tftlcd_dev.pixsize = 4;                 /* ARGB8888模式，每个像素4字节 */
    tftlcd_dev.framebuffer = LCD_FRAMEBUF_ADDR;
    tftlcd_dev.backcolor = LCD_WHITE;
    tftlcd_dev.forecolor = LCD_BLACK;

    lcdif_init_registers(cfg);

    lcd_enable();
    delayms(10);
    lcd_clear(LCD_WHITE);
}

/*
 * 读取屏幕ID，
 * 描述：LCD_DATA23=R7(M0);LCD_DATA15=G7(M1);LCD_DATA07=B7(M2);
 * 		M2:M1:M0
 *		0 :0 :0	//4.3寸480*272 RGB屏,ID=0X4342
 *		0 :0 :1	//7寸800*480 RGB屏,ID=0X7084
 *	 	0 :1 :0	//7寸1024*600 RGB屏,ID=0X7016
 *  	1 :0 :1	//10.1寸1280*800,RGB屏,ID=0X1018
 *		1 :0 :0	//4.3寸800*480 RGB屏,ID=0X4384
 * @param 		: 无
 * @return 		: 屏幕ID
 */
unsigned short lcd_read_panelid(void)
{
	unsigned char idx = 0;

	/* 配置屏幕ID信号线 */
	IOMUXC_SetPinMux(IOMUXC_LCD_VSYNC_GPIO3_IO03, 0);
	IOMUXC_SetPinConfig(IOMUXC_LCD_VSYNC_GPIO3_IO03, 0X10B0);

	/* 打开模拟开关 */
	gpio_pin_config_t idio_config;
	idio_config.direction = kGPIO_DigitalOutput;
	idio_config.outputLogic = 1;
	gpio_init(GPIO3, 3, &idio_config);

	/* 读取ID值，设置G7 B7 R7为输入 */
	IOMUXC_SetPinMux(IOMUXC_LCD_DATA07_GPIO3_IO12, 0);		/* B7(M2) */
	IOMUXC_SetPinMux(IOMUXC_LCD_DATA15_GPIO3_IO20, 0);		/* G7(M1) */
	IOMUXC_SetPinMux(IOMUXC_LCD_DATA23_GPIO3_IO28, 0);		/* R7(M0) */

	IOMUXC_SetPinConfig(IOMUXC_LCD_DATA07_GPIO3_IO12, 0xF080);
	IOMUXC_SetPinConfig(IOMUXC_LCD_DATA15_GPIO3_IO20, 0xF080);
	IOMUXC_SetPinConfig(IOMUXC_LCD_DATA23_GPIO3_IO28, 0xF080);  

	idio_config.direction = kGPIO_DigitalInput;
	gpio_init(GPIO3, 12, &idio_config);
	gpio_init(GPIO3, 20, &idio_config);
	gpio_init(GPIO3, 28, &idio_config);

	idx = (unsigned char)gpio_pinread(GPIO3, 28); 	/* 读取M0 */
	idx |= (unsigned char)gpio_pinread(GPIO3, 20) << 1;	/* 读取M1 */
	idx |= (unsigned char)gpio_pinread(GPIO3, 12) << 2;	/* 读取M2 */

	if(idx==0)return ATK4342;		//4.3寸屏,480*272分辨率
	else if(idx==1)return ATK7084;	//7寸屏,800*480分辨率
	else if(idx==2)return ATK7016;	//7寸屏,1024*600分辨率
	else if(idx==4)return ATK4384;	//4寸屏,800*480分辨率
	else if(idx==5)return ATK1018;	//10.1寸屏,1280*800分辨率		
	else if(idx==7)return ATKVGA;   //VGA模块，1366*768分辨率
	else return 0;

}
/*
 * IO引脚: 	LCD_DATA00 -> LCD_B0
 *			LCD_DATA01 -> LCD_B1
 *			LCD_DATA02 -> LCD_B2
 *			LCD_DATA03 -> LCD_B3
 *			LCD_DATA04 -> LCD_B4
 *			LCD_DATA05 -> LCD_B5
 *			LCD_DATA06 -> LCD_B6
 *			LCD_DATA07 -> LCD_B7
 *
 *			LCD_DATA08 -> LCD_G0
 *			LCD_DATA09 -> LCD_G1
 *			LCD_DATA010 -> LCD_G2
 *			LCD_DATA011 -> LCD_G3
 *			LCD_DATA012 -> LCD_G4
 *			LCD_DATA012 -> LCD_G4
 *			LCD_DATA013 -> LCD_G5
 *			LCD_DATA014 -> LCD_G6
 *			LCD_DATA015 -> LCD_G7
 *
 *			LCD_DATA016 -> LCD_R0
 *			LCD_DATA017 -> LCD_R1
 *			LCD_DATA018 -> LCD_R2 
 *			LCD_DATA019 -> LCD_R3
 *			LCD_DATA020 -> LCD_R4
 *			LCD_DATA021 -> LCD_R5
 *			LCD_DATA022 -> LCD_R6
 *			LCD_DATA023 -> LCD_R7
 *
 *			LCD_CLK -> LCD_CLK
 *			LCD_VSYNC -> LCD_VSYNC
 *			LCD_HSYNC -> LCD_HSYNC
 *			LCD_DE -> LCD_DE
 *			LCD_BL -> GPIO1_IO08 
 */
 
/*
 * @description	: LCD GPIO初始化
 * @param 		: 无
 * @return 		: 无
 */
void lcdgpio_init(void)
{
	gpio_pin_config_t gpio_config;
	

	/* 1、IO初始化复用功能 */
	IOMUXC_SetPinMux(IOMUXC_LCD_DATA00_LCDIF_DATA00,0);
	IOMUXC_SetPinMux(IOMUXC_LCD_DATA01_LCDIF_DATA01,0);
	IOMUXC_SetPinMux(IOMUXC_LCD_DATA02_LCDIF_DATA02,0);
	IOMUXC_SetPinMux(IOMUXC_LCD_DATA03_LCDIF_DATA03,0);
	IOMUXC_SetPinMux(IOMUXC_LCD_DATA04_LCDIF_DATA04,0);
	IOMUXC_SetPinMux(IOMUXC_LCD_DATA05_LCDIF_DATA05,0);
	IOMUXC_SetPinMux(IOMUXC_LCD_DATA06_LCDIF_DATA06,0);
	IOMUXC_SetPinMux(IOMUXC_LCD_DATA07_LCDIF_DATA07,0);
	
	IOMUXC_SetPinMux(IOMUXC_LCD_DATA08_LCDIF_DATA08,0);
	IOMUXC_SetPinMux(IOMUXC_LCD_DATA09_LCDIF_DATA09,0);
	IOMUXC_SetPinMux(IOMUXC_LCD_DATA10_LCDIF_DATA10,0);
	IOMUXC_SetPinMux(IOMUXC_LCD_DATA11_LCDIF_DATA11,0);
	IOMUXC_SetPinMux(IOMUXC_LCD_DATA12_LCDIF_DATA12,0);
	IOMUXC_SetPinMux(IOMUXC_LCD_DATA13_LCDIF_DATA13,0);
	IOMUXC_SetPinMux(IOMUXC_LCD_DATA14_LCDIF_DATA14,0);
	IOMUXC_SetPinMux(IOMUXC_LCD_DATA15_LCDIF_DATA15,0);

	IOMUXC_SetPinMux(IOMUXC_LCD_DATA16_LCDIF_DATA16,0);
	
	IOMUXC_SetPinMux(IOMUXC_LCD_DATA17_LCDIF_DATA17,0);
	IOMUXC_SetPinMux(IOMUXC_LCD_DATA18_LCDIF_DATA18,0);
	IOMUXC_SetPinMux(IOMUXC_LCD_DATA19_LCDIF_DATA19,0);
	IOMUXC_SetPinMux(IOMUXC_LCD_DATA20_LCDIF_DATA20,0);
	IOMUXC_SetPinMux(IOMUXC_LCD_DATA21_LCDIF_DATA21,0);
	IOMUXC_SetPinMux(IOMUXC_LCD_DATA22_LCDIF_DATA22,0);
	IOMUXC_SetPinMux(IOMUXC_LCD_DATA23_LCDIF_DATA23,0);

	IOMUXC_SetPinMux(IOMUXC_LCD_CLK_LCDIF_CLK,0);	
	IOMUXC_SetPinMux(IOMUXC_LCD_ENABLE_LCDIF_ENABLE,0);	
	IOMUXC_SetPinMux(IOMUXC_LCD_HSYNC_LCDIF_HSYNC,0);
	IOMUXC_SetPinMux(IOMUXC_LCD_VSYNC_LCDIF_VSYNC,0);

	IOMUXC_SetPinMux(IOMUXC_GPIO1_IO08_GPIO1_IO08,0);			/* 背光BL引脚      */
	IOMUXC_SetPinConfig(IOMUXC_UART1_CTS_B_GPIO1_IO18,0xF080);
					

	/* 2、配置LCD IO属性	
	 *bit 16:0 HYS关闭
	 *bit [15:14]: 0 默认22K上拉
	 *bit [13]: 0 pull功能
	 *bit [12]: 0 pull/keeper使能 
	 *bit [11]: 0 关闭开路输出
	 *bit [7:6]: 10 速度100Mhz
	 *bit [5:3]: 111 驱动能力为R0/7
	 *bit [0]: 1 高转换率
	 */
	IOMUXC_SetPinConfig(IOMUXC_LCD_DATA00_LCDIF_DATA00,0xB9);
	IOMUXC_SetPinConfig(IOMUXC_LCD_DATA01_LCDIF_DATA01,0xB9);
	IOMUXC_SetPinConfig(IOMUXC_LCD_DATA02_LCDIF_DATA02,0xB9);
	IOMUXC_SetPinConfig(IOMUXC_LCD_DATA03_LCDIF_DATA03,0xB9);
	IOMUXC_SetPinConfig(IOMUXC_LCD_DATA04_LCDIF_DATA04,0xB9);
	IOMUXC_SetPinConfig(IOMUXC_LCD_DATA05_LCDIF_DATA05,0xB9);
	IOMUXC_SetPinConfig(IOMUXC_LCD_DATA06_LCDIF_DATA06,0xB9);
	IOMUXC_SetPinConfig(IOMUXC_LCD_DATA07_LCDIF_DATA07,0xB9);
	IOMUXC_SetPinConfig(IOMUXC_LCD_DATA08_LCDIF_DATA08,0xB9);
	IOMUXC_SetPinConfig(IOMUXC_LCD_DATA09_LCDIF_DATA09,0xB9);
	IOMUXC_SetPinConfig(IOMUXC_LCD_DATA10_LCDIF_DATA10,0xB9);
	IOMUXC_SetPinConfig(IOMUXC_LCD_DATA11_LCDIF_DATA11,0xB9);
	IOMUXC_SetPinConfig(IOMUXC_LCD_DATA12_LCDIF_DATA12,0xB9);
	IOMUXC_SetPinConfig(IOMUXC_LCD_DATA13_LCDIF_DATA13,0xB9);
	IOMUXC_SetPinConfig(IOMUXC_LCD_DATA14_LCDIF_DATA14,0xB9);
	IOMUXC_SetPinConfig(IOMUXC_LCD_DATA15_LCDIF_DATA15,0xB9);
	IOMUXC_SetPinConfig(IOMUXC_LCD_DATA16_LCDIF_DATA16,0xB9);
	IOMUXC_SetPinConfig(IOMUXC_LCD_DATA17_LCDIF_DATA17,0xB9);
	IOMUXC_SetPinConfig(IOMUXC_LCD_DATA18_LCDIF_DATA18,0xB9);
	IOMUXC_SetPinConfig(IOMUXC_LCD_DATA19_LCDIF_DATA19,0xB9);
	IOMUXC_SetPinConfig(IOMUXC_LCD_DATA20_LCDIF_DATA20,0xB9);
	IOMUXC_SetPinConfig(IOMUXC_LCD_DATA21_LCDIF_DATA21,0xB9);
	IOMUXC_SetPinConfig(IOMUXC_LCD_DATA22_LCDIF_DATA22,0xB9);
	IOMUXC_SetPinConfig(IOMUXC_LCD_DATA23_LCDIF_DATA23,0xB9);

	IOMUXC_SetPinConfig(IOMUXC_LCD_CLK_LCDIF_CLK,0xB9);
	IOMUXC_SetPinConfig(IOMUXC_LCD_ENABLE_LCDIF_ENABLE,0xB9);
	IOMUXC_SetPinConfig(IOMUXC_LCD_HSYNC_LCDIF_HSYNC,0xB9);
	IOMUXC_SetPinConfig(IOMUXC_LCD_VSYNC_LCDIF_VSYNC,0xB9);

	IOMUXC_SetPinConfig(IOMUXC_GPIO1_IO08_GPIO1_IO08,0xB9);	/* 背光BL引脚 		*/

	/* GPIO初始化 */
	gpio_config.direction = kGPIO_DigitalOutput;			/* 输出 			*/
	gpio_config.outputLogic = 1; 							/* 默认关闭背光 */
	gpio_init(GPIO1, 8, &gpio_config);						/* 背光默认打开 */
	gpio_pinwrite(GPIO1, 8, 1);								/* 打开背光     */
}

/*
 * @description		: LCD时钟初始化, LCD时钟计算公式如下：
 *                	  LCD CLK = 24 * loopDiv / prediv / div
 * @param -	loopDiv	: loopDivider值
 * @param -	prediv  : lcdifprediv值
 * @param -	div		: lcdifdiv值
 * @return 			: 无
 */
void lcdclk_init(unsigned char loopDiv, unsigned char prediv, unsigned char div)
{
	/* 先初始化video pll 
     * VIDEO PLL = OSC24M * (loopDivider + (denominator / numerator)) / postDivider
 	 *不使用小数分频器，因此denominator和numerator设置为0
 	 */
	CCM_ANALOG->PLL_VIDEO_NUM = 0;		/* 不使用小数分频器 */
	CCM_ANALOG->PLL_VIDEO_DENOM = 0;	

	/*
     * PLL_VIDEO寄存器设置
     * bit[13]:    1   使能VIDEO PLL时钟
     * bit[20:19]  2  设置postDivider为1分频
     * bit[6:0] : 32  设置loopDivider寄存器
	 */
	CCM_ANALOG->PLL_VIDEO =  (2 << 19) | (1 << 13) | (loopDiv << 0); 

	/*
     * MISC2寄存器设置
     * bit[31:30]: 0  VIDEO的post-div设置，时钟源来源于postDivider，1分频
	 */
	CCM_ANALOG->MISC2 &= ~(3 << 30);
	CCM_ANALOG->MISC2 = 0 << 30;

	/* LCD时钟源来源与PLL5，也就是VIDEO           PLL  */
	CCM->CSCDR2 &= ~(7 << 15);  	
	CCM->CSCDR2 |= (2 << 15);			/* 设置LCDIF_PRE_CLK使用PLL5 */

	/* 设置LCDIF_PRE分频 */
	CCM->CSCDR2 &= ~(7 << 12);		
	CCM->CSCDR2 |= (prediv - 1) << 12;	/* 设置分频  */

	/* 设置LCDIF分频 */
	CCM->CBCMR &= ~(7 << 23);					
	CCM->CBCMR |= (div - 1) << 23;				

	/* 设置LCD时钟源为LCDIF_PRE时钟 */
	CCM->CSCDR2 &= ~(7 << 9);					/* 清除原来的设置		 	*/
	CCM->CSCDR2 |= (0 << 9);					/* LCDIF_PRE时钟源选择LCDIF_PRE时钟 */
}

/*
 * @description	: 复位ELCDIF接口
 * @param 		: 无
 * @return 		: 无
 */
void lcd_reset(void)
{
	LCDIF->CTRL  = 1<<31; /* 强制复位 */
}

/*
 * @description	: 结束复位ELCDIF接口
 * @param 		: 无
 * @return 		: 无
 */
void lcd_noreset(void)
{
	LCDIF->CTRL  = 0<<31; /* 取消强制复位 */
}

/*
 * @description	: 使能ELCDIF接口
 * @param 		: 无
 * @return 		: 无
 */
void lcd_enable(void)
{
	LCDIF->CTRL |= 1<<0; /* 使能ELCDIF */
}

/*
 * @description		: 画点函数 
 * @param - x		: x轴坐标
 * @param - y		: y轴坐标
 * @param - color	: 颜色值
 * @return 			: 无
 */
void lcd_drawpoint(unsigned short x,unsigned short y,unsigned int color)
{ 
  	*(unsigned int*)((unsigned int)tftlcd_dev.framebuffer + 
		             tftlcd_dev.pixsize * (tftlcd_dev.width * y+x))=color;
}


/*
 * @description		: 读取指定点的颜色值
 * @param - x		: x轴坐标
 * @param - y		: y轴坐标
 * @return 			: 读取到的指定点的颜色值
 */
unsigned int lcd_readpoint(unsigned short x,unsigned short y)
{ 
	return *(unsigned int*)((unsigned int)tftlcd_dev.framebuffer + 
		   tftlcd_dev.pixsize * (tftlcd_dev.width * y + x));
}

/*
 * @description		: 清屏
 * @param - color	: 颜色值
 * @return 			: 读取到的指定点的颜色值
 */
void lcd_clear(unsigned int color)
{
	unsigned int num;
	unsigned int i = 0; 

	unsigned int *startaddr=(unsigned int*)tftlcd_dev.framebuffer;	//指向帧缓存首地址
	num=(unsigned int)tftlcd_dev.width * tftlcd_dev.height;			//缓冲区总长度
	for(i = 0; i < num; i++)
	{
		startaddr[i] = color;
	}		
}

/*
 * @description		: 以指定的颜色填充一块矩形
 * @param - x0		: 矩形起始点坐标X轴
 * @param - y0		: 矩形起始点坐标Y轴
 * @param - x1		: 矩形终止点坐标X轴
 * @param - y1		: 矩形终止点坐标Y轴
 * @param - color	: 要填充的颜色
 * @return 			: 读取到的指定点的颜色值
 */
void lcd_fill(unsigned    short x0, unsigned short y0, 
                 unsigned short x1, unsigned short y1, unsigned int color)
{ 
    unsigned short x, y;

	if(x0 < 0) x0 = 0;
	if(y0 < 0) y0 = 0;
	if(x1 >= tftlcd_dev.width) x1 = tftlcd_dev.width - 1;
	if(y1 >= tftlcd_dev.height) y1 = tftlcd_dev.height - 1;
	
    for(y = y0; y <= y1; y++)
    {
        for(x = x0; x <= x1; x++)
			lcd_drawpoint(x, y, color);
    }
}

