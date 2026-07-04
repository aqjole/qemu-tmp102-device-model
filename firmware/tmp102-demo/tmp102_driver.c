#include "tmp102_driver.h"

uint16_t tmp102_read_raw(void)
{
    return 0x1900;
}

int tmp102_decode_celsius(uint16_t raw)
{
    int16_t value = (int16_t)raw;

    value >>= 4;

    return value / 16;
}