# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -g -std=gnu99 -Wno-unused-but-set-variable -Wno-unused-variable
LDFLAGS = -D_GNU_SOURCE -lpthread 

# Object files
OBJS = main.o

# Executable name
TARGET = main

# Default target
all: $(TARGET)

# Link object files to create executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

# Compile main.c
main.o: main.c
	$(CC) $(CFLAGS) -c main.c

# Clean build artifacts
clean:
	rm -f $(OBJS) $(TARGET)

# Phony targets (not actual files)
.PHONY: all clean