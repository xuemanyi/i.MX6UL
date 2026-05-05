#ifndef _BSP_KEYFILTER_H
#define _BSP_KEYFILTER_H

void filterkey_init(void);
void filtertimer_init(unsigned int value);
void filtertimer_stop(void);
void filtertimer_restart(unsigned int value);
void filtertimer_irqhandler(unsigned int giccIar, void *userParam);
void gpio1_16_31_irqhandler(unsigned int giccIar, void *userParam);

#endif /* _BSP_KEYFILTER_H */