CROSS_COMPILE := arm-linux-gnueabihf-
NAME          ?= ledc

CC      := $(CROSS_COMPILE)gcc
LD      := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump

OBJS := start.o main.o

CFLAGS := -Wall -O2 -nostdlib -ffreestanding

all: $(NAME).bin

$(NAME).bin: $(OBJS)
	$(LD) -Timx6ul.lds -o $(NAME).elf $^
	$(OBJCOPY) -O binary -S $(NAME).elf $@
	$(OBJDUMP) -D -m arm $(NAME).elf > $(NAME).dis

%.o: %.S
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.s
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf *.o $(NAME).bin $(NAME).elf $(NAME).dis

.PHONY: all clean