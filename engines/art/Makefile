# Makefile for art 18.2.1 -- the ARMA/SARIMA identifier (console engine)
# Works on Linux, macOS, and Windows (MSYS2/MinGW-w64)
# Also supports cross-compilation to Windows from Linux using MXE
#   Example: make CROSS=x86_64-w64-mingw32.static-
#
#   bin/art      the engine: a data file in, DATA_art.out and DATA_art.cand out
#   bin/art_cli  ART_18's development CLI (simulation, benchmarks, regression)
#
# The engine sources are in src/, the headers in include/. ART_18's GTK GUI
# did not come with the engine (docs/ESTUDIO-identificador.md): the window is
# lib/analisis's.

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

LIB_DIR = ../../lib
CFLAGS  = -O2 -g -Wall -Iinclude -I$(LIB_DIR)/datos -I$(LIB_DIR)/xlsx -I$(LIB_DIR)/rutas
LDFLAGS +=
LIBS    = -lz -lm

SRC_DIR   = src
BUILD_DIR = obj
BIN_DIR   = bin

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

# lib/: the data door (lib/datos, which reads .xlsx through lib/xlsx) and the
# file names that keep their directory (lib/rutas).
LIB_OBJS = $(BUILD_DIR)/lib_datos.o $(BUILD_DIR)/lib_xlsx.o $(BUILD_DIR)/lib_rutas.o

TARGET     = $(BIN_DIR)/art$(EXE_EXT)
TARGET_DEV = $(BIN_DIR)/art_cli$(EXE_EXT)

GSL_CFLAGS := $(shell $(PKG_CONFIG) --cflags gsl 2>/dev/null)
GSL_LIBS   := $(shell $(PKG_CONFIG) --libs   gsl 2>/dev/null)
ifeq ($(GSL_LIBS),)
    GSL_CFLAGS =
    GSL_LIBS   = -lgsl -lgslcblas
endif
ifeq ($(OS),macos)
    PKG_CONFIG_PATH ?= /usr/local/lib/pkgconfig:/opt/homebrew/lib/pkgconfig
    export PKG_CONFIG_PATH
endif

ALL_CFLAGS = $(CFLAGS) $(GSL_CFLAGS)
ALL_LIBS   = $(GSL_LIBS) $(LIBS)

.PHONY: all dev check clean

all: $(TARGET)

dev: $(TARGET_DEV)

$(TARGET): $(OBJS) $(BUILD_DIR)/art_main.o $(LIB_OBJS) | $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(ALL_LIBS)

$(TARGET_DEV): $(OBJS) $(BUILD_DIR)/main_cli.o | $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(ALL_LIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(ALL_CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/art_main.o: cli/art.c | $(BUILD_DIR)
	$(CC) $(ALL_CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/main_cli.o: cli/main_cli.c | $(BUILD_DIR)
	$(CC) $(ALL_CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/lib_datos.o: $(LIB_DIR)/datos/datos.c | $(BUILD_DIR)
	$(CC) $(ALL_CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/lib_xlsx.o: $(LIB_DIR)/xlsx/xlsx.c | $(BUILD_DIR)
	$(CC) $(ALL_CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/lib_rutas.o: $(LIB_DIR)/rutas/rutas.c | $(BUILD_DIR)
	$(CC) $(ALL_CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR) $(BIN_DIR):
	mkdir -p $@

check: $(TARGET)
	sh tests/run_tests.sh $(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

-include $(wildcard $(BUILD_DIR)/*.d)
