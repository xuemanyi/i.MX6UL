CROSS_COMPILE := arm-linux-gnueabihf-

CC      := $(CROSS_COMPILE)gcc
LD      := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump

TARGET := ledc
OBJS   := start.o main.o
LDS    := imx6ul.lds

CFLAGS := -Wall -nostdlib -O2

all: $(TARGET).bin

$(TARGET).bin: $(OBJS)
	$(LD) -T $(LDS) -o $(TARGET).elf $^
	$(OBJCOPY) -O binary -S $(TARGET).elf $@
	$(OBJDUMP) -D -m arm $(TARGET).elf > $(TARGET).dis

%.o: %.s
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o $(TARGET).bin $(TARGET).elf $(TARGET).dis