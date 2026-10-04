# Makefile for DRVARMA (command‑line) and DRVARMA GUI
# Works on Linux, macOS, and Windows (MSYS2/MinGW‑w64)
# Also supports cross‑compilation to Windows from Linux using MXE
#   Example: make CROSS=x86_64-w64-mingw32.static-    # 64‑bit static
#            make CROSS=i686-w64-mingw32.static-      # 32‑bit static

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

# Windows‑specific: hide console window
ifeq ($(OS),windows)
    LDFLAGS += -mwindows -Wl,--subsystem,windows
endif


# `make` a secas construye `all`: si no, el primer objetivo del fichero es
# $(GIT_HASH_H) y en un arbol limpio no se construye ningun binario.
.DEFAULT_GOAL := all

# Compiler and basic flags
CFLAGS   = -O2 -g -Wall -I./include -I$(BUILD_DIR) -I$(LIB_DIR)/fuepre -I$(LIB_DIR)/prewhiten -I$(LIB_DIR)/dates -I$(LIB_DIR)/optim -I$(LIB_DIR)/inpcheck -I$(LIB_DIR)/lik
LDFLAGS  +=
LIBS     = -lm

# Directories
SRC_DIR   = src
GUI_DIR   = gui
INCLUDE_DIR = include
BUILD_DIR = build
BIN_DIR   = bin

# Core drvarma sources (all .c files in src/)
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

# Shared with drtran and its GUI (lib/ of the monorepo): the .pre reader,
# the univariate transformations and the date helpers. One source each.
LIB_DIR  = ../../lib
LIB_OBJS = $(BUILD_DIR)/fue_pre_reader.o $(BUILD_DIR)/fuepre_motor.o \
           $(BUILD_DIR)/prewhiten.o $(BUILD_DIR)/dates.o $(BUILD_DIR)/lnsrch.o \
           $(BUILD_DIR)/lik.o $(BUILD_DIR)/multshea.o \
           $(BUILD_DIR)/fuepre_forecast.o $(BUILD_DIR)/inpcheck_fue.o

# GUI source and objects
GUI_SRC        = $(GUI_DIR)/drvarma_gui.c
GUI_OBJ        = $(BUILD_DIR)/gui/drvarma_gui.o
SEASONAL_SRC   = $(GUI_DIR)/seasonal_detection.c
SEASONAL_OBJ   = $(BUILD_DIR)/gui/seasonal_detection.o
JOHANSEN_SRC   = $(GUI_DIR)/johansen_test.c
JOHANSEN_OBJ   = $(BUILD_DIR)/gui/johansen_test.o
VECM_SRC       = $(GUI_DIR)/vecm.c
VECM_OBJ       = $(BUILD_DIR)/gui/vecm.o
GUI_OBJS       = $(GUI_OBJ) $(SEASONAL_OBJ) $(JOHANSEN_OBJ) $(VECM_OBJ)

# Executable names
DRV_EXEC  = $(BIN_DIR)/drvarma$(EXE_EXT)
GUI_EXEC  = $(BIN_DIR)/drvarma_gui$(EXE_EXT)

# pkg-config flags for GTK+3 and GSL
GTK_CFLAGS  := $(shell $(PKG_CONFIG) --cflags gtk+-3.0 2>/dev/null)
GTK_LIBS    := $(shell $(PKG_CONFIG) --libs   gtk+-3.0 2>/dev/null)
GSL_CFLAGS  := $(shell $(PKG_CONFIG) --cflags gsl 2>/dev/null)
GSL_LIBS    := $(shell $(PKG_CONFIG) --libs   gsl 2>/dev/null)

# Fallback in case pkg-config fails
ifeq ($(GTK_CFLAGS),)
    GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0 2>/dev/null || echo "")
    GTK_LIBS   = $(shell pkg-config --libs   gtk+-3.0 2>/dev/null || echo "-lgtk-3 -lgdk-3 -lgobject-2.0 -lglib-2.0")
endif
ifeq ($(GSL_CFLAGS),)
    GSL_CFLAGS = $(shell pkg-config --cflags gsl 2>/dev/null || echo "")
    GSL_LIBS   = $(shell pkg-config --libs   gsl 2>/dev/null || echo "-lgsl -lgslcblas")
endif

# On macOS, pkg-config may need additional paths for Homebrew
ifeq ($(OS),macos)
    PKG_CONFIG_PATH ?= /usr/local/lib/pkgconfig:/opt/homebrew/lib/pkgconfig
    export PKG_CONFIG_PATH
endif

# glib (needed by seasonal_detection.c for g_print), used by both GUI and CLI
GLIB_CFLAGS := $(shell $(PKG_CONFIG) --cflags glib-2.0 2>/dev/null)
GLIB_LIBS   := $(shell $(PKG_CONFIG) --libs   glib-2.0 2>/dev/null)
ifeq ($(GLIB_LIBS),)
    GLIB_LIBS = -lglib-2.0
endif

# Combine GUI flags (include -DDRVARMA_GUI to exclude detect function)
GUI_CFLAGS  = $(CFLAGS) $(GTK_CFLAGS) $(GSL_CFLAGS) -I$(GUI_DIR) -DDRVARMA_GUI
GUI_LIBS    = $(GTK_LIBS) $(GSL_LIBS) -lm

# CLI flags: the command-line engine now reuses gui/seasonal_detection.c
# (harmonic seasonal adjustment), which depends on GSL + glib.
CLI_CFLAGS  = $(CFLAGS) $(GSL_CFLAGS) $(GLIB_CFLAGS) -I$(GUI_DIR)
CLI_LIBS    = $(GSL_LIBS) $(GLIB_LIBS) $(LIBS)

