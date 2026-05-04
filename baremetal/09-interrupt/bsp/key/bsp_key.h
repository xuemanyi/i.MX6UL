#ifndef __BSP_KEY_H
#define __BSP_KEY_H

/*
 * Key driver interface.
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

#endif /* __BSP_KEY_H */