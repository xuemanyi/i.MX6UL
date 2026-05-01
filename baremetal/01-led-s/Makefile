CROSS_COMPILE := arm-linux-gnueabihf-

AS      := $(CROSS_COMPILE)as
LD      := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump

TARGET := led
SRC    := led.s
OBJ    := led.o

# Load address of the program in memory
# This defines where the code will be placed and executed.
# It must match the actual load address used by the bootloader or debugger.
# If mismatched, all absolute/relative addresses will be incorrect and the program will crash.
LOAD_ADDR := 0x87800000

all: $(TARGET).bin

$(OBJ): $(SRC)
	$(AS) -mcpu=cortex-a7 -o $@ $<

$(TARGET).elf: $(OBJ)
	$(LD) -Ttext $(LOAD_ADDR) -o $@ $<

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(OBJDUMP) -D $< > $(TARGET).dis

clean:
	rm -f *.o *.elf *.bin *.dis