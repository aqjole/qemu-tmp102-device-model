/*
 * QTest migration testcase for the TMP102 temperature sensor
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"

#include "hw/i2c/bcm2835_i2c.h"
#include "hw/sensor/tmp102_regs.h"
#include "libqtest.h"
#include "migration/migration-qmp.h"
#include "qobject/qdict.h"

#define TMP102_TEST_ID   "tmp102-migration-test"
#define TMP102_TEST_ADDR 0x48
#define BCM2835_I2C1_BASE 0x3f804000

static void bcm2835_i2c_init_transfer(QTestState *qts, uint32_t base,
                                      bool read)
{
    int interrupt = read ? BCM2835_I2C_C_INTR : BCM2835_I2C_C_INTT;

    qtest_writel(qts, base + BCM2835_I2C_C,
                 BCM2835_I2C_C_I2CEN | BCM2835_I2C_C_INTD |
                 BCM2835_I2C_C_ST | BCM2835_I2C_C_CLEAR | interrupt | read);
}

static void bcm2835_i2c_clear_status(QTestState *qts, uint32_t base)
{
    qtest_writel(qts, base + BCM2835_I2C_S,
                 BCM2835_I2C_S_DONE | BCM2835_I2C_S_ERR |
                 BCM2835_I2C_S_CLKT);
}

static void tmp102_i2c_set16(QTestState *qts, uint8_t reg, uint16_t value)
{
    qtest_writel(qts, BCM2835_I2C1_BASE + BCM2835_I2C_A, TMP102_TEST_ADDR);
    qtest_writel(qts, BCM2835_I2C1_BASE + BCM2835_I2C_DLEN, 3);

    bcm2835_i2c_init_transfer(qts, BCM2835_I2C1_BASE, false);

    qtest_writel(qts, BCM2835_I2C1_BASE + BCM2835_I2C_FIFO, reg);
    qtest_writel(qts, BCM2835_I2C1_BASE + BCM2835_I2C_FIFO, value >> 8);
    qtest_writel(qts, BCM2835_I2C1_BASE + BCM2835_I2C_FIFO, value & 0xff);

    bcm2835_i2c_clear_status(qts, BCM2835_I2C1_BASE);
}

static uint16_t tmp102_i2c_get16(QTestState *qts, uint8_t reg)
{
    uint8_t hi;
    uint8_t lo;

    qtest_writel(qts, BCM2835_I2C1_BASE + BCM2835_I2C_A, TMP102_TEST_ADDR);
    qtest_writel(qts, BCM2835_I2C1_BASE + BCM2835_I2C_DLEN, 1);

    bcm2835_i2c_init_transfer(qts, BCM2835_I2C1_BASE, false);

    qtest_writel(qts, BCM2835_I2C1_BASE + BCM2835_I2C_FIFO, reg);

    qtest_writel(qts, BCM2835_I2C1_BASE + BCM2835_I2C_DLEN, 2);
    bcm2835_i2c_init_transfer(qts, BCM2835_I2C1_BASE, true);

    hi = qtest_readl(qts, BCM2835_I2C1_BASE + BCM2835_I2C_FIFO);
    lo = qtest_readl(qts, BCM2835_I2C1_BASE + BCM2835_I2C_FIFO);

    bcm2835_i2c_clear_status(qts, BCM2835_I2C1_BASE);

    return (hi << 8) | lo;
}

static int qmp_tmp102_get_temperature(QTestState *qts, const char *id)
{
    QDict *response;
    int ret;

    response = qtest_qmp(qts,
                         "{ 'execute': 'qom-get', 'arguments': { "
                         "'path': %s, 'property': 'temperature' } }", id);
    g_assert(qdict_haskey(response, "return"));
    ret = qdict_get_int(response, "return");
    qobject_unref(response);
    return ret;
}

static void qmp_tmp102_set_temperature(QTestState *qts, const char *id,
                                       int value)
{
    QDict *response;

    response = qtest_qmp(qts,
                         "{ 'execute': 'qom-set', 'arguments': { "
                         "'path': %s, 'property': 'temperature', "
                         "'value': %d } }", id, value);
    g_assert(qdict_haskey(response, "return"));
    qobject_unref(response);
}

static char *tmp102_qemu_args(int temperature, const char *extra)
{
    return g_strdup_printf("-M raspi2b -display none "
                           "-device tmp102,id=" TMP102_TEST_ID
                           ",address=0x48,bus=i2c-bus.1,temperature=%d %s",
                           temperature, extra ? extra : "");
}

static void test_tmp102_migrate(void)
{
    g_autofree char *src_args = tmp102_qemu_args(25000, "");
    g_autofree char *dst_args = tmp102_qemu_args(0, "-incoming defer");
    QTestState *src;
    QTestState *dst;
    uint16_t config;
    uint16_t t_low;
    uint16_t t_high;

    src = qtest_init(src_args);
    dst = qtest_init(dst_args);

    tmp102_i2c_set16(src, TMP102_REG_CONFIG,
                     TMP102_CONFIG_RESET | TMP102_CONFIG_TM |
                     TMP102_CONFIG_POL | TMP102_CONFIG_FAULTS_4);
    tmp102_i2c_set16(src, TMP102_REG_T_LOW, 0x1230);
    tmp102_i2c_set16(src, TMP102_REG_T_HIGH, 0x4560);
    qmp_tmp102_set_temperature(src, TMP102_TEST_ID, 80000);

    config = tmp102_i2c_get16(src, TMP102_REG_CONFIG);
    t_low = tmp102_i2c_get16(src, TMP102_REG_T_LOW);
    t_high = tmp102_i2c_get16(src, TMP102_REG_T_HIGH);

    qtest_qom_set_bool(src, TMP102_TEST_ID, "inject-corrupt-data", true);
    qtest_qom_set_bool(src, TMP102_TEST_ID, "stuck-alert", true);
    qtest_qom_set_bool(src, TMP102_TEST_ID, "inject-nack", true);

    g_assert_cmpint(qmp_tmp102_get_temperature(dst, TMP102_TEST_ID), ==, 0);

    migrate_incoming_qmp(dst, "tcp:127.0.0.1:0", NULL, "{}");
    migrate_ensure_converge(src);
    migrate_qmp(src, dst, NULL, NULL, "{}");
    wait_for_migration_complete(src);
    wait_for_migration_complete(dst);

    g_assert_cmpint(qmp_tmp102_get_temperature(dst, TMP102_TEST_ID), ==, 80000);
    g_assert_true(qtest_qom_get_bool(dst, TMP102_TEST_ID,
                                     "inject-corrupt-data"));
    g_assert_true(qtest_qom_get_bool(dst, TMP102_TEST_ID, "stuck-alert"));
    g_assert_true(qtest_qom_get_bool(dst, TMP102_TEST_ID, "inject-nack"));

    qtest_qom_set_bool(dst, TMP102_TEST_ID, "inject-corrupt-data", false);
    qtest_qom_set_bool(dst, TMP102_TEST_ID, "inject-nack", false);

    g_assert_cmphex(tmp102_i2c_get16(dst, TMP102_REG_CONFIG), ==, config);
    g_assert_cmphex(tmp102_i2c_get16(dst, TMP102_REG_T_LOW), ==, t_low);
    g_assert_cmphex(tmp102_i2c_get16(dst, TMP102_REG_T_HIGH), ==, t_high);
    g_assert_cmphex(tmp102_i2c_get16(dst, TMP102_REG_TEMPERATURE), ==, 0x5000);

    qtest_quit(dst);
    qtest_quit(src);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    qtest_add_func("/tmp102/migration/state", test_tmp102_migrate);
    return g_test_run();
}
