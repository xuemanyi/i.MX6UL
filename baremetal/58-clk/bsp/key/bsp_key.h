#ifndef BSP_KEY_H
#define BSP_KEY_H

/*
 * 按键驱动接口。
 */

#include "imx6ul.h"

enum keyvalue {
	KEY_NONE = 0,
	KEY0_VALUE,
	KEY1_VALUE,
	KEY2_VALUE,
};

void key_init(void);
int key_getvalue(void);

#endif /* BSP_KEY_H */