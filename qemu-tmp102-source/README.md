# TMP102 QEMU Source Snapshot

This folder mirrors only the QEMU files created for the TMP102 device model.
It is here so the source can be browsed directly on GitHub without pushing a
full QEMU checkout.

## Files

```text
hw/sensor/tmp102.c
include/hw/sensor/tmp102.h
include/hw/sensor/tmp102_regs.h
tests/qtest/tmp102-test.c
```

These files belong inside a QEMU tree at the same paths shown above.

## Integration

The device also needs small build-system edits in existing QEMU files:

```text
hw/sensor/Kconfig
hw/sensor/meson.build
tests/qtest/meson.build
```

Those edits are included in the patch series under `../patches/`.
Use the patches if you want to apply the work to a real QEMU checkout.
