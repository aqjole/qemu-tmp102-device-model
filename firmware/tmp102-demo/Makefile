CC = cc
CFLAGS = -Wall -Wextra -std=c11

CROSS = arm-none-eabi-
ARM_CC = $(CROSS)gcc
ARM_OBJCOPY = $(CROSS)objcopy
ARM_CFLAGS = -mcpu=cortex-a7 -marm -ffreestanding -Wall -Wextra -std=c11 -O0 -g -DTMP102_DEMO_BAREMETAL
ARM_LDFLAGS = -T linker.ld -nostdlib -Wl,-Map=firmware.map
QEMU = ../../qemu/build/qemu-system-arm

FIRMWARE_ELF = firmware.elf
FIRMWARE_BIN = firmware.bin

all: $(FIRMWARE_BIN)

$(FIRMWARE_ELF): startup.S main.c tmp102_driver.c tmp102_driver.h i2c.c i2c.h uart.c uart.h linker.ld
	$(ARM_CC) $(ARM_CFLAGS) $(ARM_LDFLAGS) -o $(FIRMWARE_ELF) startup.S main.c tmp102_driver.c i2c.c uart.c

$(FIRMWARE_BIN): $(FIRMWARE_ELF)
	$(ARM_OBJCOPY) -O binary $(FIRMWARE_ELF) $(FIRMWARE_BIN)

test_tmp102_driver: test_tmp102_driver.c tmp102_driver.c tmp102_driver.h
	$(CC) $(CFLAGS) -o test_tmp102_driver test_tmp102_driver.c tmp102_driver.c

test: test_tmp102_driver
	./test_tmp102_driver

run: $(FIRMWARE_BIN)
	$(QEMU) -M raspi2b -display none -serial mon:stdio -bios $(FIRMWARE_BIN) -device tmp102,address=0x48,bus=i2c-bus.1,temperature=25000

clean:
	rm -f $(FIRMWARE_ELF) $(FIRMWARE_BIN) firmware.map test_tmp102_driver
