#include "i2c.h"

#define BSC1_BASE 0x3f804000u

#define BSC_C     (*(volatile uint32_t *)(BSC1_BASE + 0x00))
#define BSC_S     (*(volatile uint32_t *)(BSC1_BASE + 0x04))
#define BSC_DLEN  (*(volatile uint32_t *)(BSC1_BASE + 0x08))
#define BSC_A     (*(volatile uint32_t *)(BSC1_BASE + 0x0c))
#define BSC_FIFO  (*(volatile uint32_t *)(BSC1_BASE + 0x10))

#define BSC_C_I2CEN  (1u << 15)
#define BSC_C_ST     (1u << 7)
#define BSC_C_CLEAR  ((1u << 5) | (1u << 4))
#define BSC_C_READ   (1u << 0)

#define BSC_S_CLKT   (1u << 9)
#define BSC_S_ERR    (1u << 8)
#define BSC_S_TXD    (1u << 4)
#define BSC_S_RXD    (1u << 5)
#define BSC_S_DONE   (1u << 1)

#define I2C_TIMEOUT 1000000u

static void i2c_clear_status(void)
{
    BSC_S = BSC_S_DONE | BSC_S_ERR | BSC_S_CLKT;
}

static int i2c_wait(uint32_t mask)
{
    for (uint32_t i = 0; i < I2C_TIMEOUT; i++) {
        uint32_t status = BSC_S;

        if (status & (BSC_S_ERR | BSC_S_CLKT)) {
            return -1;
        }

        if (status & mask) {
            return 0;
        }
    }

    return -1;
}

int i2c_write(uint8_t addr, const uint8_t *data, uint32_t len)
{
    i2c_clear_status();

    BSC_A = addr;
    BSC_DLEN = len;
    BSC_C = BSC_C_I2CEN | BSC_C_CLEAR | BSC_C_ST;

    for (uint32_t i = 0; i < len; i++) {
        if (i2c_wait(BSC_S_TXD) < 0) {
            return -1;
        }

        BSC_FIFO = data[i];
    }

    if (i2c_wait(BSC_S_DONE) < 0) {
        return -1;
    }

    i2c_clear_status();
    return 0;
}

int i2c_read(uint8_t addr, uint8_t *data, uint32_t len)
{
    i2c_clear_status();

    BSC_A = addr;
    BSC_DLEN = len;
    BSC_C = BSC_C_I2CEN | BSC_C_CLEAR | BSC_C_ST | BSC_C_READ;

    for (uint32_t i = 0; i < len; i++) {
        if (i2c_wait(BSC_S_RXD) < 0) {
            return -1;
        }

        data[i] = (uint8_t)BSC_FIFO;
    }

    if (i2c_wait(BSC_S_DONE) < 0) {
        return -1;
    }

    i2c_clear_status();
    return 0;
}

int i2c_write_read(uint8_t addr, const uint8_t *tx, uint32_t tx_len,
                   uint8_t *rx, uint32_t rx_len)
{
    if (i2c_write(addr, tx, tx_len) < 0) {
        return -1;
    }

    return i2c_read(addr, rx, rx_len);
}
