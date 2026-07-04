CC = cc
CFLAGS = -Wall -Wextra -std=c11

CROSS = arm-none-eabi-
ARM_CC = $(CROSS)gcc
ARM_OBJCOPY = $(CROSS)objcopy
ARM_CFLAGS = -mcpu=cortex-a15 -marm -ffreestanding -Wall -Wextra -std=c11 -O0 -g
ARM_LDFLAGS = -T linker.ld -nostdlib -Wl,-Map=firmware.map

FIRMWARE_ELF = firmware.elf
FIRMWARE_BIN = firmware.bin

all: $(FIRMWARE_ELF)

$(FIRMWARE_ELF): startup.S main.c tmp102_driver.c tmp102_driver.h uart.c uart.h linker.ld
	$(ARM_CC) $(ARM_CFLAGS) $(ARM_LDFLAGS) -o $(FIRMWARE_ELF) startup.S main.c tmp102_driver.c uart.c

$(FIRMWARE_BIN): $(FIRMWARE_ELF)
	$(ARM_OBJCOPY) -O binary $(FIRMWARE_ELF) $(FIRMWARE_BIN)

test_tmp102_driver: test_tmp102_driver.c tmp102_driver.c tmp102_driver.h
	$(CC) $(CFLAGS) -o test_tmp102_driver test_tmp102_driver.c tmp102_driver.c

test: test_tmp102_driver
	./test_tmp102_driver

clean:
	rm -f $(FIRMWARE_ELF) $(FIRMWARE_BIN) firmware.map test_tmp102_driver
