# Use := for immediate assignment
SRC := writer.c
TARGET := writer
OBJS := $(SRC:.c=.o)

# Toolchain path
CROSS_PATH = /home/ciprian/Downloads/arm-gnu-toolchain-15.2.rel1-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-

# Logic to switch compilers
ifneq (,$(filter CROSS_COMPILE,$(MAKECMDGOALS)))
    CC = $(CROSS_PATH)gcc
else
    # Suggest using gcc here if writer.c is a C file
    CC = gcc
endif

# Default target
all: $(TARGET)

# The redirect for specific command style
CROSS_COMPILE: all
	@:

# The build rule
$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Rule for object files
%.o : %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	-rm -f *.o writer *.elf *.map

.PHONY: all clean CROSS_COMPILE