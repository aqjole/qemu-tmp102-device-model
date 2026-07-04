#include "tmp102_driver.h"

#ifdef TMP102_DEMO_BAREMETAL
#include "i2c.h"
#endif

uint16_t tmp102_read_raw(void)
{
#ifdef TMP102_DEMO_BAREMETAL
    uint8_t reg = TMP102_REG_TEMPERATURE;
    uint8_t data[2];

    if (i2c_write_read(TMP102_ADDR, &reg, 1, data, 2) < 0) {
        return TMP102_READ_ERROR;
    }

    return ((uint16_t)data[0] << 8) | data[1];
#else
    return 0x1900;
#endif
}

int tmp102_decode_celsius(uint16_t raw)
{
    int16_t value = (int16_t)raw;

    value >>= 4;

    return value / 16;
}
