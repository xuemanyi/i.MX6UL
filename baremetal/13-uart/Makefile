CROSS_COMPILE	:= arm-linux-gnueabihf-
TARGET		?= uart

CC		:= $(CROSS_COMPILE)gcc
LD		:= $(CROSS_COMPILE)gcc
OBJCOPY		:= $(CROSS_COMPILE)objcopy
OBJDUMP		:= $(CROSS_COMPILE)objdump

# 不要手写 libgcc 路径
# LIBPATH 删除

INCDIRS		:= imx6ul \
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

SRCDIRS		:= project \
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

INCLUDE		:= $(patsubst %, -I %, $(INCDIRS))

SFILES		:= $(foreach dir, $(SRCDIRS), $(wildcard $(dir)/*.S))
CFILES		:= $(foreach dir, $(SRCDIRS), $(wildcard $(dir)/*.c))

SOBJS		:= $(patsubst %.S, obj/%.o, $(notdir $(SFILES)))
COBJS		:= $(patsubst %.c, obj/%.o, $(notdir $(CFILES)))
OBJS		:= $(SOBJS) $(COBJS)

VPATH		:= $(SRCDIRS)

CFLAGS		:= -Wall -nostdlib -fno-builtin -c -O2 $(INCLUDE)
LDFLAGS		:= -nostdlib -T imx6ul.lds

.PHONY: all clean

all: $(TARGET).bin

$(TARGET).bin: $(OBJS)
	$(LD) $(LDFLAGS) -o $(TARGET).elf $^ -lgcc
	$(OBJCOPY) -O binary -S $(TARGET).elf $@
	$(OBJDUMP) -D -m arm $(TARGET).elf > $(TARGET).dis

obj/%.o: %.S
	@mkdir -p obj
	$(CC) $(CFLAGS) -o $@ $<

obj/%.o: %.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -rf obj $(TARGET).elf $(TARGET).dis $(TARGET).bin