# seasonal_detection.c compiled for the CLI (separate object from the GUI's)
SEASONAL_CLI_OBJ = $(BUILD_DIR)/seasonal_detection.o

# The git commit the binary is built from, for the "Program" line of the .out,
# -version and the GUI title (include/version.h). A version constant alone
# says nothing about which code produced an output (docs/DISENO-interfaz.md,
# "la version del motor no sirve"). The header is rewritten only when the
# hash changes, so a new commit rebuilds exactly what prints it.
GIT_HASH_H = $(BUILD_DIR)/git_hash.h
$(GIT_HASH_H): FORCE | $(BUILD_DIR)
	@h=$$(git describe --always --dirty --abbrev=10 2>/dev/null || echo unknown); \
	 printf '#define DRVARMA_GIT "%s"\n' "$$h" > $@.tmp; \
	 cmp -s $@.tmp $@ && rm -f $@.tmp || mv $@.tmp $@
FORCE:
$(BUILD_DIR)/drvarma.o $(BUILD_DIR)/escalera.o $(GUI_OBJ): $(GIT_HASH_H)

# Default target: build both
all: $(DRV_EXEC) $(GUI_EXEC)

# Create directories if they don't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/gui:
	mkdir -p $(BUILD_DIR)/gui

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Core drvarma (command‑line)
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CLI_CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(LIB_DIR)/fuepre/%.c | $(BUILD_DIR)
	$(CC) $(CLI_CFLAGS) -c $< -o $@
$(BUILD_DIR)/%.o: $(LIB_DIR)/prewhiten/%.c | $(BUILD_DIR)
	$(CC) $(CLI_CFLAGS) -c $< -o $@
$(BUILD_DIR)/%.o: $(LIB_DIR)/dates/%.c | $(BUILD_DIR)
	$(CC) $(CLI_CFLAGS) -c $< -o $@
# fue's own validator decides whether a file is a univariate model of fue:
# the engine's file, renamed on the fly, as the fue GUI compiles it.
$(BUILD_DIR)/inpcheck_fue.o: ../fue/src/inpcheck.c $(LIB_DIR)/inpcheck/inpcheck.h | $(BUILD_DIR)
	$(CC) $(CLI_CFLAGS) -I$(LIB_DIR)/inpcheck -Dinp_check=inp_check_fue -c $< -o $@
$(BUILD_DIR)/%.o: $(LIB_DIR)/lik/%.c | $(BUILD_DIR)
	$(CC) $(CLI_CFLAGS) -c $< -o $@
$(BUILD_DIR)/%.o: $(LIB_DIR)/optim/%.c | $(BUILD_DIR)
	$(CC) $(CLI_CFLAGS) -c $< -o $@

# seasonal_detection.c (from gui/) compiled into the CLI engine
$(SEASONAL_CLI_OBJ): $(SEASONAL_SRC) | $(BUILD_DIR)
	$(CC) $(CLI_CFLAGS) -c $< -o $@

$(DRV_EXEC): $(OBJS) $(LIB_OBJS) $(SEASONAL_CLI_OBJ) | $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(CLI_LIBS)

# GUI drvarma
$(GUI_OBJ): $(GUI_SRC) | $(BUILD_DIR)/gui
	$(CC) $(GUI_CFLAGS) -c $< -o $@

$(SEASONAL_OBJ): $(SEASONAL_SRC) | $(BUILD_DIR)/gui
	$(CC) $(GUI_CFLAGS) -c $< -o $@

$(JOHANSEN_OBJ): $(JOHANSEN_SRC) | $(BUILD_DIR)/gui
	$(CC) $(GUI_CFLAGS) -c $< -o $@

$(VECM_OBJ): $(VECM_SRC) | $(BUILD_DIR)/gui
	$(CC) $(GUI_CFLAGS) -c $< -o $@

$(GUI_EXEC): $(GUI_OBJS) | $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(GUI_LIBS)

# Convenience targets
drvarma: $(DRV_EXEC)
gui: $(GUI_EXEC)

# Clean
clean:
	rm -rf $(BUILD_DIR)/*.o $(BUILD_DIR)/gui/*.o $(DRV_EXEC) $(GUI_EXEC)

distclean: clean
	rm -rf $(BUILD_DIR) $(BIN_DIR)

# Install (optional, only the command‑line program)
install: $(DRV_EXEC)
	cp $(DRV_EXEC) /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/drvarma$(EXE_EXT)

# Help
help:
	@echo "Available targets:"
	@echo "  all       - build both drvarma and drvarma_gui (default)"
	@echo "  drvarma   - build only the command‑line program"
	@echo "  gui       - build only the graphical interface"
	@echo "  clean     - remove object files and executables"
	@echo "  distclean - remove build/ and bin/ directories"
	@echo "  install   - install drvarma to /usr/local/bin"
	@echo "  uninstall - remove drvarma from /usr/local/bin"
	@echo "  help      - show this message"
	@echo ""
	@echo "Cross‑compilation to Windows (static) from Linux using MXE:"
	@echo "  make CROSS=i686-w64-mingw32.static-      # 32‑bit"
	@echo "  make CROSS=x86_64-w64-mingw32.static-    # 64‑bit"
	@echo "  (Ensure the MXE toolchain is in PATH and PKG_CONFIG_PATH is set)"

# Dependencies on headers (simplified)
$(BUILD_DIR)/drvarma.o: $(INCLUDE_DIR)/main.h
$(BUILD_DIR)/elfvarma.o: $(INCLUDE_DIR)/main.h
$(BUILD_DIR)/drvmlest.o: $(INCLUDE_DIR)/main.h
$(BUILD_DIR)/nlatools.o: $(INCLUDE_DIR)/main.h

.PHONY: all drvarma gui clean distclean install uninstall help FORCE
