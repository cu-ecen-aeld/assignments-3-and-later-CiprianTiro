# Variables
CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -g -Wall -Werror
TARGET = aesdsocket
LDFLAGS ?= -lpthread -lrt

# a. All and default targets
all: $(TARGET)

CROSS_COMPILE: all
	@:

default: all

$(TARGET): aesdsocket.o
	$(CC) $(CFLAGS) -o $(TARGET) aesdsocket.o $(LDFLAGS)

aesdsocket.o: aesdsocket.c
	$(CC) $(CFLAGS) -c aesdsocket.c -o aesdsocket.o

clean:
	rm -f $(TARGET) *.o *.elf *.map

.PHONY: all clean CROSS_COMPILE