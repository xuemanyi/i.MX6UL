#include "bsp_uart.h"

/*
 * Initialize UART1 as 115200-8N1.
 */
void uart_init(void)
{
	uart_io_init();

	uart_disable(UART1);
	uart_softreset(UART1);

	UART1->UCR1 = 0;
	UART1->UCR1 &= ~(1U << 14);

	/*
	 * Configure UART frame format.
	 *
	 * IRTS : ignore RTS pin
	 * PREN : parity disabled
	 * STPB : one stop bit
	 * WS   : 8-bit data
	 * TXEN : transmitter enabled
	 * RXEN : receiver enabled
	 */
	UART1->UCR2 |= (1U << 14) |
		       (1U << 5) |
		       (1U << 2) |
		       (1U << 1);

	/*
	 * RXDMUXSEL must be set on i.MX6UL/i.MX6ULL.
	 */
	UART1->UCR3 |= 1U << 2;

	/*
	 * Configure baud rate to 115200.
	 *
	 * Baud = RefFreq / (16 * (UBMR + 1) / (UBIR + 1))
	 *
	 * RefFreq = 80 MHz
	 * UBIR    = 71
	 * UBMR    = 3124
	 */
	UART1->UFCR = 5U << 7;
	UART1->UBIR = 71;
	UART1->UBMR = 3124;

	uart_enable(UART1);
}

/*
 * Initialize UART1 TX/RX pads.
 */
void uart_io_init(void)
{
	IOMUXC_SetPinMux(IOMUXC_UART1_TX_DATA_UART1_TX, 0);
	IOMUXC_SetPinMux(IOMUXC_UART1_RX_DATA_UART1_RX, 0);

	IOMUXC_SetPinConfig(IOMUXC_UART1_TX_DATA_UART1_TX, 0x10b0);
	IOMUXC_SetPinConfig(IOMUXC_UART1_RX_DATA_UART1_RX, 0x10b0);
}

/*
 * Configure UART baud rate.
 */
void uart_setbaudrate(UART_Type *base,
		      unsigned int baudrate,
		      unsigned int srcclock_hz)
{
	uint32_t numerator;
	uint32_t denominator;
	uint32_t divisor;
	uint32_t refFreqDiv;
	uint32_t divider;
	uint64_t baudDiff;
	uint64_t tempNumerator;
	uint32_t tempDenominator;

	numerator = srcclock_hz;
	denominator = baudrate << 4;
	divisor = 1;

	while (denominator != 0) {
		divisor = denominator;
		denominator = numerator % denominator;
		numerator = divisor;
	}

	numerator = srcclock_hz / divisor;
	denominator = (baudrate << 4) / divisor;

	if ((numerator > (UART_UBIR_INC_MASK * 7)) ||
	    (denominator > UART_UBIR_INC_MASK)) {
		uint32_t m;
		uint32_t n;
		uint32_t max;

		m = (numerator - 1) / (UART_UBIR_INC_MASK * 7) + 1;
		n = (denominator - 1) / UART_UBIR_INC_MASK + 1;
		max = (m > n) ? m : n;

		numerator /= max;
		denominator /= max;

		if (numerator == 0) {
			numerator = 1;
		}

		if (denominator == 0) {
			denominator = 1;
		}
	}

	divider = (numerator - 1) / UART_UBIR_INC_MASK + 1;

	switch (divider) {
	case 1:
		refFreqDiv = 0x05;
		break;
	case 2:
		refFreqDiv = 0x04;
		break;
	case 3:
		refFreqDiv = 0x03;
		break;
	case 4:
		refFreqDiv = 0x02;
		break;
	case 5:
		refFreqDiv = 0x01;
		break;
	case 6:
		refFreqDiv = 0x00;
		break;
	case 7:
		refFreqDiv = 0x06;
		break;
	default:
		refFreqDiv = 0x05;
		break;
	}

	tempNumerator = srcclock_hz;
	tempDenominator = numerator << 4;
	divisor = 1;

	while (tempDenominator != 0) {
		divisor = tempDenominator;
		tempDenominator = tempNumerator % tempDenominator;
		tempNumerator = divisor;
	}

	tempNumerator = srcclock_hz / divisor;
	tempDenominator = (numerator << 4) / divisor;

	baudDiff = (tempNumerator * denominator) / tempDenominator;
	baudDiff = (baudDiff >= baudrate) ?
		   (baudDiff - baudrate) :
		   (baudrate - baudDiff);

	if (baudDiff < (baudrate / 100) * 3) {
		base->UFCR &= ~UART_UFCR_RFDIV_MASK;
		base->UFCR |= UART_UFCR_RFDIV(refFreqDiv);
		base->UBIR = UART_UBIR_INC(denominator - 1);
		base->UBMR = UART_UBMR_MOD(numerator / divider - 1);
	}
}

/*
 * Disable UART.
 */
void uart_disable(UART_Type *base)
{
	base->UCR1 &= ~(1U << 0);
}

/*
 * Enable UART.
 */
void uart_enable(UART_Type *base)
{
	base->UCR1 |= 1U << 0;
}

/*
 * Reset UART controller.
 */
void uart_softreset(UART_Type *base)
{
	base->UCR2 &= ~(1U << 0);

	while ((base->UCR2 & 0x1) == 0) {
	}
}

/*
 * Send one character through UART1.
 */
void putc(unsigned char c)
{
	while (((UART1->USR2 >> 3) & 0x1) == 0) {
	}

	UART1->UTXD = c & 0xff;
}

/*
 * Send a string through UART1.
 */
void puts(const char *str)
{
	const char *p = str;

	while (*p) {
		putc(*p++);
	}
}

/*
 * Receive one character from UART1.
 */
unsigned char getc(void)
{
	while ((UART1->USR2 & 0x1) == 0) {
	}

	return UART1->URXD & 0xff;
}

/*
 * Stub required by the bare-metal toolchain.
 */
void raise(int sig_nr)
{
	(void)sig_nr;
}