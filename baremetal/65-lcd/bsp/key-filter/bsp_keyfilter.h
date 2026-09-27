#ifndef BSP_KEYFILTER_H
#define BSP_KEYFILTER_H

/**
 * filterkey_init() - 初始化按键中断和 EPIT1 消抖定时器
 */
void filterkey_init(void);

/**
 * filtertimer_init() - 初始化消抖定时器
 * @value: EPIT1 重装值
 */
void filtertimer_init(unsigned int value);

/**
 * filtertimer_stop() - 停止消抖定时器
 */
void filtertimer_stop(void);

/**
 * filtertimer_restart() - 使用新重装值重启消抖定时器
 * @value: EPIT1 重装值
 */
void filtertimer_restart(unsigned int value);

/**
 * filtertimer_irqhandler() - 处理消抖定时器中断
 * @giccIar: GIC 中断确认值
 * @userParam: 注册时传入的用户参数
 *
 * Context: IRQ 分发路径调用，不允许睡眠。
 */
void filtertimer_irqhandler(unsigned int giccIar, void *userParam);

/**
 * gpio1_16_31_irqhandler() - 处理 GPIO1 高半组按键中断
 * @giccIar: GIC 中断确认值
 * @userParam: 注册时传入的用户参数
 *
 * Context: IRQ 分发路径调用，不允许睡眠。
 */
void gpio1_16_31_irqhandler(unsigned int giccIar, void *userParam);

#endif /* BSP_KEYFILTER_H */
