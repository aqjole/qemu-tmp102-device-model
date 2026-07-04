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
integration-edits.patch
```

The `.c` and `.h` files belong inside a QEMU tree at the same paths shown
above. `integration-edits.patch` contains the exact TMP102 lines added to
existing QEMU build files.

## Integration

This folder is a source snapshot for browsing. Use the ordered patch series in
`../patches/` if you want to apply the work to a real QEMU checkout with commit
history preserved.
