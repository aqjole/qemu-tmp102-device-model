#include "qemu/osdep.h"
#include "hw/i2c/i2c.h"
#include "hw/core/irq.h"
#include "hw/sensor/tmp102.h"
#include "qapi/error.h"
#include "qapi/visitor.h"
#include "qemu/bitops.h"
#include "qemu/module.h"
#include "migration/vmstate.h"

static uint16_t tmp102_encode_temperature(TMP102State *s)
{
    if (s->config & TMP102_CONFIG_EM) {
        return (uint16_t)(((uint16_t)s->temperature <<
                           TMP102_TEMP_EXTENDED_SHIFT) |
                          TMP102_TEMP_EXTENDED_FLAG);
    }

    return (uint16_t)((uint16_t)s->temperature << TMP102_TEMP_NORMAL_SHIFT);
}

static int16_t tmp102_decode_limit(TMP102State *s, uint16_t value)
{
    if (s->config & TMP102_CONFIG_EM) {
        return sextract32(value, TMP102_TEMP_EXTENDED_SHIFT,
                          TMP102_TEMP_EXTENDED_BITS);
    }

    return sextract32(value, TMP102_TEMP_NORMAL_SHIFT,
                      TMP102_TEMP_NORMAL_BITS);
}

static bool tmp102_alert_level(TMP102State *s)
{
    return s->alert_asserted ^ !(s->config & TMP102_CONFIG_POL);
}

static uint8_t tmp102_fault_count(TMP102State *s)
{
    switch (s->config & TMP102_CONFIG_FAULT_QUEUE) {
    case TMP102_CONFIG_FAULTS_2:
        return 2;
    case TMP102_CONFIG_FAULTS_4:
        return 4;
    case TMP102_CONFIG_FAULTS_6:
        return 6;
    default:
        return 1;
    }
}

static void tmp102_faults_reset(TMP102State *s)
{
    s->high_faults = 0;
    s->low_faults = 0;
}

static void tmp102_alert_update(TMP102State *s)
{
    qemu_set_irq(s->alert, tmp102_alert_level(s));
}

static void tmp102_alert_sample(TMP102State *s, bool one_shot)
{
    uint8_t faults = tmp102_fault_count(s);

    if ((s->config & TMP102_CONFIG_SD) && !one_shot) {
        tmp102_faults_reset(s);
        tmp102_alert_update(s);
        return;
    }

    if (s->config & TMP102_CONFIG_TM) {
        if (s->alert_asserted) {
            tmp102_alert_update(s);
            return;
        }

        if (s->detect_falling) {
            s->high_faults = 0;

            if (s->temperature < tmp102_decode_limit(s, s->t_low)) {
                s->low_faults++;
                if (s->low_faults >= faults) {
                    s->alert_asserted = true;
                    s->detect_falling = false;
                    tmp102_faults_reset(s);
                }
            } else {
                s->low_faults = 0;
            }
        } else {
            s->low_faults = 0;

            if (s->temperature >= tmp102_decode_limit(s, s->t_high)) {
                s->high_faults++;
                if (s->high_faults >= faults) {
                    s->alert_asserted = true;
                    s->detect_falling = true;
                    tmp102_faults_reset(s);
                }
            } else {
                s->high_faults = 0;
            }
        }
    } else {
        if (s->alert_asserted) {
            s->high_faults = 0;

            if (s->temperature < tmp102_decode_limit(s, s->t_low)) {
                s->low_faults++;
                if (s->low_faults >= faults) {
                    s->alert_asserted = false;
                    tmp102_faults_reset(s);
                }
            } else {
                s->low_faults = 0;
            }
        } else {
            s->low_faults = 0;

            if (s->temperature >= tmp102_decode_limit(s, s->t_high)) {
                s->high_faults++;
                if (s->high_faults >= faults) {
                    s->alert_asserted = true;
                    tmp102_faults_reset(s);
                }
            } else {
                s->high_faults = 0;
            }
        }
    }

    tmp102_alert_update(s);
}

static void tmp102_interrupt_clear(TMP102State *s)
{
    if (s->config & TMP102_CONFIG_TM) {
        s->alert_asserted = false;
        tmp102_faults_reset(s);
        tmp102_alert_update(s);
    }
}

static uint16_t tmp102_config_read(TMP102State *s)
{
    if (tmp102_alert_level(s)) {
        return s->config | TMP102_CONFIG_AL;
    }

    return s->config & ~TMP102_CONFIG_AL;
}

static void tmp102_get_temperature(Object *obj, Visitor *v, const char *name,
                                   void *opaque, Error **errp)
{
    TMP102State *s = TMP102(obj);
    int64_t value = s->temperature * 1000 / 16;

    visit_type_int(v, name, &value, errp);
}

static void tmp102_set_temperature(Object *obj, Visitor *v, const char *name,
                                   void *opaque, Error **errp)
{
    TMP102State *s = TMP102(obj);
    int64_t temp;

    if (!visit_type_int(v, name, &temp, errp)) {
        return;
    }
    if (temp >= 128000 || temp < -128000) {
        error_setg(errp, "value %" PRId64 " milli-Celsius is out of range",
                   temp);
        return;
    }

    s->temperature = (int16_t)(temp * 16 / 1000);
    tmp102_alert_sample(s, false);
}

