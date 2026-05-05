#ifndef _BSP_EXIT_H
#define _BSP_EXIT_H

#include "imx6ul.h"

void exit_init(void);
void gpio1_io18_irqhandler(unsigned int giccIar, void *userParam);

#endif /* _BSP_EXIT_H */