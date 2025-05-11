# Compiler and assembler
CC      := clang
AS      := as
LD      := ld
SDKROOT := $(shell xcrun --sdk macosx --show-sdk-path)

# Directories
BUILD_DIR := build
LIB_SRC := lib
LIB_OUT := $(BUILD_DIR)/librs.so

# Install location
PREFIX ?= /usr/local
LIB_DEST := $(PREFIX)/lib
INC_DEST := $(PREFIX)/include/runestone
PKGCONFIG_DIR := $(LIB_DEST)/pkgconfig
PC_TEMPLATE := runestone.pc.in
PC_FILE := runestone.pc

# Flags
CFLAGS := -std=c99 -Wall -Wextra -Werror -fPIC
LDFLAGS := -shared

.PHONY: all test clean install uninstall

# Default build
all: $(LIB_OUT)

$(LIB_OUT): $(wildcard $(LIB_SRC)/*.c) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Run test
test: all
	$(CC) simple.c -L$(BUILD_DIR) -lrs -o simple
	./simple
	$(AS) simple.S -o simple.o
	$(LD) -arch arm64 -lSystem \
		-syslibroot $(SDKROOT) \
		-e _start -o simple simple.o

# Install library, headers, and pkg-config file
install: all
	install -d $(LIB_DEST)
	install -m 755 $(LIB_OUT) $(LIB_DEST)
	install -d $(INC_DEST)
	install -m 644 $(LIB_SRC)/*.h $(INC_DEST)
	install -d $(PKGCONFIG_DIR)
	sed 's|@PREFIX@|$(PREFIX)|g' $(PC_TEMPLATE) > $(PC_FILE)
	install -m 644 $(PC_FILE) $(PKGCONFIG_DIR)
	rm -f $(PC_FILE)

# Uninstall everything
uninstall:
	rm -f $(LIB_DEST)/librs.so
	rm -rf $(INC_DEST)
	rm -f $(PKGCONFIG_DIR)/runestone.pc

# Clean
clean:
	rm -rf $(BUILD_DIR) simple simple.S simple.o $(PC_FILE)
