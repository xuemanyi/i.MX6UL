# Program name
TARGET := chardev_app

# Source files
SRCS := chardev_app.c

# Object files
OBJS := $(SRCS:.c=.o)

# Cross compiler (can be overridden)
CC ?= arm-linux-gnueabihf-gcc

# Compiler flags
CFLAGS ?= -Wall -Wextra -O2

# Include directories
CPPFLAGS += -I.

# Phony targets
.PHONY: all clean

# Default target
all: $(TARGET)

# Link target
$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

# Compile source files
%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

# Clean target
clean:
	$(RM) $(TARGET) $(OBJS)
