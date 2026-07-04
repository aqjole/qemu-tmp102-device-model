/*
 * QTest testcase for the TMP102 temperature sensor
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"

#include "libqtest-single.h"
#include "libqos/qgraph.h"
#include "libqos/i2c.h"
#include "qobject/qdict.h"
#include "hw/sensor/tmp102_regs.h"

#define TMP102_TEST_ID   "tmp102-test"
#define TMP102_TEST_ADDR 0x48

static int qmp_tmp102_get_temperature(const char *id)
{
    QDict *response;
    int ret;

    response = qmp("{ 'execute': 'qom-get', 'arguments': { 'path': %s, "
                   "'property': 'temperature' } }", id);
    g_assert(qdict_haskey(response, "return"));
    ret = qdict_get_int(response, "return");
    qobject_unref(response);
    return ret;
}

static void qmp_tmp102_set_temperature(const char *id, int value)
{
    QDict *response;

    response = qmp("{ 'execute': 'qom-set', 'arguments': { 'path': %s, "
                   "'property': 'temperature', 'value': %d } }", id, value);
    g_assert(qdict_haskey(response, "return"));
    qobject_unref(response);
}

static void register_io(void *obj, void *data, QGuestAllocator *alloc)
{
    QI2CDevice *i2cdev = (QI2CDevice *)obj;

    g_assert_cmphex(i2c_get16(i2cdev, TMP102_REG_TEMPERATURE), ==, 0x0000);
    g_assert_cmphex(i2c_get16(i2cdev, TMP102_REG_CONFIG), ==, 0x60a0);
    g_assert_cmphex(i2c_get16(i2cdev, TMP102_REG_T_LOW), ==, 0x4b00);
    g_assert_cmphex(i2c_get16(i2cdev, TMP102_REG_T_HIGH), ==, 0x5000);

    i2c_set16(i2cdev, TMP102_REG_T_LOW, 0x1230);
    g_assert_cmphex(i2c_get16(i2cdev, TMP102_REG_T_LOW), ==, 0x1230);

    i2c_set16(i2cdev, TMP102_REG_T_HIGH, 0x4560);
    g_assert_cmphex(i2c_get16(i2cdev, TMP102_REG_T_HIGH), ==, 0x4560);

    i2c_set16(i2cdev, TMP102_REG_TEMPERATURE, 0x7ff0);
    g_assert_cmphex(i2c_get16(i2cdev, TMP102_REG_TEMPERATURE), ==, 0x0000);

    i2c_set16(i2cdev, TMP102_REG_CONFIG, 0x0000);
    g_assert_cmphex(i2c_get16(i2cdev, TMP102_REG_CONFIG), ==, 0x6020);

    i2c_set16(i2cdev, TMP102_REG_CONFIG, 0xffff);
    g_assert_cmphex(i2c_get16(i2cdev, TMP102_REG_CONFIG), ==, 0xffd0);
}

static void temperature(void *obj, void *data, QGuestAllocator *alloc)
{
    QI2CDevice *i2cdev = (QI2CDevice *)obj;

    i2c_set16(i2cdev, TMP102_REG_CONFIG, TMP102_CONFIG_RESET);

    g_assert_cmpint(qmp_tmp102_get_temperature(TMP102_TEST_ID), ==, 0);
    g_assert_cmphex(i2c_get16(i2cdev, TMP102_REG_TEMPERATURE), ==, 0x0000);

    qmp_tmp102_set_temperature(TMP102_TEST_ID, 20000);
    g_assert_cmpint(qmp_tmp102_get_temperature(TMP102_TEST_ID), ==, 20000);
    g_assert_cmphex(i2c_get16(i2cdev, TMP102_REG_TEMPERATURE), ==, 0x1400);

    qmp_tmp102_set_temperature(TMP102_TEST_ID, 25000);
    g_assert_cmpint(qmp_tmp102_get_temperature(TMP102_TEST_ID), ==, 25000);
    g_assert_cmphex(i2c_get16(i2cdev, TMP102_REG_TEMPERATURE), ==, 0x1900);

    qmp_tmp102_set_temperature(TMP102_TEST_ID, -25000);
    g_assert_cmpint(qmp_tmp102_get_temperature(TMP102_TEST_ID), ==, -25000);
    g_assert_cmphex(i2c_get16(i2cdev, TMP102_REG_TEMPERATURE), ==, 0xe700);

    i2c_set16(i2cdev, TMP102_REG_CONFIG,
              TMP102_CONFIG_RESET | TMP102_CONFIG_EM);
    g_assert_cmphex(i2c_get16(i2cdev, TMP102_REG_CONFIG), ==, 0x60b0);

    qmp_tmp102_set_temperature(TMP102_TEST_ID, 0);
    g_assert_cmpint(qmp_tmp102_get_temperature(TMP102_TEST_ID), ==, 0);
    g_assert_cmphex(i2c_get16(i2cdev, TMP102_REG_TEMPERATURE), ==, 0x0001);

    qmp_tmp102_set_temperature(TMP102_TEST_ID, 25000);
    g_assert_cmpint(qmp_tmp102_get_temperature(TMP102_TEST_ID), ==, 25000);
    g_assert_cmphex(i2c_get16(i2cdev, TMP102_REG_TEMPERATURE), ==, 0x0c81);

    qmp_tmp102_set_temperature(TMP102_TEST_ID, -25000);
    g_assert_cmpint(qmp_tmp102_get_temperature(TMP102_TEST_ID), ==, -25000);
    g_assert_cmphex(i2c_get16(i2cdev, TMP102_REG_TEMPERATURE), ==, 0xf381);
}

static void command_line_temperature(void *obj, void *data,
                                     QGuestAllocator *alloc)
{
    QI2CDevice *i2cdev = (QI2CDevice *)obj;

    g_assert_cmpint(qmp_tmp102_get_temperature(TMP102_TEST_ID), ==, 25000);
    g_assert_cmphex(i2c_get16(i2cdev, TMP102_REG_TEMPERATURE), ==, 0x1900);
}

static void alert(void *obj, void *data, QGuestAllocator *alloc)
{
    QI2CDevice *i2cdev = (QI2CDevice *)obj;

    qtest_irq_intercept_out(global_qtest, TMP102_TEST_ID);

    i2c_set16(i2cdev, TMP102_REG_CONFIG, TMP102_CONFIG_RESET);
    i2c_set16(i2cdev, TMP102_REG_T_LOW, TMP102_T_LOW_RESET);
    i2c_set16(i2cdev, TMP102_REG_T_HIGH, TMP102_T_HIGH_RESET);

    qmp_tmp102_set_temperature(TMP102_TEST_ID, 25000);
    g_assert_true(get_irq(0));
    g_assert_cmphex(i2c_get16(i2cdev, TMP102_REG_CONFIG), ==, 0x60a0);

    qmp_tmp102_set_temperature(TMP102_TEST_ID, 80000);
    g_assert_false(get_irq(0));
    g_assert_cmphex(i2c_get16(i2cdev, TMP102_REG_CONFIG), ==, 0x6080);

    qmp_tmp102_set_temperature(TMP102_TEST_ID, 78000);
    g_assert_false(get_irq(0));
    g_assert_cmphex(i2c_get16(i2cdev, TMP102_REG_CONFIG), ==, 0x6080);

    qmp_tmp102_set_temperature(TMP102_TEST_ID, 74000);
    g_assert_true(get_irq(0));
    g_assert_cmphex(i2c_get16(i2cdev, TMP102_REG_CONFIG), ==, 0x60a0);

    i2c_set16(i2cdev, TMP102_REG_CONFIG,
              TMP102_CONFIG_RESET | TMP102_CONFIG_POL);
    g_assert_false(get_irq(0));
    g_assert_cmphex(i2c_get16(i2cdev, TMP102_REG_CONFIG), ==, 0x6480);

    qmp_tmp102_set_temperature(TMP102_TEST_ID, 80000);
    g_assert_true(get_irq(0));
    g_assert_cmphex(i2c_get16(i2cdev, TMP102_REG_CONFIG), ==, 0x64a0);

    qmp_tmp102_set_temperature(TMP102_TEST_ID, 74000);
    g_assert_false(get_irq(0));
    g_assert_cmphex(i2c_get16(i2cdev, TMP102_REG_CONFIG), ==, 0x6480);

    i2c_set16(i2cdev, TMP102_REG_CONFIG,
              TMP102_CONFIG_RESET | TMP102_CONFIG_FAULTS_2);
    g_assert_true(get_irq(0));

    qmp_tmp102_set_temperature(TMP102_TEST_ID, 80000);
    g_assert_true(get_irq(0));

    qmp_tmp102_set_temperature(TMP102_TEST_ID, 80000);
    g_assert_false(get_irq(0));

    qmp_tmp102_set_temperature(TMP102_TEST_ID, 74000);
    g_assert_false(get_irq(0));

    qmp_tmp102_set_temperature(TMP102_TEST_ID, 74000);
    g_assert_true(get_irq(0));
}

static void interrupt(void *obj, void *data, QGuestAllocator *alloc)
{
    QI2CDevice *i2cdev = (QI2CDevice *)obj;

    qtest_irq_intercept_out(global_qtest, TMP102_TEST_ID);

    i2c_set16(i2cdev, TMP102_REG_CONFIG,
              TMP102_CONFIG_RESET | TMP102_CONFIG_TM);
    i2c_set16(i2cdev, TMP102_REG_T_LOW, TMP102_T_LOW_RESET);
    i2c_set16(i2cdev, TMP102_REG_T_HIGH, TMP102_T_HIGH_RESET);

    qmp_tmp102_set_temperature(TMP102_TEST_ID, 25000);
    g_assert_true(get_irq(0));

    qmp_tmp102_set_temperature(TMP102_TEST_ID, 80000);
    g_assert_false(get_irq(0));

    g_assert_cmphex(i2c_get16(i2cdev, TMP102_REG_TEMPERATURE), ==, 0x5000);
    g_assert_true(get_irq(0));

    qmp_tmp102_set_temperature(TMP102_TEST_ID, 80000);
    g_assert_true(get_irq(0));

    qmp_tmp102_set_temperature(TMP102_TEST_ID, 74000);
    g_assert_false(get_irq(0));

    g_assert_cmphex(i2c_get16(i2cdev, TMP102_REG_TEMPERATURE), ==, 0x4a00);
    g_assert_true(get_irq(0));

    qmp_tmp102_set_temperature(TMP102_TEST_ID, 74000);
    g_assert_true(get_irq(0));

    qmp_tmp102_set_temperature(TMP102_TEST_ID, 80000);
    g_assert_false(get_irq(0));
}

static void shutdown_mode(void *obj, void *data, QGuestAllocator *alloc)
{
    QI2CDevice *i2cdev = (QI2CDevice *)obj;

    qtest_irq_intercept_out(global_qtest, TMP102_TEST_ID);

    i2c_set16(i2cdev, TMP102_REG_CONFIG, TMP102_CONFIG_RESET);
    i2c_set16(i2cdev, TMP102_REG_T_LOW, TMP102_T_LOW_RESET);
    i2c_set16(i2cdev, TMP102_REG_T_HIGH, TMP102_T_HIGH_RESET);

    qmp_tmp102_set_temperature(TMP102_TEST_ID, 25000);
    g_assert_true(get_irq(0));

    i2c_set16(i2cdev, TMP102_REG_CONFIG,
              TMP102_CONFIG_RESET | TMP102_CONFIG_SD);
    g_assert_cmphex(i2c_get16(i2cdev, TMP102_REG_CONFIG), ==, 0x61a0);

    qmp_tmp102_set_temperature(TMP102_TEST_ID, 80000);
    g_assert_cmphex(i2c_get16(i2cdev, TMP102_REG_TEMPERATURE), ==, 0x5000);
    g_assert_true(get_irq(0));

    i2c_set16(i2cdev, TMP102_REG_CONFIG,
              TMP102_CONFIG_RESET | TMP102_CONFIG_SD | TMP102_CONFIG_OS);
    g_assert_false(get_irq(0));
    g_assert_cmphex(i2c_get16(i2cdev, TMP102_REG_CONFIG), ==, 0xe180);

    qmp_tmp102_set_temperature(TMP102_TEST_ID, 74000);
    g_assert_false(get_irq(0));

    i2c_set16(i2cdev, TMP102_REG_CONFIG,
              TMP102_CONFIG_RESET | TMP102_CONFIG_SD | TMP102_CONFIG_OS);
    g_assert_true(get_irq(0));
    g_assert_cmphex(i2c_get16(i2cdev, TMP102_REG_CONFIG), ==, 0xe1a0);

    i2c_set16(i2cdev, TMP102_REG_CONFIG, TMP102_CONFIG_RESET);
    g_assert_true(get_irq(0));

    qmp_tmp102_set_temperature(TMP102_TEST_ID, 80000);
    g_assert_false(get_irq(0));

    i2c_set16(i2cdev, TMP102_REG_CONFIG,
              TMP102_CONFIG_RESET | TMP102_CONFIG_TM);
    qmp_tmp102_set_temperature(TMP102_TEST_ID, 80000);
    g_assert_false(get_irq(0));

    i2c_set16(i2cdev, TMP102_REG_CONFIG,
              TMP102_CONFIG_RESET | TMP102_CONFIG_TM | TMP102_CONFIG_SD);
    g_assert_true(get_irq(0));
}

static void tmp102_register_nodes(void)
{
    QOSGraphEdgeOptions opts = {
        .extra_device_opts = "id=" TMP102_TEST_ID ",address=0x48"
    };
    QOSGraphTestOptions command_line_temperature_opts = {
        .edge.extra_device_opts = "temperature=25000"
    };

    add_qi2c_address(&opts, &(QI2CAddress) { TMP102_TEST_ADDR });

    qos_node_create_driver("tmp102", i2c_device_create);
    qos_node_consumes("tmp102", "i2c-bus", &opts);

    qos_add_test("register-io", "tmp102", register_io, NULL);
    qos_add_test("temperature", "tmp102", temperature, NULL);
    qos_add_test("command-line-temperature", "tmp102",
                 command_line_temperature, &command_line_temperature_opts);
    qos_add_test("alert", "tmp102", alert, NULL);
    qos_add_test("interrupt", "tmp102", interrupt, NULL);
    qos_add_test("shutdown", "tmp102", shutdown_mode, NULL);
}

libqos_init(tmp102_register_nodes);
