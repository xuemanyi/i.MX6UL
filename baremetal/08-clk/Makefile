# Bare-metal BSP Makefile for i.MX6UL
#
# This Makefile:
# - Collects sources from multiple directories
# - Compiles objects into obj/
# - Links using custom linker script
# - Generates ELF / BIN / DIS outputs

# Cross compiler prefix
CROSS_COMPILE := arm-linux-gnueabihf-

# Final target name
TARGET        ?= bsp

# Toolchain
CC      := $(CROSS_COMPILE)gcc
LD      := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump

# Include directories (converted to -I flags)
INCDIRS := imx6ul \
           bsp/clk \
           bsp/led \
           bsp/delay \
		   bsp/beep \
		   bsp/gpio \
		   bsp/key

# Source directories
SRCDIRS := project \
           bsp/clk \
           bsp/led \
           bsp/delay \
		   bsp/beep \
		   bsp/gpio \
		   bsp/key

# Object output directory
OBJDIR := obj

# Generate include flags
INCLUDE := $(addprefix -I,$(INCDIRS))

# Collect source files
SFILES := $(foreach dir,$(SRCDIRS),$(wildcard $(dir)/*.S))
CFILES := $(foreach dir,$(SRCDIRS),$(wildcard $(dir)/*.c))

# Convert to object files (placed in obj/)
# Example: project/main.c -> obj/main.o
OBJS := $(patsubst %.S,$(OBJDIR)/%.o,$(notdir $(SFILES))) \
        $(patsubst %.c,$(OBJDIR)/%.o,$(notdir $(CFILES)))

# Allow make to search source files in SRCDIRS
VPATH := $(SRCDIRS)

# Compiler / assembler flags
# -nostdlib      : no standard C library
# -ffreestanding : bare-metal environment
CFLAGS  := -Wall -O2 -nostdlib -ffreestanding $(INCLUDE)
ASFLAGS := $(CFLAGS)

# Linker flags (use custom linker script)
LDFLAGS := -Timx6ul.lds

.PHONY: all clean

# Default target
all: $(TARGET).bin

# Link objects -> ELF -> BIN -> DIS
$(TARGET).bin: $(OBJS)
	@echo "Linking objects:"
	@echo $(OBJS)
	$(LD) $(LDFLAGS) -o $(TARGET).elf $^
	$(OBJCOPY) -O binary -S $(TARGET).elf $@
	$(OBJDUMP) -D -m arm $(TARGET).elf > $(TARGET).dis

# Compile assembly files
$(OBJDIR)/%.o: %.S | $(OBJDIR)
	$(CC) $(ASFLAGS) -c -o $@ $<

# Compile C source files
$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# Create obj directory if not exists
$(OBJDIR):
	mkdir -p $@

# Clean all build artifacts
clean:
	rm -rf $(OBJDIR) $(TARGET).elf $(TARGET).dis $(TARGET).bin