static void tmp102_read(TMP102State *s)
{
    uint16_t value;

    s->len = 0;
    tmp102_interrupt_clear(s);

    switch (s->pointer & TMP102_POINTER_MASK) {
    case TMP102_REG_TEMPERATURE:
        value = tmp102_encode_temperature(s);
        s->buf[s->len++] = value >> 8;
        s->buf[s->len++] = value >> 0;
        break;
    case TMP102_REG_CONFIG:
        value = tmp102_config_read(s);
        s->buf[s->len++] = value >> 8;
        s->buf[s->len++] = value >> 0;
        break;
    case TMP102_REG_T_LOW:
        s->buf[s->len++] = s->t_low >> 8;
        s->buf[s->len++] = s->t_low >> 0;
        break;
    case TMP102_REG_T_HIGH:
        s->buf[s->len++] = s->t_high >> 8;
        s->buf[s->len++] = s->t_high >> 0;
        break;
    }
}

static void tmp102_write(TMP102State *s)
{
    uint16_t value = ((uint16_t)s->buf[0] << 8) | s->buf[1];
    uint16_t old_config = s->config;
    bool reset_faults = false;
    bool one_shot = false;

    switch (s->pointer & TMP102_POINTER_MASK) {
    case TMP102_REG_TEMPERATURE:
        break;
    case TMP102_REG_CONFIG:
        s->config = (s->config & TMP102_CONFIG_READ_ONLY) |
                    (value & TMP102_CONFIG_WRITABLE);
        if ((s->config & TMP102_CONFIG_SD) &&
            (s->config & TMP102_CONFIG_TM) &&
            !(old_config & TMP102_CONFIG_SD)) {
            s->alert_asserted = false;
        }
        one_shot = (s->config & TMP102_CONFIG_SD) &&
                   (value & TMP102_CONFIG_OS);
        reset_faults = true;
        break;
    case TMP102_REG_T_LOW:
        s->t_low = value;
        reset_faults = true;
        break;
    case TMP102_REG_T_HIGH:
        s->t_high = value;
        reset_faults = true;
        break;
    }

    if (reset_faults) {
        tmp102_faults_reset(s);
    }
    if (one_shot) {
        tmp102_alert_sample(s, true);
    } else {
        tmp102_alert_update(s);
    }
}

static uint8_t tmp102_rx(I2CSlave *i2c)
{
    TMP102State *s = TMP102(i2c);

    if (s->len < 2) {
        return s->buf[s->len++];
    }

    return 0xff;
}

static int tmp102_tx(I2CSlave *i2c, uint8_t data)
{
    TMP102State *s = TMP102(i2c);

    if (s->len == 0) {
        s->pointer = data & TMP102_POINTER_MASK;
    } else {
        if (s->len <= 2) {
            s->buf[s->len - 1] = data;
        }

        if (s->len == 2) {
            tmp102_write(s);
        }
    }

    s->len++;
    return 0;
}

static int tmp102_event(I2CSlave *i2c, enum i2c_event event)
{
    TMP102State *s = TMP102(i2c);

    if (event == I2C_START_RECV) {
        tmp102_read(s);
    }

    s->len = 0;
    return 0;
}

static void tmp102_reset(I2CSlave *i2c)
{
    TMP102State *s = TMP102(i2c);

    s->pointer = TMP102_REG_TEMPERATURE;
    s->len = 0;
    s->config = TMP102_CONFIG_RESET;
    s->t_low = TMP102_T_LOW_RESET;
    s->t_high = TMP102_T_HIGH_RESET;
    s->alert_asserted = false;
    s->detect_falling = false;
    tmp102_faults_reset(s);
    tmp102_alert_update(s);
}

static int tmp102_post_load(void *opaque, int version_id)
{
    TMP102State *s = opaque;

    tmp102_alert_update(s);
    return 0;
}

static const VMStateDescription vmstate_tmp102 = {
    .name = "TMP102",
    .version_id = 1,
    .minimum_version_id = 1,
    .post_load = tmp102_post_load,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT8(len, TMP102State),
        VMSTATE_UINT8_ARRAY(buf, TMP102State, 2),
        VMSTATE_UINT8(pointer, TMP102State),
        VMSTATE_UINT16(config, TMP102State),
        VMSTATE_INT16(temperature, TMP102State),
        VMSTATE_UINT16(t_low, TMP102State),
        VMSTATE_UINT16(t_high, TMP102State),
        VMSTATE_BOOL(alert_asserted, TMP102State),
        VMSTATE_BOOL(detect_falling, TMP102State),
        VMSTATE_UINT8(high_faults, TMP102State),
        VMSTATE_UINT8(low_faults, TMP102State),
        VMSTATE_I2C_SLAVE(i2c, TMP102State),
        VMSTATE_END_OF_LIST()
    }
};

static void tmp102_realize(DeviceState *dev, Error **errp)
{
    I2CSlave *i2c = I2C_SLAVE(dev);
    TMP102State *s = TMP102(i2c);

    qdev_init_gpio_out(&i2c->qdev, &s->alert, 1);
    tmp102_reset(i2c);
}

static void tmp102_initfn(Object *obj)
{
    object_property_add(obj, "temperature", "int",
                        tmp102_get_temperature,
                        tmp102_set_temperature, NULL, NULL);
}

static void tmp102_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    I2CSlaveClass *k = I2C_SLAVE_CLASS(klass);

    dc->realize = tmp102_realize;
    dc->vmsd = &vmstate_tmp102;
    k->event = tmp102_event;
    k->recv = tmp102_rx;
    k->send = tmp102_tx;
}

static const TypeInfo tmp102_info = {
    .name          = TYPE_TMP102,
    .parent        = TYPE_I2C_SLAVE,
    .instance_size = sizeof(TMP102State),
    .instance_init = tmp102_initfn,
    .class_init    = tmp102_class_init,
};

static void tmp102_register_types(void)
{
    type_register_static(&tmp102_info);
}

type_init(tmp102_register_types)
