# Program name
TARGET := app-new-char-led

# Source file
SRC := new-char-led.c

# Cross compiler (can be overridden)
CC ?= arm-linux-gnueabihf-gcc

# Compiler flags
CFLAGS ?= -Wall -Wextra -O2

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	$(RM) $(TARGET)
