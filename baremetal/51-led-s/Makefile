TARGET := led
SRC    := led.s
OBJ    := led.o

# 程序加载地址必须与下载工具或调试器使用的地址保持一致。
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

.PHONY: all clean
