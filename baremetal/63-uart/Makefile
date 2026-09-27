TARGET ?= uart

LDLIBS := -lgcc
CFLAGS := -Wall -Wextra -nostdlib -ffreestanding -fno-builtin -c -O2 \
	-marm -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard

INCDIRS := imx6ul \
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

SRCDIRS := project \
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

.PHONY: all clean

all: $(TARGET).bin

$(TARGET).bin: $(OBJS)
	$(CC) -nostdlib -Timx6ul.lds -o $(TARGET).elf $^ $(LDLIBS)
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
