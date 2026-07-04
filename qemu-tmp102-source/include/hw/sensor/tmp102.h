/*
 * Texas Instruments TMP102 Temperature Sensor
 *
 * Datasheet:
 * https://www.ti.com/lit/ds/symlink/tmp102.pdf
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * later. See the COPYING file in the top-level directory.
 */

#ifndef QEMU_TMP102_H
#define QEMU_TMP102_H

#include "hw/i2c/i2c.h"
#include "hw/sensor/tmp102_regs.h"
#include "qom/object.h"

#define TYPE_TMP102 "tmp102"
OBJECT_DECLARE_SIMPLE_TYPE(TMP102State, TMP102)

/**
 * TMP102State:
 * @pointer: currently selected register pointer.
 * @config: 16-bit configuration register.
 * @temperature: current measured temperature in sixteenths of a degree C.
 * @t_low: raw low-threshold register.
 * @t_high: raw high-threshold register.
 * @alert: ALERT GPIO output.
 * @alert_asserted: logical ALERT state before POL is applied.
 * @detect_falling: false while waiting for high threshold, true while waiting
 *                  for low threshold.
 *
 * The external QOM temperature property should use milli-Celsius, matching the
 * TMP105 model. Internally, sixteenths of a degree maps directly to the
 * TMP102 0.0625 C LSB.
 */
struct TMP102State {
    /*< private >*/
    I2CSlave i2c;
    /*< public >*/

    uint8_t pointer;
    uint8_t len;
    uint8_t buf[2];

    uint16_t config;
    int16_t temperature;
    uint16_t t_low;
    uint16_t t_high;

    qemu_irq alert;
    bool alert_asserted;
    bool detect_falling;
    uint8_t high_faults;
    uint8_t low_faults;
};

#endif
