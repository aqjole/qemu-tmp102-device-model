#include "uart.h"

#define UART0_BASE 0x3f201000u
#define UART_DR    (*(volatile uint32_t *)(UART0_BASE + 0x00))
#define UART_FR    (*(volatile uint32_t *)(UART0_BASE + 0x18))
#define UART_CR    (*(volatile uint32_t *)(UART0_BASE + 0x30))
#define UART_TXFF  (1u << 5)
#define UART_CR_UARTEN (1u << 0)
#define UART_CR_TXE    (1u << 8)

void uart_init(void)
{
    UART_CR = UART_CR_UARTEN | UART_CR_TXE;
}

void uart_putc(char c)
{
    if (c == '\n') {
        uart_putc('\r');
    }

    while (UART_FR & UART_TXFF) {
    }

    UART_DR = (uint32_t)c;
}

void uart_puts(const char *s)
{
    while (*s) {
        uart_putc(*s++);
    }
}

void uart_put_hex16(uint16_t value)
{
    const char hex[] = "0123456789abcdef";

    uart_puts("0x");
    for (int shift = 12; shift >= 0; shift -= 4) {
        uart_putc(hex[(value >> shift) & 0xf]);
    }
}

void uart_put_int(int value)
{
    char buf[12];
    int i = 0;
    unsigned int n;

    if (value < 0) {
        uart_putc('-');
        n = (unsigned int)(-value);
    } else {
        n = (unsigned int)value;
    }

    if (n == 0) {
        uart_putc('0');
        return;
    }

    while (n > 0) {
        buf[i++] = (char)('0' + (n % 10));
        n /= 10;
    }

    while (i > 0) {
        uart_putc(buf[--i]);
    }
}
