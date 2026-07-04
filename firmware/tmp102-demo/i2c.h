#ifndef I2C_H
#define I2C_H

#include <stdint.h>

int i2c_write(uint8_t addr, const uint8_t *data, uint32_t len);
int i2c_read(uint8_t addr, uint8_t *data, uint32_t len);
int i2c_write_read(uint8_t addr, const uint8_t *tx, uint32_t tx_len,
                   uint8_t *rx, uint32_t rx_len);

#endif
