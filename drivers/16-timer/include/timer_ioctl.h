/* SPDX-License-Identifier: GPL-2.0 */
#ifndef TIMER_IOCTL_H
#define TIMER_IOCTL_H

#include <linux/ioctl.h>

/* 定时器控制命令。 */
#define CLOSE_CMD	_IO(0xef, 0x1)
#define OPEN_CMD	_IO(0xef, 0x2)
#define SETPERIOD_CMD	_IO(0xef, 0x3)

#endif /* TIMER_IOCTL_H */
