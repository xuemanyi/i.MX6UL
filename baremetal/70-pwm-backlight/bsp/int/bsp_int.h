#ifndef BSP_INT_H
#define BSP_INT_H

#include "imx6ul.h"

typedef void (*system_irq_handler_t)(unsigned int giccIar, void *param);

/**
 * struct sys_irq_handle - IRQ 处理函数注册项
 * @irqHandler: 中断处理函数
 * @userParam: 传递给处理函数的用户参数
 */
struct sys_irq_handle {
	system_irq_handler_t irqHandler;
	void *userParam;
};

typedef struct sys_irq_handle sys_irq_handle_t;

/**
 * int_init() - 初始化 GIC、向量基地址和 IRQ 处理表
 *
 * Context: 裸机启动阶段调用，此时设备中断应保持关闭。
 */
void int_init(void);

/**
 * system_irqtable_init() - 将 IRQ 表初始化为默认处理函数
 */
void system_irqtable_init(void);

/**
 * system_register_irqhandler() - 注册指定 IRQ 的处理函数
 * @irq: 中断号
 * @handler: 中断处理函数，不允许为 NULL
 * @userParam: 传递给处理函数的用户参数，允许为 NULL
 */
void system_register_irqhandler(IRQn_Type irq,
				system_irq_handler_t handler,
				void *userParam);
/**
 * system_irqhandler() - 分发 GIC 确认的 IRQ
 * @giccIar: 从 GICC_IAR 读取的中断确认值
 *
 * Context: IRQ 异常入口切换到 SVC 模式后调用，不允许睡眠。
 */
void system_irqhandler(unsigned int giccIar);

/**
 * default_irqhandler() - 未注册 IRQ 的默认处理函数
 * @giccIar: GIC 中断确认值
 * @userParam: 用户参数
 *
 * Context: 中断上下文，不允许睡眠。
 */
void default_irqhandler(unsigned int giccIar, void *userParam);

#endif /* BSP_INT_H */
