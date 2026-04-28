# Use := for immediate assignment
SRC := writer.c
TARGET := writer
OBJS := $(SRC:.c=.o)

CROSS_COMPILE ?=
CC = $(CROSS_COMPILE)gcc

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