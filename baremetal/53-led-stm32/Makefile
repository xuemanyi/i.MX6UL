TARGET := ledc
OBJS := start.o main.o

CFLAGS := -Wall -Wextra -O2 -nostdlib -ffreestanding

all: $(TARGET).bin

$(TARGET).bin: $(OBJS)
	$(LD) -Timx6ul.lds -o $(TARGET).elf $^
	$(OBJCOPY) -O binary -S $(TARGET).elf $@
	$(OBJDUMP) -D -m arm $(TARGET).elf > $(TARGET).dis

%.o: %.s
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf *.o $(TARGET).bin $(TARGET).elf $(TARGET).dis

.PHONY: all clean
