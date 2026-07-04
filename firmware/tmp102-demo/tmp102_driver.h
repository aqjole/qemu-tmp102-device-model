#ifndef TMP102_DRIVER_H
#define TMP102_DRIVER_H

#include <stdint.h>

#define TMP102_ADDR 0x48

#define TMP102_REG_TEMPERATURE 0x00
#define TMP102_REG_CONFIG      0x01
#define TMP102_REG_T_LOW       0x02
#define TMP102_REG_T_HIGH      0x03

#define TMP102_READ_ERROR 0xffff

uint16_t tmp102_read_raw(void);
int tmp102_decode_celsius(uint16_t raw);

#endif
