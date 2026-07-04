# QEMU TMP102 Device Model

[![CI](https://github.com/aqjole/qemu-tmp102-device-model/actions/workflows/ci.yml/badge.svg)](https://github.com/aqjole/qemu-tmp102-device-model/actions/workflows/ci.yml)

This repository adds a TMP102 I2C temperature-sensor model to QEMU and includes
a tiny bare-metal ARM firmware demo that talks to it.

Why this project:
- It models real hardware behavior inside QEMU instead of mocking it in app code.
- It validates the model with qtests, including VMState migration coverage.
- It keeps the work reproducible as an ordered patch series applied by CI.

The local upstream QEMU checkout lives in `qemu/` and is intentionally ignored by git. Project artifacts such as docs, firmware, patch series, and CI files are tracked here.

## Layout

```text
.
├── README.md
├── docs/
├── firmware/tmp102-demo/
├── .github/workflows/
├── patches/
├── qemu-tmp102-source/
├── reference/
├── review_book.md
└── ci/
```

`qemu/`, `reference/`, `review_book.md`, and the implementation plan are local
development/reference files and are ignored by git.

`qemu-tmp102-source/` contains a browseable snapshot of the TMP102 QEMU source
files plus a small integration patch showing the exact QEMU build-system edits
needed to wire them in. `patches/` contains the same work as an ordered patch
series.

## CI

GitHub Actions checks the project from a clean base: it clones QEMU at the
pinned commit the patch series was developed against, applies `patches/` with
`git am`, builds `qemu-system-arm`, runs the TMP102 qtests, and runs the
firmware host test.

CI currently verifies:
- The patch series applies cleanly to the pinned QEMU base.
- `qemu-system-arm` builds with the TMP102 device model.
- `qemu:qtest-arm/qos-test` passes, including the TMP102 QoS tests.
- `qemu:qtest-arm/tmp102-fault-test` passes for runtime fault injection.
- `qemu:qtest-arm/tmp102-migration-test` passes for VMState migration.
- `make -C firmware/tmp102-demo test` passes for host-side decode logic.

## Development History

The QEMU device model was developed as an 18-commit patch series. The patches in
`patches/` preserve that progression:

```text
0001  register headers
0002  device skeleton
0003  QEMU build wiring
0004  register I/O
0005  register I/O qtest
0006  temperature property
0007  extended temperature mode test
0008  comparator ALERT
0009  ALERT polarity test
0010  ALERT status bit
0011  fault queue
0012  interrupt mode
0013  shutdown mode
0014  one-shot mode
0015  preserve command-line temperature across reset
0016  VMState support and command-line temperature qtest
0017  migration qtest for VMState
0018  runtime fault injection
```

For browsing the final source directly, see `qemu-tmp102-source/`. For applying
the work to a QEMU checkout with the development history preserved, use the
ordered patch files in `patches/`.

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
meson test -C build --print-errorlogs \
  qemu:qtest-arm/qos-test \
  qemu:qtest-arm/tmp102-fault-test \
  qemu:qtest-arm/tmp102-migration-test
```

The TMP102 tests cover register I/O, temperature encoding, ALERT behavior,
interrupt mode, shutdown/one-shot behavior, command-line temperature
initialization, runtime fault injection, and VMState migration.

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

To demonstrate firmware error handling with an injected I2C NACK:

```sh
cd firmware/tmp102-demo
make run-fault
```

Expected output:

```text
TMP102 read failed
```

To quit QEMU from the terminal, press `Ctrl-A`, then `X`.

## Clean Firmware Build

```sh
cd firmware/tmp102-demo
make clean
```
