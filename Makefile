# Compiler and assembler
CC      := clang
AS      := as
LD      := ld
SDKROOT := $(shell xcrun --sdk macosx --show-sdk-path)

# Directories
BUILD_DIR := build
LIB_SRC   := lib
LIB_OUT   := $(BUILD_DIR)/librs.so

# Flags
CFLAGS   := -std=c99 -Wall -Wextra -Werror -fPIC
LDFLAGS  := -shared

# Targets
.PHONY: all test clean

# Default: build shared library
all: $(LIB_OUT)

$(LIB_OUT): $(wildcard $(LIB_SRC)/*.c) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Test target: compile and run test + assemble and link .S
test: all
	$(CC) simple.c -L$(BUILD_DIR) -lrs -o simple
	./simple
	$(AS) simple.S -o simple.o
	$(LD) -arch arm64 -lSystem \
		-syslibroot $(SDKROOT) \
		-e _start -o simple simple.o

# Clean everything
clean:
	rm -rf $(BUILD_DIR) simple simple.S simple.o
