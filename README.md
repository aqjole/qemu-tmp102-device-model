# QEMU TMP102 Device Model

This repo tracks the project wrapper for a QEMU TMP102 I2C temperature-sensor model.

The local upstream QEMU checkout lives in `qemu/` and is intentionally ignored by git. Project artifacts such as docs, firmware, patch series, and CI files are tracked here.

## Layout

```text
.
├── README.md
├── docs/
├── firmware/
├── patches/
└── ci/
```

## Local QEMU Build

QEMU is configured locally for ARM system emulation:

```sh
cd qemu
./configure --target-list=arm-softmmu --disable-docs --disable-werror --disable-gtk --disable-sdl --disable-cocoa --disable-slirp
ninja -C build qemu-system-arm
```

