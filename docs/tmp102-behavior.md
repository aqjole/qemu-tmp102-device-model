# TMP102 Behavior Reference

Source: [Texas Instruments TMP102 datasheet, Rev. I, revised June 2024](https://www.ti.com/lit/ds/symlink/tmp102.pdf).

This file is the implementation reference for the QEMU TMP102 model. It records the device behavior we need to reproduce, the register constants to encode, and the qtest vectors that should prove the model.

## Datasheet Source Map

- Section 6.3.1: digital temperature output and conversion examples.
- Section 6.3.2 through 6.3.6: serial interface, addressing, pointer writes, and register reads.
- Section 6.3.8: general-call reset.
- Section 6.4: functional modes.
- Section 6.5.1 through 6.5.4: pointer, temperature, configuration, and threshold registers.

## Implementation Artifacts

- `qemu/include/hw/sensor/tmp102_regs.h`: shared register constants for the device model and qtests.
- `qemu/include/hw/sensor/tmp102.h`: QOM/I2C device type declaration and state struct.

## Modeling Scope

Model in QEMU:

- I2C target device behavior.
- Address selection from ADD0-equivalent configuration.
- Pointer register and four data registers.
- Temperature encoding in normal and extended mode.
- Configuration register reset values and writable/read-only bits.
- `T_LOW` and `T_HIGH` threshold registers.
- ALERT output state in comparator and interrupt modes.
- Fault queue, shutdown, one-shot, conversion-rate, and extended-mode bits.
- General-call reset only if it fits cleanly into QEMU's I2C target path.

Do not model initially:

- Analog supply behavior, package thermal lag, accuracy error, or self-heating.
- I2C electrical timing, pullups, spike filters, or high-speed filter switching.
- Real ADC conversion timing beyond simple state changes needed by tests.

## Addressing And Bus Behavior

TMP102 is an I2C/SMBus target. All data bytes are transmitted MSB first.

ADD0-derived 7-bit addresses:

| ADD0 connection | 7-bit address | Hex |
| --- | ---: | ---: |
| GND | `1001000` | `0x48` |
| V+ | `1001001` | `0x49` |
| SDA | `1001010` | `0x4A` |
| SCL | `1001011` | `0x4B` |

QEMU model decision:

- Expose an `address` QOM property accepting `0x48` through `0x4B`.
- Default to `0x48`.
- Reject or clamp invalid values only if the local QEMU I2C pattern expects that; prefer rejecting with an error.

## Pointer Register

The pointer register is an 8-bit selector. Bits `P1:P0` select the active data register; bits `P7:P2` must be written as zero. On reset, `P1:P0 = 00`, so the temperature register is selected.

| Pointer bits | Register | Access |
| --- | --- | --- |
| `00` | Temperature | Read-only |
| `01` | Configuration | Read/write |
| `10` | `T_LOW` | Read/write |
| `11` | `T_HIGH` | Read/write |

Implementation rules:

- A write transaction starts with a pointer byte.
- Additional bytes write the selected register.
- A read transaction returns bytes from the register selected by the most recent pointer write.
- Multi-byte registers are transferred MSB first.
- Reads may be one or two bytes; qtests should cover both if the QEMU I2C test helpers make this easy.

## Register Summary

| Register | Pointer | Width | Reset | Access | Notes |
| --- | ---: | ---: | ---: | --- | --- |
| Temperature | `0x00` | 16 bits | `0x0000` until first conversion | R | External `temperature` property drives this. |
| Configuration | `0x01` | 16 bits | `0x60A0` | R/W with read-only bits | Controls modes and ALERT. |
| `T_LOW` | `0x02` | 16 bits | `0x4B00` | R/W | 75 C in normal 12-bit format. |
| `T_HIGH` | `0x03` | 16 bits | `0x5000` | R/W | 80 C in normal 12-bit format. |

## Temperature Encoding

Normal mode (`EM=0`):

- 12-bit two's-complement value.
- LSB = `0.0625 C`.
- Temperature bits occupy register bits `[15:4]`.
- Register bits `[3:0]` read as zero.

Extended mode (`EM=1`):

- 13-bit two's-complement value.
- Same LSB: `0.0625 C`.
- Temperature bits occupy register bits `[15:3]`.
- For the temperature register, bit `0` reads as `1` to indicate extended format; bits `[2:1]` read as zero.
- For `T_LOW` and `T_HIGH`, unused low bits read as zero.

Normal-mode register test vectors:

| Temperature | 12-bit raw | 16-bit register |
| ---: | ---: | ---: |
| `25 C` | `0x190` | `0x1900` |
| `0 C` | `0x000` | `0x0000` |
| `-25 C` | `0xE70` | `0xE700` |
| `75 C` | `0x4B0` | `0x4B00` |
| `80 C` | `0x500` | `0x5000` |
| `127.9375 C` | `0x7FF` | `0x7FF0` |

Extended-mode raw test vectors:

| Temperature | 13-bit raw | Temperature register bytes |
| ---: | ---: | ---: |
| `150 C` | `0x0960` | `0x4B01` |
| `128 C` | `0x0800` | `0x4001` |
| `25 C` | `0x0190` | `0x0C81` |
| `0 C` | `0x0000` | `0x0001` |
| `-25 C` | `0x1E70` | `0xF381` |

Implementation helper shape:

```c
static uint16_t tmp102_encode_temp(int milli_c, bool extended, bool is_temp_reg);
static int tmp102_decode_limit(uint16_t reg, bool extended);
```

Use integer math in milli-Celsius or sixteenth-degrees internally. Avoid floating point in the device model.

## Configuration Register

Reset value: `0x60A0`.

Byte 1, MSB:

| Bit | Mask | Name | Reset | Behavior |
| ---: | ---: | --- | ---: | --- |
| 15 | `0x8000` | `OS` | `0` | One-shot start/conversion-ready. |
| 14 | `0x4000` | `R1` | `1` | Read-only resolution bit. |
| 13 | `0x2000` | `R0` | `1` | Read-only resolution bit. |
| 12 | `0x1000` | `F1` | `0` | Fault queue. |
| 11 | `0x0800` | `F0` | `0` | Fault queue. |
| 10 | `0x0400` | `POL` | `0` | ALERT active polarity. |
| 9 | `0x0200` | `TM` | `0` | Comparator or interrupt mode. |
| 8 | `0x0100` | `SD` | `0` | Shutdown mode. |

Byte 2, LSB:

| Bit | Mask | Name | Reset | Behavior |
| ---: | ---: | --- | ---: | --- |
| 7 | `0x0080` | `CR1` | `1` | Conversion rate. |
| 6 | `0x0040` | `CR0` | `0` | Conversion rate. |
| 5 | `0x0020` | `AL` | `1` | Read-only alert status. |
| 4 | `0x0010` | `EM` | `0` | Extended 13-bit mode. |
| 3:0 | `0x000F` | unused | `0` | Read as zero. |

Write rules:

- Preserve `R1:R0 = 11`.
- Preserve or recompute read-only `AL`.
- Force unused bits `[3:0]` to zero.
- Honor writable bits: `OS`, `F1:F0`, `POL`, `TM`, `SD`, `CR1:CR0`, `EM`.

Conversion-rate settings:

| `CR1:CR0` | Rate |
| --- | ---: |
| `00` | `0.25 Hz` |
| `01` | `1 Hz` |
| `10` | `4 Hz` default |
| `11` | `8 Hz` |

Fault queue settings:

| `F1:F0` | Consecutive faults |
| --- | ---: |
| `00` | `1` |
| `01` | `2` |
| `10` | `4` |
| `11` | `6` |

## Mode Behavior

Continuous conversion:

- Default operating mode.
- In QEMU, the `temperature` property represents the latest completed conversion.
- Updating the property should recompute the temperature register and ALERT state.

Shutdown:

- `SD=1` leaves the serial interface available.
- For first implementation, freeze automatic conversion updates but allow QOM temperature changes for tests.
- Entering shutdown should clear interrupt-mode ALERT.

One-shot:

- Only meaningful while shutdown is active.
- Writing `OS=1` starts one conversion.
- Simple model: complete immediately, update the temperature register, then make `OS` read as `1`.
- More realistic delayed conversion can be deferred unless qtests need it.

Extended mode:

- `EM=0`: 12-bit register format.
- `EM=1`: 13-bit register format.
- Changing `EM` should affect temperature and threshold encode/decode behavior immediately.

## ALERT Behavior

ALERT polarity:

- `POL=0`: ALERT pin is active low.
- `POL=1`: ALERT pin is active high.
- Internally track logical alert asserted/deasserted, then apply polarity at the GPIO output.

Comparator mode (`TM=0`):

- Assert after temperature is greater than or equal to `T_HIGH` for the configured number of consecutive faults.
- Remain asserted until temperature is below `T_LOW` for the configured number of consecutive faults.

Interrupt mode (`TM=1`):

- Assert after temperature is greater than or equal to `T_HIGH` for the configured fault count.
- Clear on any register read, SMBus Alert Response, shutdown, or general-call reset.
- After a high-threshold interrupt is cleared, assert next when temperature drops below `T_LOW` for the configured fault count.
- After a low-threshold interrupt is cleared, assert next when temperature reaches or exceeds `T_HIGH` again.

`AL` bit:

- Read-only.
- Reflects comparator status, not the `TM` interrupt latch.
- `POL` inverts the reported `AL` bit.

## Reset State

Power-up or reset state:

- Pointer register selects temperature: `0x00`.
- Temperature register reads `0x0000` until first conversion.
- Configuration register: `0x60A0`.
- `T_LOW`: `0x4B00`.
- `T_HIGH`: `0x5000`.
- Comparator mode, active-low ALERT, continuous conversion, 4 Hz conversion rate, normal 12-bit mode.

## qtest Coverage From This Reference

Initial qtests:

- Device instantiates at default address `0x48`.
- Address property accepts `0x48` through `0x4B`.
- Reset values match this document.
- Pointer register selects each register.
- Configuration write masks read-only and unused bits correctly.
- Temperature property encodes normal-mode vectors.
- `T_LOW` and `T_HIGH` round-trip with MSB-first byte order.
- Comparator ALERT asserts and deasserts across thresholds.

Expanded qtests:

- Extended-mode temperature vectors.
- Extended-mode threshold decode.
- Fault queue counts: 1, 2, 4, 6.
- `POL` flips GPIO output and `AL` readback.
- Interrupt mode latch and clear-on-read cycle.
- Shutdown and one-shot behavior.
- General-call reset if implemented.
