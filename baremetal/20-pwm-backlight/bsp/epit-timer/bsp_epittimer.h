#ifndef _BSP_EPITTIMER_H
#define _BSP_EPITTIMER_H

#include "imx6ul.h"

void epit1_init(unsigned int frac, unsigned int value);
void epit1_irqhandler(unsigned int giccIar, void *userParam);

#endif /* _BSP_EPITTIMER_H */