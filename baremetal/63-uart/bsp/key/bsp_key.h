#ifndef BSP_KEY_H
#define BSP_KEY_H

#include "imx6ul.h"

/**
 * enum keyvalue - 按键编码
 * @KEY_NONE: 当前没有按键按下
 * @KEY0_VALUE: KEY0 按下
 * @KEY1_VALUE: KEY1 按下，保留值
 * @KEY2_VALUE: KEY2 按下，保留值
 */
enum keyvalue {
	KEY_NONE = 0,
	KEY0_VALUE,
	KEY1_VALUE,
	KEY2_VALUE,
};

/**
 * key_init() - 初始化按键 GPIO
 */
void key_init(void);

/**
 * key_getvalue() - 使用软件消抖读取按键
 *
 * Return: 无按键返回 KEY_NONE，否则返回对应 enum keyvalue。
 */
int key_getvalue(void);

#endif /* BSP_KEY_H */
