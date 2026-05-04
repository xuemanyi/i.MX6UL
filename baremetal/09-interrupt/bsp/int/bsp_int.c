#include "bsp_int.h"

static unsigned int irqNesting;
static sys_irq_handle_t irqTable[NUMBER_OF_INT_VECTORS];

/*
 * Initialize the interrupt subsystem.
 *
 * The GIC must be initialized before enabling device interrupts.
 * The vector base is moved to the image start address.
 */
void int_init(void)
{
	GIC_Init();
	system_irqtable_init();
	__set_VBAR((uint32_t)0x87800000);
}

/*
 * Initialize the IRQ handler table with the default handler.
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
 * Dispatch an IRQ from the assembly IRQ entry.
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
 * Default IRQ handler.
 *
 * This handler is used when no specific handler has been registered.
 */
void default_irqhandler(unsigned int giccIar, void *userParam)
{
	(void)giccIar;
	(void)userParam;

	while (1) {
	}
}