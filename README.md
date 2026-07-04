# QEMU TMP102 Device Model

This repo tracks a learning project that adds a TMP102 I2C temperature-sensor
model to QEMU and a tiny bare-metal ARM firmware demo that talks to it.

The local upstream QEMU checkout lives in `qemu/` and is intentionally ignored by git. Project artifacts such as docs, firmware, patch series, and CI files are tracked here.

## Layout

```text
.
├── README.md
├── docs/
├── firmware/tmp102-demo/
├── patches/
├── reference/
├── review_book.md
└── ci/
```

`qemu/`, `reference/`, `review_book.md`, and the implementation plan are local
learning/work files and are ignored by git.

## Build QEMU

From the project root:

```sh
cd qemu
./configure --target-list=arm-softmmu --disable-docs --disable-werror --disable-gtk --disable-sdl --disable-cocoa --disable-slirp
ninja -C build qemu-system-arm
```

The important output is:

```text
build/qemu-system-arm
```

That is the ARM system emulator containing the TMP102 device model.

## Test QEMU Device Model

Run the ARM qtests:

```sh
cd qemu
meson test -C build qemu:qtest-arm/qos-test
```

The TMP102 tests cover register I/O, temperature encoding, ALERT behavior,
interrupt mode, shutdown/one-shot behavior, VMState support, and command-line
temperature initialization.

You can also confirm QEMU knows about the device:

```sh
cd qemu
build/qemu-system-arm -device help | rg tmp102
```

Expected:

```text
name "tmp102", bus i2c-bus
```

## Build Firmware Demo

The demo firmware lives in `firmware/tmp102-demo/`.

```sh
cd firmware/tmp102-demo
make
```

This builds:

```text
firmware.elf
firmware.bin
```

The demo uses the Raspberry Pi 2B machine and reads TMP102 over I2C bus 1.

## Test Firmware Helper Logic

The TMP102 decode logic can be tested on the host without QEMU:

```sh
cd firmware/tmp102-demo
make test
```

## Run Firmware Demo

Run with the default 25 C sensor value:

```sh
cd firmware/tmp102-demo
make run
```

Expected output:

```text
TMP102 raw: 0x1900
Temperature: 25 C
```

To try another temperature, run QEMU manually and change the `temperature`
property. The value is in milli-Celsius, so `80000` means 80 C:

```sh
cd firmware/tmp102-demo
../../qemu/build/qemu-system-arm \
  -M raspi2b \
  -display none \
  -serial mon:stdio \
  -bios firmware.bin \
  -device tmp102,address=0x48,bus=i2c-bus.1,temperature=80000
```

Expected output:

```text
TMP102 raw: 0x5000
Temperature: 80 C
```

To quit QEMU from the terminal, press `Ctrl-A`, then `X`.

## Clean Firmware Build

```sh
cd firmware/tmp102-demo
make clean
```
