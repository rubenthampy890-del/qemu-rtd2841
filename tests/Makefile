# tests/Makefile - Build bare-metal diagnostic test binaries from assembly source

CROSS_COMPILE ?= arm-none-eabi-
AS            := $(CROSS_COMPILE)as
LD            := $(CROSS_COMPILE)ld
OBJCOPY       := $(CROSS_COMPILE)objcopy

ASFLAGS       := -mcpu=cortex-a53
LDFLAGS       := -Ttext 0x10000

TARGETS       := firmware_test.bin firmware_fb_test.bin

all: $(TARGETS)

firmware_test.bin: firmware_test.s
	$(AS) $(ASFLAGS) $< -o firmware_test.o
	$(LD) $(LDFLAGS) firmware_test.o -o firmware_test.elf
	$(OBJCOPY) -O binary firmware_test.elf $@
	rm -f firmware_test.o firmware_test.elf

firmware_fb_test.bin: firmware_fb_test.s
	$(AS) $(ASFLAGS) $< -o firmware_fb_test.o
	$(LD) $(LDFLAGS) firmware_fb_test.o -o firmware_fb_test.elf
	$(OBJCOPY) -O binary firmware_fb_test.elf $@
	rm -f firmware_fb_test.o firmware_fb_test.elf

clean:
	rm -f $(TARGETS) *.o *.elf

.PHONY: all clean
