# Program name
TARGET := led-app

# Source file
SRC := led-app.c

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
