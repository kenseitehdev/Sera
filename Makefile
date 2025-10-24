# Sera - Polyglot Web Server Makefile
# Project structure:
#   src/       - Source files
#   include/   - Header files
#   build/     - Object files
#   bin/       - Binary output

# Compiler
CC = gcc

# Project name
PROJECT = sera

# Directories
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
BIN_DIR = bin

# Target binary
TARGET = $(BIN_DIR)/$(PROJECT)

# Source files
SOURCES = $(wildcard $(SRC_DIR)/*.c)

# Object files
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Detect OS
UNAME_S := $(shell uname -s)

# Python version detection
PYTHON_VERSION ?= $(shell python3 --version 2>&1 | grep -oP '3\.\d+' | head -1)
ifeq ($(PYTHON_VERSION),)
    PYTHON_VERSION = 3.11
endif

# Base compiler flags
CFLAGS = -Wall -Wextra -O2 -std=c11 -I$(INC_DIR)

# Base linker flags
LDFLAGS = -lpthread -ldl -lm

# Lua flags
LUA_CFLAGS = $(shell pkg-config --cflags lua5.4 2>/dev/null || pkg-config --cflags lua 2>/dev/null || echo "-I/usr/include/lua5.4")
LUA_LDFLAGS = $(shell pkg-config --libs lua5.4 2>/dev/null || pkg-config --libs lua 2>/dev/null || echo "-llua5.4")

# SQLite flags
SQLITE_CFLAGS = $(shell pkg-config --cflags sqlite3 2>/dev/null || echo "")
SQLITE_LDFLAGS = $(shell pkg-config --libs sqlite3 2>/dev/null || echo "-lsqlite3")

# Python flags
PYTHON_CFLAGS = $(shell pkg-config --cflags python-$(PYTHON_VERSION) 2>/dev/null || python3-config --cflags 2>/dev/null || echo "-I/usr/include/python$(PYTHON_VERSION)")
PYTHON_LDFLAGS = $(shell pkg-config --libs python-$(PYTHON_VERSION) 2>/dev/null || python3-config --ldflags 2>/dev/null || echo "-lpython$(PYTHON_VERSION)")

# OS-specific adjustments
ifeq ($(UNAME_S),Darwin)
    # macOS
    CFLAGS += -I/opt/homebrew/include -I/usr/local/include
    LDFLAGS += -L/opt/homebrew/lib -L/usr/local/lib
    # Fix for macOS Python linking
    PYTHON_LDFLAGS := $(shell python3-config --ldflags --embed 2>/dev/null || python3-config --ldflags)
endif

ifeq ($(UNAME_S),Linux)
    # Linux
    LDFLAGS += -Wl,--export-dynamic
endif

# Combine all flags
ALL_CFLAGS = $(CFLAGS) $(LUA_CFLAGS) $(SQLITE_CFLAGS) $(PYTHON_CFLAGS)
ALL_LDFLAGS = $(LDFLAGS) $(LUA_LDFLAGS) $(SQLITE_LDFLAGS) $(PYTHON_LDFLAGS)

# Phony targets
.PHONY: all clean install uninstall setup test debug check-deps help dirs

# Default target
all: check-deps dirs $(TARGET)

# Create directories
dirs:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(INC_DIR)

# Build target
$(TARGET): $(OBJECTS)
	@echo "Linking $(TARGET)..."
	@echo "Python version: $(PYTHON_VERSION)"
	$(CC) $(OBJECTS) -o $(TARGET) $(ALL_LDFLAGS)
	@echo "✓ Build complete: $(TARGET)"

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "Compiling $<..."
	$(CC) $(ALL_CFLAGS) -c $< -o $@

# Debug build
debug: CFLAGS += -g -DDEBUG -O0
debug: clean all
	@echo "✓ Debug build complete"

# Check dependencies
check-deps:
	@echo "Checking dependencies..."
	@command -v $(CC) >/dev/null 2>&1 || { echo "✗ Error: gcc not found"; exit 1; }
	@command -v python3 >/dev/null 2>&1 || { echo "⚠ Warning: python3 not found"; }
	@command -v pkg-config >/dev/null 2>&1 || { echo "⚠ Warning: pkg-config not found, using defaults"; }
	@echo "✓ Dependencies OK"

# Setup project directories
setup:
	@echo "Setting up Sera project structure..."
	mkdir -p public
	mkdir -p public/assets
	mkdir -p scripts/api
	mkdir -p scripts/handlers
	mkdir -p scripts/legacy
	mkdir -p scripts/services
	mkdir -p scripts/admin
	mkdir -p $(INC_DIR)
	mkdir -p $(SRC_DIR)
	mkdir -p $(BUILD_DIR)
	mkdir -p $(BIN_DIR)
	@echo "Creating default configuration..."
	@if [ ! -f server.conf ]; then \
		echo "# Sera Server Configuration" > server.conf; \
		echo "port = 8080" >> server.conf; \
		echo "threads = 4" >> server.conf; \
		echo "" >> server.conf; \
		echo "# Directory paths" >> server.conf; \
		echo "root = ./public" >> server.conf; \
		echo "script_dir = ./scripts" >> server.conf; \
		echo "database = ./data.db" >> server.conf; \
		echo "" >> server.conf; \
		echo "# Runtime binaries" >> server.conf; \
		echo "php_cgi = /usr/bin/php-cgi" >> server.conf; \
		echo "node_bin = /usr/bin/node" >> server.conf; \
		echo "ruby_bin = /usr/bin/ruby" >> server.conf; \
		echo "" >> server.conf; \
		echo "# Dynamic routes with parameters" >> server.conf; \
		echo "route GET /api/users/:id python api/user.py" >> server.conf; \
		echo "route GET /api/data/:table/:id lua handlers/crud.lua" >> server.conf; \
		echo "route GET /api/test python api/test.py" >> server.conf; \
		echo "route GET / static /index.html" >> server.conf; \
		echo "✓ Created server.conf"; \
	fi
	@if [ ! -f public/index.html ]; then \
		echo "<!DOCTYPE html>" > public/index.html; \
		echo "<html><head><meta charset=\"utf-8\"><title>Sera Server</title></head>" >> public/index.html; \
		echo "<body style=\"font-family: system-ui; max-width: 800px; margin: 50px auto; padding: 20px;\">" >> public/index.html; \
		echo "<h1>🚀 Sera Server is Running!</h1>" >> public/index.html; \
		echo "<p>A polyglot web server supporting Python, Lua, PHP, Node.js, Ruby, and SQLite3.</p>" >> public/index.html; \
		echo "<h2>Test Endpoints:</h2>" >> public/index.html; \
		echo "<ul>" >> public/index.html; \
		echo "<li><a href=\"/api/test\">/api/test</a> - Python handler</li>" >> public/index.html; \
		echo "<li><a href=\"/api/users/123\">/api/users/123</a> - Dynamic routing</li>" >> public/index.html; \
		echo "</ul></body></html>" >> public/index.html; \
		echo "✓ Created public/index.html"; \
	fi
	@if [ ! -f scripts/api/test.py ]; then \
		echo "#!/usr/bin/env python3" > scripts/api/test.py; \
		echo "# Sera test endpoint" >> scripts/api/test.py; \
		echo "import json" >> scripts/api/test.py; \
		echo "" >> scripts/api/test.py; \
		echo "data = {" >> scripts/api/test.py; \
		echo "    'status': 'ok'," >> scripts/api/test.py; \
		echo "    'message': 'Sera server is running'," >> scripts/api/test.py; \
		echo "    'server': 'Sera/1.0'" >> scripts/api/test.py; \
		echo "}" >> scripts/api/test.py; \
		echo "" >> scripts/api/test.py; \
		echo "body = json.dumps(data)" >> scripts/api/test.py; \
		echo "response = f'HTTP/1.1 200 OK\\r\\nContent-Type: application/json\\r\\n\\r\\n{body}'" >> scripts/api/test.py; \
		chmod +x scripts/api/test.py; \
		echo "✓ Created scripts/api/test.py"; \
	fi
	@if [ ! -f scripts/api/user.py ]; then \
		echo "#!/usr/bin/env python3" > scripts/api/user.py; \
		echo "# Example with dynamic routing" >> scripts/api/user.py; \
		echo "import json" >> scripts/api/user.py; \
		echo "" >> scripts/api/user.py; \
		echo "# Get route parameter" >> scripts/api/user.py; \
		echo "params = environ.get('route.params', {})" >> scripts/api/user.py; \
		echo "user_id = params.get('id', 'unknown')" >> scripts/api/user.py; \
		echo "" >> scripts/api/user.py; \
		echo "data = {" >> scripts/api/user.py; \
		echo "    'id': user_id," >> scripts/api/user.py; \
		echo "    'name': f'User {user_id}'," >> scripts/api/user.py; \
		echo "    'email': f'user{user_id}@example.com'" >> scripts/api/user.py; \
		echo "}" >> scripts/api/user.py; \
		echo "" >> scripts/api/user.py; \
		echo "body = json.dumps(data)" >> scripts/api/user.py; \
		echo "response = f'HTTP/1.1 200 OK\\r\\nContent-Type: application/json\\r\\n\\r\\n{body}'" >> scripts/api/user.py; \
		chmod +x scripts/api/user.py; \
		echo "✓ Created scripts/api/user.py"; \
	fi
	@echo ""
	@echo "✓ Setup complete! Directory structure:"
	@echo "  $(SRC_DIR)/     - Source code"
	@echo "  $(INC_DIR)/     - Headers"
	@echo "  $(BUILD_DIR)/   - Object files"
	@echo "  $(BIN_DIR)/     - Binary output"
	@echo "  public/         - Static files"
	@echo "  scripts/        - Runtime scripts"

# Install to system
install: $(TARGET)
	@echo "Installing $(PROJECT) to /usr/local/bin..."
	install -m 755 $(TARGET) /usr/local/bin/$(PROJECT)
	@echo "✓ Installation complete"
	@echo "Run with: $(PROJECT) server.conf"

# Uninstall from system
uninstall:
	@echo "Removing $(PROJECT) from /usr/local/bin..."
	rm -f /usr/local/bin/$(PROJECT)
	@echo "✓ Uninstall complete"

# Run the server
run: $(TARGET)
	@echo "Starting Sera server..."
	$(TARGET) server.conf

# Run tests
test: $(TARGET)
	@echo "Starting Sera test server..."
	@$(TARGET) server.conf &
	@sleep 2
	@echo "Testing endpoints..."
	@curl -s http://localhost:8080/ | grep -q "Sera" && echo "✓ Static file serving works" || echo "✗ Static file test failed"
	@curl -s http://localhost:8080/api/test | grep -q "Sera" && echo "✓ Python API works" || echo "✗ Python API test failed"
	@curl -s http://localhost:8080/api/users/42 | grep -q "42" && echo "✓ Dynamic routing works" || echo "✗ Dynamic routing test failed"
	@pkill -f "$(TARGET)" || true
	@echo "✓ Tests complete"

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	rm -rf $(BIN_DIR)
	rm -f core
	@echo "✓ Clean complete"

# Full clean (including generated files)
distclean: clean
	@echo "Removing all generated files..."
	rm -rf public scripts data.db
	@echo "✓ Distribution clean complete"

# Show build configuration
config:
	@echo "Sera Build Configuration:"
	@echo "  Project:         $(PROJECT)"
	@echo "  CC:              $(CC)"
	@echo "  Target:          $(TARGET)"
	@echo "  OS:              $(UNAME_S)"
	@echo "  Python Version:  $(PYTHON_VERSION)"
	@echo ""
	@echo "Directories:"
	@echo "  Source:          $(SRC_DIR)/"
	@echo "  Include:         $(INC_DIR)/"
	@echo "  Build:           $(BUILD_DIR)/"
	@echo "  Binary:          $(BIN_DIR)/"
	@echo ""
	@echo "Compiler Flags:"
	@echo "  $(ALL_CFLAGS)"
	@echo ""
	@echo "Linker Flags:"
	@echo "  $(ALL_LDFLAGS)"

# Help
help:
	@echo "Sera - Polyglot Web Server"
	@echo "=========================="
	@echo ""
	@echo "Usage: make [target]"
	@echo ""
	@echo "Build Targets:"
	@echo "  all          - Build the server (default)"
	@echo "  debug        - Build with debug symbols"
	@echo "  clean        - Remove build artifacts"
	@echo "  distclean    - Remove all generated files"
	@echo ""
	@echo "Setup & Install:"
	@echo "  setup        - Create project directory structure"
	@echo "  install      - Install to /usr/local/bin (requires sudo)"
	@echo "  uninstall    - Remove from /usr/local/bin (requires sudo)"
	@echo ""
	@echo "Running & Testing:"
	@echo "  run          - Build and run the server"
	@echo "  test         - Build and run tests"
	@echo ""
	@echo "Info:"
	@echo "  config       - Show build configuration"
	@echo "  check-deps   - Verify dependencies"
	@echo "  help         - Show this help message"
	@echo ""
	@echo "Environment Variables:"
	@echo "  PYTHON_VERSION - Set Python version (default: auto-detect)"
	@echo "    Example: make PYTHON_VERSION=3.12"
	@echo ""
	@echo "Quick Start:"
	@echo "  make setup              # Setup project structure"
	@echo "  make                    # Build Sera"
	@echo "  make run                # Run the server"
	@echo ""
	@echo "Or manually:"
	@echo "  make && bin/sera server.conf"
