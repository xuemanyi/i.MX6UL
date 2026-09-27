#include "bsp_int.h"

static unsigned int irqNesting;
static sys_irq_handle_t irqTable[NUMBER_OF_INT_VECTORS];

/*
 * 初始化中断子系统。
 *
 * 使能设备中断前必须先初始化 GIC。
 * 异常向量基地址移动到镜像起始地址。
 */
void int_init(void)
{
	GIC_Init();
	system_irqtable_init();
	__set_VBAR((uint32_t)0x87800000);
}

/*
 * 使用默认处理函数初始化 IRQ 处理表。
 */
void system_irqtable_init(void)
{
	unsigned int i;

	irqNesting = 0;

	for (i = 0; i < NUMBER_OF_INT_VECTORS; i++) {
		system_register_irqhandler((IRQn_Type)i,
					   default_irqhandler,
					   NULL);
	}
}

/*
 * Register a C-level interrupt handler.
 */
void system_register_irqhandler(IRQn_Type irq,
				system_irq_handler_t handler,
				void *userParam)
{
	irqTable[irq].irqHandler = handler;
	irqTable[irq].userParam = userParam;
}

/*
 * 分发汇编 IRQ 入口传入的中断。
 *
 * The lower 10 bits of GICC_IAR contain the interrupt ID.
 */
void system_irqhandler(unsigned int giccIar)
{
	uint32_t intNum;

	intNum = giccIar & 0x3ffUL;

	if ((intNum == 1023) || (intNum >= NUMBER_OF_INT_VECTORS)) {
		return;
	}

	irqNesting++;

	irqTable[intNum].irqHandler(intNum, irqTable[intNum].userParam);

	irqNesting--;
}

/*
 * 默认 IRQ 处理函数。
 *
 * 未注册专用处理函数时使用该默认处理函数。
 */
void default_irqhandler(unsigned int giccIar, void *userParam)
{
	(void)giccIar;
	(void)userParam;

	while (1) {
	}
}