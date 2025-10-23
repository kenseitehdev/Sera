# === Makefile for Sera ===

CC = gcc
CFLAGS = -Wall -O2 -g -Iinclude

# --- Homebrew Prefixes ---
LUA_PREFIX = $(shell brew --prefix lua)
QJS_PREFIX = $(shell brew --prefix quickjs)
PY_PREFIX  = $(shell brew --prefix python@3.14)
PHP_PREFIX = $(shell brew --prefix php)

# --- Include Paths ---
CFLAGS += -I$(LUA_PREFIX)/include/lua \
          -I$(PY_PREFIX)/Frameworks/Python.framework/Versions/3.14/include/python3.14 \
          -I$(PHP_PREFIX)/include/php \
          -I$(PHP_PREFIX)/include/php/main \
          -I$(PHP_PREFIX)/include/php/Zend \
          -I$(PHP_PREFIX)/include/php/TSRM \
          -I$(PHP_PREFIX)/include/php/sapi/cli

# --- Library Paths ---
LDFLAGS += -L/usr/local/lib \
           -L$(LUA_PREFIX)/lib \
           -L$(PY_PREFIX)/Frameworks/Python.framework/Versions/3.14/lib \
           -L$(PHP_PREFIX)/lib \
           -ldl -lpthread -lm -llua -lquickjs -lpython3.14 -lsqlite3

# --- Directory Structure ---
SRC_DIR = src
OBJ_DIR = build
BIN_DIR = bin
TARGET = $(BIN_DIR)/Sera

SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRC))

# --- Build Targets ---
all: $(TARGET)

$(TARGET): $(OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "✅ Build complete: $@"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR)/* $(BIN_DIR)/*

.PHONY: all clean
