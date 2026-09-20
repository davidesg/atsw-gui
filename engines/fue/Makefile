# Makefile for FUE 1.13.1 (console engine)
# Works on Linux, macOS, and Windows (MSYS2/MinGW-w64)
# Also supports cross-compilation to Windows from Linux using MXE
#   Example: make CROSS=x86_64-w64-mingw32.static-    # 64-bit static
#            make CROSS=i686-w64-mingw32.static-      # 32-bit static
#
# Source files (.c) must be in src/, headers (.h) in include/.

# Detect OS (unless cross-compiling)
ifdef CROSS
    OS         = windows
    EXE_EXT    = .exe
    CC         = $(CROSS)gcc
    PKG_CONFIG = $(CROSS)pkg-config
    LDFLAGS   += -static
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        OS      = linux
        EXE_EXT =
    endif
    ifeq ($(UNAME_S),Darwin)
        OS      = macos
        EXE_EXT =
    endif
    ifeq ($(OS),)
        OS      = windows
        EXE_EXT = .exe
    endif
    CC         = gcc
    PKG_CONFIG = pkg-config
endif

# Compiler flags
CFLAGS  = -O2 -g -Wall -Iinclude -I../../lib/fugdraw -I../../lib/fugplot -I../../lib/equation -I../../lib/rutas
LDFLAGS +=
LIBS    = -lm

# Directories
SRC_DIR   = src
BUILD_DIR = obj
BIN_DIR   = bin

# Source and object files
# La biblioteca compartida. fugdraw.c es el mismo fichero en los cuatro
# programas -- byte a byte -- y por eso vive en lib/ y no aqui.
LIB_DIR  = ../../lib
LIB_SRCS = $(LIB_DIR)/fugdraw/fugdraw.c $(LIB_DIR)/fugplot/fugplot.c \
           $(LIB_DIR)/rutas/rutas.c \
           $(LIB_DIR)/fugplot/plotsupport.c \
           $(LIB_DIR)/equation/eqlatex.c

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS)) \
       $(patsubst $(LIB_DIR)/%.c,$(BUILD_DIR)/lib_%.o,$(LIB_SRCS))

# Executable name
TARGET = $(BIN_DIR)/fue$(EXE_EXT)

# GSL flags via pkg-config
GSL_CFLAGS := $(shell $(PKG_CONFIG) --cflags gsl 2>/dev/null)
GSL_LIBS   := $(shell $(PKG_CONFIG) --libs   gsl 2>/dev/null)

# Fallback if pkg-config fails
ifeq ($(GSL_LIBS),)
    GSL_CFLAGS =
    GSL_LIBS   = -lgsl -lgslcblas
endif

# On macOS, Homebrew puts pkg-config files in non-standard locations
ifeq ($(OS),macos)
    PKG_CONFIG_PATH ?= /usr/local/lib/pkgconfig:/opt/homebrew/lib/pkgconfig
    export PKG_CONFIG_PATH
endif

# Combined flags
ALL_CFLAGS = $(CFLAGS) $(GSL_CFLAGS)
ALL_LIBS   = $(GSL_LIBS) $(LIBS)

# Default target
all: $(TARGET)

# Create directories if needed
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Compile
$(BUILD_DIR)/lib_equation/%.o: $(LIB_DIR)/equation/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(ALL_CFLAGS) -c $< -o $@

$(BUILD_DIR)/lib_fugplot/%.o: $(LIB_DIR)/fugplot/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(ALL_CFLAGS) -c $< -o $@

$(BUILD_DIR)/lib_rutas/%.o: $(LIB_DIR)/rutas/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(ALL_CFLAGS) -c $< -o $@

$(BUILD_DIR)/lib_fugdraw/%.o: $(LIB_DIR)/fugdraw/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(ALL_CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(ALL_CFLAGS) -c $< -o $@

# Link
$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(ALL_LIBS)

# Regression tests (tests/run_tests.sh; see tests/README.md)
check: $(TARGET)
	sh tests/run_tests.sh $(TARGET)

# Clean
clean:
	rm -rf $(BUILD_DIR)/*.o $(TARGET)

distclean: clean
	rm -rf $(BUILD_DIR) $(BIN_DIR)

# Install
install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/fue$(EXE_EXT)

# Help
help:
	@echo "Available targets:"
	@echo "  all       - build fue (default)"
	@echo "  clean     - remove object files and executable"
	@echo "  distclean - remove obj/ and bin/ directories"
	@echo "  check     - regression tests (tests/run_tests.sh)"
	@echo "  install   - install fue to /usr/local/bin"
	@echo "  uninstall - remove fue from /usr/local/bin"
	@echo "  help      - show this message"
	@echo ""
	@echo "Cross-compilation to Windows (static) from Linux using MXE:"
	@echo "  make CROSS=i686-w64-mingw32.static-      # 32-bit"
	@echo "  make CROSS=x86_64-w64-mingw32.static-    # 64-bit"
	@echo "  (Ensure the MXE toolchain is in PATH and PKG_CONFIG_PATH is set)"

# Header dependencies (explicit)
$(BUILD_DIR)/fue.o:             include/fue.h include/nlatools.h $(LIB_DIR)/fugplot/fugplot.h $(LIB_DIR)/fugdraw/fugdraw.h include/plothost.h include/inpcheck.h $(LIB_DIR)/equation/equation.h include/report.h
$(BUILD_DIR)/inpcheck.o:        include/inpcheck.h
$(BUILD_DIR)/equation.o:        $(LIB_DIR)/equation/equation.h include/fue.h include/nlatools.h
$(BUILD_DIR)/eqlatex.o:         $(LIB_DIR)/equation/equation.h
$(BUILD_DIR)/report.o:          include/report.h $(LIB_DIR)/equation/equation.h $(LIB_DIR)/fugdraw/fugdraw.h include/plothost.h
$(BUILD_DIR)/diagnose.o:        include/fue.h include/nlatools.h
$(BUILD_DIR)/fugdraw.o:         $(LIB_DIR)/fugdraw/fugdraw.h $(LIB_DIR)/fugdraw/fd_metrics.h
$(BUILD_DIR)/lib_fugplot/fugplot.o: $(LIB_DIR)/fugplot/fugplot.h $(LIB_DIR)/fugdraw/fugdraw.h include/plothost.h include/fue.h include/nlatools.h
$(BUILD_DIR)/lib_fugplot/plotsupport.o: include/plothost.h include/fue.h include/nlatools.h
$(BUILD_DIR)/lib_rutas/rutas.o: $(LIB_DIR)/rutas/rutas.h
$(BUILD_DIR)/nlatools.o:        include/nlatools.h
$(BUILD_DIR)/drvmlest.o:        include/fue.h include/nlatools.h
$(BUILD_DIR)/elfvarma.o:        include/fue.h include/nlatools.h
$(BUILD_DIR)/qnewtopt.o:        include/fue.h include/nlatools.h
$(BUILD_DIR)/usmelard.o:        include/fue.h include/nlatools.h

.PHONY: all check clean distclean install uninstall help
