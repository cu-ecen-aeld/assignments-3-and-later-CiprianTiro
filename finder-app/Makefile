SRC := writer.c
TARGET = writer
OBJS := $(SRC:.c=.o)

CROSS_PATH = /home/ciprian/Downloads/arm-gnu-toolchain-15.2.rel1-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-

ifneq (,$(filter CROSS_COMPILE,$(MAKECMDGOALS)))
    CC = $(CROSS_PATH)gcc
else
    CC = g++
endif

# If CROSS_COMPILE command, treat it exactly like for all
CROSS_COMPILE: all
	@:

all: $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJS) -o $(TARGET) $(LDFLAGS)

clean:
	-rm -f *.o $(TARGET) *.elf *.map

.PHONY: all clean CROSS_COMPILE