#include "tmp102_driver.h"
#include "uart.h"

int main(void)
{
    uart_init();

    uint16_t raw = tmp102_read_raw();

    if (raw == TMP102_READ_ERROR) {
        uart_puts("TMP102 read failed\n");
        while (1) {
        }
    }

    int temp_c = tmp102_decode_celsius(raw);

    uart_puts("TMP102 raw: ");
    uart_put_hex16(raw);
    uart_puts("\n");

    uart_puts("Temperature: ");
    uart_put_int(temp_c);
    uart_puts(" C\n");

    while (1) {
    }
}
