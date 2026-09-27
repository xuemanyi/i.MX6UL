#ifndef BSP_UART_H
#define BSP_UART_H

#include "imx6ul.h"

/**
 * uart_init() - 初始化 UART1，默认波特率为 115200
 */
void uart_init(void);

/**
 * uart_io_init() - 配置 UART1 收发引脚复用和 PAD
 */
void uart_io_init(void);

/**
 * uart_disable() - 禁止 UART 控制器
 * @base: UART 控制器基地址
 */
void uart_disable(UART_Type *base);

/**
 * uart_enable() - 使能 UART 控制器
 * @base: UART 控制器基地址
 */
void uart_enable(UART_Type *base);

/**
 * uart_softreset() - 软件复位 UART 控制器
 * @base: UART 控制器基地址
 */
void uart_softreset(UART_Type *base);

/**
 * uart_setbaudrate() - 配置 UART 波特率
 * @base: UART 控制器基地址
 * @baudrate: 目标波特率
 * @srcclock_hz: UART 模块输入时钟频率
 */
void uart_setbaudrate(UART_Type *base,
		      unsigned int baudrate,
		      unsigned int srcclock_hz);
/**
 * putc() - 发送一个字符
 * @c: 待发送字符
 */
void putc(unsigned char c);

/**
 * puts() - 发送以 NUL 结尾的字符串
 * @str: 待发送字符串，不允许为 NULL
 */
void puts(const char *str);

/**
 * getc() - 阻塞接收一个字符
 *
 * Return: 接收到的字符。
 */
unsigned char getc(void);

/**
 * raise() - 提供精简 C 库所需的异常占位接口
 * @sig_nr: 信号编号
 */
void raise(int sig_nr);

#endif /* BSP_UART_H */
