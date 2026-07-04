#ifndef UART_H
#define UART_H

#include <stdint.h>

void uart_putc(char c);
void uart_puts(const char *s);
void uart_put_hex16(uint16_t value);
void uart_put_int(int value);

#endif
