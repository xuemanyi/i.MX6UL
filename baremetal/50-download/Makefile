HOSTCC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Werror -std=c11

TARGET := imxdownload

all: $(TARGET)

$(TARGET): imxdownload.c
	$(HOSTCC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET) load.imx

.PHONY: all clean
