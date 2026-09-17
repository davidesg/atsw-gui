# Makefile for FUE GUI (fue_gui)
# Works on Linux, macOS, and Windows (MSYS2/MinGW‑w64)
# Also supports cross‑compilation to Windows from Linux using MXE
#   Example: make CROSS=x86_64-w64-mingw32.static-    # 64‑bit static
#            make CROSS=i686-w64-mingw32.static-      # 32‑bit static
#
# IMPORTANT: All source files (.c) must be placed in the 'src/' directory.
#            Headers (.h) must be placed in the 'include/' directory.

# Detect OS (unless cross‑compiling)
ifdef CROSS
    # Cross‑compilation: force Windows settings
    OS = windows
    EXE_EXT = .exe
    CC = $(CROSS)gcc
    PKG_CONFIG = $(CROSS)pkg-config
    LDFLAGS += -static
    # The user must ensure that the MXE environment is set up,
    # e.g., by having the toolchain in PATH and PKG_CONFIG_PATH
    # pointing to the MXE library directories.
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        OS = linux
        EXE_EXT =
    endif
    ifeq ($(UNAME_S),Darwin)
        OS = macos
        EXE_EXT =
    endif
    ifeq ($(OS),)
        # Assume Windows (MSYS2, Cygwin, etc.)
        OS = windows
        EXE_EXT = .exe
    endif
    CC = gcc
    PKG_CONFIG = pkg-config
endif

# Compiler and basic flags
CFLAGS   = -O2 -g -Wall -Iinclude -I../../lib/fugdraw
LDFLAGS  +=
LIBS     = -lm

# Windows‑specific: hide console window
ifeq ($(OS),windows)
    LDFLAGS += -mwindows -Wl,--subsystem,windows
endif

# Directories
SRC_DIR   = src
BUILD_DIR = obj
BIN_DIR   = bin

# All source files in src/
# La biblioteca compartida. fugdraw.c es el mismo fichero en los cuatro
# programas -- byte a byte -- y por eso vive en lib/ y no aqui.
LIB_DIR  = ../../lib
LIB_SRCS = $(LIB_DIR)/fugdraw/fugdraw.c

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS)) \
       $(patsubst $(LIB_DIR)/%.c,$(BUILD_DIR)/lib_%.o,$(LIB_SRCS))

# Executable name
TARGET = $(BIN_DIR)/fue_gui$(EXE_EXT)

# On macOS: locate Homebrew prefix dynamically (handles both Intel /usr/local
# and Apple Silicon /opt/homebrew), then expose pkg-config and GTK3 paths.
# Must come BEFORE the $(shell pkg-config ...) calls below (:= evaluates immediately).
ifeq ($(OS),macos)
    _BREW := $(shell brew --prefix 2>/dev/null)
    ifeq ($(_BREW),)
        _BREW := $(shell [ -d /opt/homebrew ] && echo /opt/homebrew || echo /usr/local)
    endif
    export PATH            := $(_BREW)/bin:$(PATH)
    export PKG_CONFIG_PATH := $(_BREW)/lib/pkgconfig:$(_BREW)/opt/gtk+3/lib/pkgconfig$(if $(PKG_CONFIG_PATH),:$(PKG_CONFIG_PATH))
    PKG_CONFIG = pkg-config
endif

# pkg‑config flags for GTK+3
GTK_CFLAGS  := $(shell $(PKG_CONFIG) --cflags gtk+-3.0 2>/dev/null)
GTK_LIBS    := $(shell $(PKG_CONFIG) --libs   gtk+-3.0 2>/dev/null)

ifeq ($(GTK_CFLAGS),)
  ifeq ($(OS),macos)
    $(error GTK+3 not found. Install it with:  brew install gtk+3  pkg-config)
  else
    $(error GTK+3 not found. Install libgtk-3-dev (Debian/Ubuntu) or gtk3-devel (Fedora/RHEL))
  endif
endif

# Combine flags
GUI_CFLAGS = $(CFLAGS) $(GTK_CFLAGS)
GUI_LIBS   = $(GTK_LIBS) -lm

# Default target: build GUI
all: $(TARGET)

# Create directories if they don't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Compile each source file
$(BUILD_DIR)/lib_fugdraw/%.o: $(LIB_DIR)/fugdraw/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(ALL_CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(GUI_CFLAGS) -c $< -o $@

# Link
$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(GUI_LIBS)

# Convenience target
gui: $(TARGET)

# Tests (tests/run_tests.sh)
check:
	sh tests/run_tests.sh

# Clean
clean:
	rm -rf $(BUILD_DIR)/*.o $(TARGET) tests/work

distclean: clean
	rm -rf $(BUILD_DIR) $(BIN_DIR) tests/work

# Install (optional)
install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/fue_gui$(EXE_EXT)

# Help
help:
	@echo "Available targets:"
	@echo "  all       - build fue_gui (default)"
	@echo "  gui       - same as all"
	@echo "  check     - run the tests (tests/run_tests.sh)"
	@echo "  clean     - remove object files and executable"
	@echo "  distclean - remove obj/ and bin/ directories"
	@echo "  install   - install fue_gui to /usr/local/bin"
	@echo "  uninstall - remove fue_gui from /usr/local/bin"
	@echo "  help      - show this message"
	@echo ""
	@echo "Cross‑compilation to Windows (static) from Linux using MXE:"
	@echo "  make CROSS=i686-w64-mingw32.static-      # 32‑bit"
	@echo "  make CROSS=x86_64-w64-mingw32.static-    # 64‑bit"
	@echo "  (Ensure the MXE toolchain is in PATH and PKG_CONFIG_PATH is set)"

# Header dependencies (explicit)
$(BUILD_DIR)/data_handling.o: include/data_handling.h include/fue_globals.h include/file_io.h include/model_spec.h include/utils.h
$(BUILD_DIR)/deterministic_dialog.o: include/deterministic_dialog.h include/fue_globals.h include/utils.h
$(BUILD_DIR)/file_io.o: include/file_io.h include/fue_globals.h include/model_spec.h include/utils.h include/forecast_tab.h
$(BUILD_DIR)/forecast_tab.o: include/forecast_tab.h include/fue_globals.h
$(BUILD_DIR)/main.o: include/main_window.h include/fue_globals.h include/data_handling.h include/model_spec.h include/deterministic_dialog.h include/operator_dialog.h include/file_io.h
$(BUILD_DIR)/main_window.o: include/main_window.h include/fue_globals.h include/data_handling.h include/model_spec.h include/file_io.h include/deterministic_dialog.h include/operator_dialog.h include/forecast_tab.h
$(BUILD_DIR)/model_globals.o: include/fue_globals.h include/nlutils.h
$(BUILD_DIR)/model_spec.o: include/model_spec.h include/fue_globals.h
$(BUILD_DIR)/nlutils.o: include/nlutils.h
$(BUILD_DIR)/operator_dialog.o: include/operator_dialog.h include/fue_globals.h include/deterministic_dialog.h include/model_spec.h include/utils.h
$(BUILD_DIR)/utils.o: include/utils.h

.PHONY: all gui check clean distclean install uninstall help
