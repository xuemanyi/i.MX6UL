TARGET ?= uart

TOOLCHAIN_DIR := /usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf
CROSS_COMPILE := $(TOOLCHAIN_DIR)/bin/arm-linux-gnueabihf-

CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump

CFLAGS := -Wall -nostdlib -fno-builtin -c -O2 -marm -march=armv7-a
LIBPATH := -L$(TOOLCHAIN_DIR)/lib/gcc/arm-linux-gnueabihf/4.9.4 -lgcc

INCDIRS	:= imx6ul \
	stdio/include \
	bsp/clk \
	bsp/led \
	bsp/delay \
	bsp/beep \
	bsp/gpio \
	bsp/key \
	bsp/exit \
	bsp/int \
	bsp/epit-timer \
	bsp/key-filter \
	bsp/uart

SRCDIRS	:= project \
	stdio/lib \
	bsp/clk \
	bsp/led \
	bsp/delay \
	bsp/beep \
	bsp/gpio \
	bsp/key \
	bsp/exit \
	bsp/int \
	bsp/epit-timer \
	bsp/key-filter \
	bsp/uart

INCLUDE := $(patsubst %, -I%, $(INCDIRS))

SFILES := $(foreach dir, $(SRCDIRS), $(wildcard $(dir)/*.S))
CFILES := $(foreach dir, $(SRCDIRS), $(wildcard $(dir)/*.c))

SFILENDIR := $(notdir $(SFILES))
CFILENDIR := $(notdir $(CFILES))

SOBJS := $(patsubst %, obj/%, $(SFILENDIR:.S=.o))
COBJS := $(patsubst %, obj/%, $(CFILENDIR:.c=.o))
OBJS := $(SOBJS) $(COBJS)

VPATH := $(SRCDIRS)

.PHONY: clean

$(TARGET).bin : $(OBJS)
	$(LD) -Timx6ul.lds -o $(TARGET).elf $^ $(LIBPATH)
	$(OBJCOPY) -O binary -S $(TARGET).elf $@
	$(OBJDUMP) -D -m arm $(TARGET).elf > $(TARGET).dis

$(SOBJS) : obj/%.o : %.S
	@mkdir -p obj
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ $<

$(COBJS) : obj/%.o : %.c
	@mkdir -p obj
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ $<

clean:
	rm -rf $(TARGET).elf $(TARGET).dis $(TARGET).bin $(COBJS) $(SOBJS)