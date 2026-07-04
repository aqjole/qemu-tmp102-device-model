/*
 * QTest fault-injection testcase for the TMP102 temperature sensor
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"

#include "hw/i2c/bcm2835_i2c.h"
#include "hw/sensor/tmp102_regs.h"
#include "libqtest-single.h"
#include "qobject/qdict.h"

#define TMP102_TEST_ID   "tmp102-fault-test"
#define TMP102_TEST_ADDR 0x48
#define BCM2835_I2C1_BASE 0x3f804000

static void bcm2835_i2c_init_transfer(uint32_t base, bool read)
{
    int interrupt = read ? BCM2835_I2C_C_INTR : BCM2835_I2C_C_INTT;

    writel(base + BCM2835_I2C_C,
           BCM2835_I2C_C_I2CEN | BCM2835_I2C_C_INTD |
           BCM2835_I2C_C_ST | BCM2835_I2C_C_CLEAR | interrupt | read);
}

static void bcm2835_i2c_clear_status(uint32_t base)
{
    writel(base + BCM2835_I2C_S,
           BCM2835_I2C_S_DONE | BCM2835_I2C_S_ERR | BCM2835_I2C_S_CLKT);
}

static void qmp_tmp102_set_temperature(const char *id, int value)
{
    QDict *response;

    response = qmp("{ 'execute': 'qom-set', 'arguments': { 'path': %s, "
                   "'property': 'temperature', 'value': %d } }", id, value);
    g_assert(qdict_haskey(response, "return"));
    qobject_unref(response);
}

static void tmp102_i2c_set16(uint8_t reg, uint16_t value)
{
    writel(BCM2835_I2C1_BASE + BCM2835_I2C_A, TMP102_TEST_ADDR);
    writel(BCM2835_I2C1_BASE + BCM2835_I2C_DLEN, 3);

    bcm2835_i2c_init_transfer(BCM2835_I2C1_BASE, false);

    writel(BCM2835_I2C1_BASE + BCM2835_I2C_FIFO, reg);
    writel(BCM2835_I2C1_BASE + BCM2835_I2C_FIFO, value >> 8);
    writel(BCM2835_I2C1_BASE + BCM2835_I2C_FIFO, value & 0xff);

    bcm2835_i2c_clear_status(BCM2835_I2C1_BASE);
}

static uint16_t tmp102_i2c_get16(uint8_t reg)
{
    uint8_t hi;
    uint8_t lo;

    writel(BCM2835_I2C1_BASE + BCM2835_I2C_A, TMP102_TEST_ADDR);
    writel(BCM2835_I2C1_BASE + BCM2835_I2C_DLEN, 1);

    bcm2835_i2c_init_transfer(BCM2835_I2C1_BASE, false);

    writel(BCM2835_I2C1_BASE + BCM2835_I2C_FIFO, reg);

    writel(BCM2835_I2C1_BASE + BCM2835_I2C_DLEN, 2);
    bcm2835_i2c_init_transfer(BCM2835_I2C1_BASE, true);

    hi = readl(BCM2835_I2C1_BASE + BCM2835_I2C_FIFO);
    lo = readl(BCM2835_I2C1_BASE + BCM2835_I2C_FIFO);

    bcm2835_i2c_clear_status(BCM2835_I2C1_BASE);

    return (hi << 8) | lo;
}

static uint32_t tmp102_i2c_start_status(bool read)
{
    uint32_t status;

    bcm2835_i2c_clear_status(BCM2835_I2C1_BASE);
    writel(BCM2835_I2C1_BASE + BCM2835_I2C_A, TMP102_TEST_ADDR);
    writel(BCM2835_I2C1_BASE + BCM2835_I2C_DLEN, 0);

    bcm2835_i2c_init_transfer(BCM2835_I2C1_BASE, read);
    status = readl(BCM2835_I2C1_BASE + BCM2835_I2C_S);
    bcm2835_i2c_clear_status(BCM2835_I2C1_BASE);

    return status;
}

static void test_corrupt_data(void)
{
    qtest_qom_set_bool(global_qtest, TMP102_TEST_ID, "inject-corrupt-data",
                       false);
    qmp_tmp102_set_temperature(TMP102_TEST_ID, 25000);

    g_assert_cmphex(tmp102_i2c_get16(TMP102_REG_TEMPERATURE), ==, 0x1900);

    qtest_qom_set_bool(global_qtest, TMP102_TEST_ID, "inject-corrupt-data",
                       true);
    g_assert_true(qtest_qom_get_bool(global_qtest, TMP102_TEST_ID,
                                     "inject-corrupt-data"));
    g_assert_cmphex(tmp102_i2c_get16(TMP102_REG_TEMPERATURE), ==, 0xffff);
    g_assert_cmphex(tmp102_i2c_get16(TMP102_REG_CONFIG), ==,
                    TMP102_CONFIG_RESET);

    qtest_qom_set_bool(global_qtest, TMP102_TEST_ID, "inject-corrupt-data",
                       false);
    g_assert_cmphex(tmp102_i2c_get16(TMP102_REG_TEMPERATURE), ==, 0x1900);
}

static void test_stuck_alert(void)
{
    qtest_irq_intercept_out(global_qtest, TMP102_TEST_ID);

    qtest_qom_set_bool(global_qtest, TMP102_TEST_ID, "stuck-alert", false);
    tmp102_i2c_set16(TMP102_REG_CONFIG, TMP102_CONFIG_RESET);
    tmp102_i2c_set16(TMP102_REG_T_LOW, TMP102_T_LOW_RESET);
    tmp102_i2c_set16(TMP102_REG_T_HIGH, TMP102_T_HIGH_RESET);

    qmp_tmp102_set_temperature(TMP102_TEST_ID, 25000);
    g_assert_true(get_irq(0));

    qtest_qom_set_bool(global_qtest, TMP102_TEST_ID, "stuck-alert", true);
    g_assert_true(qtest_qom_get_bool(global_qtest, TMP102_TEST_ID,
                                     "stuck-alert"));

    qmp_tmp102_set_temperature(TMP102_TEST_ID, 80000);
    g_assert_true(get_irq(0));

    qtest_qom_set_bool(global_qtest, TMP102_TEST_ID, "stuck-alert", false);
    g_assert_false(get_irq(0));
}

static void test_inject_nack(void)
{
    uint32_t status;

    qtest_qom_set_bool(global_qtest, TMP102_TEST_ID, "inject-nack", false);
    status = tmp102_i2c_start_status(false);
    g_assert_cmphex(status & BCM2835_I2C_S_ERR, ==, 0);

    qtest_qom_set_bool(global_qtest, TMP102_TEST_ID, "inject-nack", true);
    g_assert_true(qtest_qom_get_bool(global_qtest, TMP102_TEST_ID,
                                     "inject-nack"));

    status = tmp102_i2c_start_status(false);
    g_assert_cmphex(status & BCM2835_I2C_S_ERR, ==, BCM2835_I2C_S_ERR);

    status = tmp102_i2c_start_status(true);
    g_assert_cmphex(status & BCM2835_I2C_S_ERR, ==, BCM2835_I2C_S_ERR);

    qtest_qom_set_bool(global_qtest, TMP102_TEST_ID, "inject-nack", false);
}

int main(int argc, char **argv)
{
    int ret;

    g_test_init(&argc, &argv, NULL);

    qtest_add_func("/tmp102/fault/corrupt-data", test_corrupt_data);
    qtest_add_func("/tmp102/fault/stuck-alert", test_stuck_alert);
    qtest_add_func("/tmp102/fault/inject-nack", test_inject_nack);

    qtest_start("-M raspi2b -display none "
                "-device tmp102,id=" TMP102_TEST_ID
                ",address=0x48,bus=i2c-bus.1,temperature=25000");
    ret = g_test_run();
    qtest_end();

    return ret;
}
