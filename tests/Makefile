# tests/Makefile - Build bare-metal diagnostic test binaries from assembly source

CROSS_COMPILE ?= arm-none-eabi-
AS            := $(CROSS_COMPILE)as
OBJCOPY       := $(CROSS_COMPILE)objcopy

ASFLAGS       := -mcpu=cortex-a53

TARGETS       := firmware_test.bin firmware_fb_test.bin

all: $(TARGETS)

firmware_test.bin: firmware_test.s
	$(AS) $(ASFLAGS) $< -o firmware_test.o
	$(OBJCOPY) -O binary firmware_test.o $@
	rm -f firmware_test.o

firmware_fb_test.bin: firmware_fb_test.s
	$(AS) $(ASFLAGS) $< -o firmware_fb_test.o
	$(OBJCOPY) -O binary firmware_fb_test.o $@
	rm -f firmware_fb_test.o

clean:
	rm -f $(TARGETS) *.o

.PHONY: all clean
