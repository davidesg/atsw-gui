# Makefile for DRVEC — VEC model estimation via Mauricio (2006) EML
# Based on drvarma v.04.1 engine.  No GUI, GSL only.
#
# Usage:
#   make          — build drvec
#   make test     — run the regression and invariant suite
#   make clean    — remove objects and binary
#   make rebuild  — distclean + all

CC       = gcc
PKG_CONFIG = pkg-config

GSL_CFLAGS := $(shell $(PKG_CONFIG) --cflags gsl 2>/dev/null)
GSL_LIBS   := $(shell $(PKG_CONFIG) --libs   gsl 2>/dev/null)
ifeq ($(GSL_LIBS),)
    GSL_LIBS = -lgsl -lgslcblas
endif

CFLAGS   = -O2 -g3 -Wall -I./include $(GSL_CFLAGS)
LDFLAGS  =
LIBS     = $(GSL_LIBS) -lm

SRC_DIR     = src
INCLUDE_DIR = include
BUILD_DIR   = build
BIN_DIR     = bin

DRVEC_SRC  = $(SRC_DIR)/drvec.c
ENGINE_SRC = $(SRC_DIR)/elfvarma.c \
             $(SRC_DIR)/drvmlest.c \
             $(SRC_DIR)/qnewtopt.c \
             $(SRC_DIR)/nlatools.c

# Puente con la suite (fue/.pre).  Copiado de drtran; ver docs/PLAN_BETA.md F2.1
SUITE_SRC  = $(SRC_DIR)/fue_pre_reader.c \
             $(SRC_DIR)/fue_bridge.c

ALL_SRC = $(DRVEC_SRC) $(ENGINE_SRC) $(SUITE_SRC)
OBJS    = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(ALL_SRC))

EXEC = $(BIN_DIR)/drvec

all: $(EXEC)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# El lector de .pre es una COPIA de drtran y se mantiene con el minimo delta
# posible (ver docs/PLAN_BETA.md F2.1), asi que sus avisos -- fgets/fscanf sin
# comprobar retorno, indentacion -- son suyos de origen y NO se corrigen aqui:
# tocarlos ampliaria la diferencia con el original y haria mas dificil auditar
# la deriva.  Se silencian solo para este objeto, para que el resto del build
# siga siendo legible.
$(BUILD_DIR)/fue_pre_reader.o: $(SRC_DIR)/fue_pre_reader.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Wno-unused-result -Wno-misleading-indentation -c $< -o $@

$(EXEC): $(OBJS) | $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

test: $(EXEC)
	@tests/run_tests.sh

test-verbose: $(EXEC)
	@tests/run_tests.sh -v

clean:
	rm -rf $(BUILD_DIR)/*.o $(EXEC) *.eps *.out *.txt

distclean: clean
	rm -rf $(BUILD_DIR) $(BIN_DIR)

rebuild: distclean all

# Header dependencies
$(BUILD_DIR)/drvec.o:     $(INCLUDE_DIR)/main.h
$(BUILD_DIR)/elfvarma.o:  $(INCLUDE_DIR)/main.h
$(BUILD_DIR)/drvmlest.o:  $(INCLUDE_DIR)/main.h
$(BUILD_DIR)/nlatools.o:  $(INCLUDE_DIR)/main.h
$(BUILD_DIR)/drvec.o:         $(INCLUDE_DIR)/fue_pre_reader.h \
                              $(INCLUDE_DIR)/fue_bridge.h
$(BUILD_DIR)/fue_pre_reader.o: $(INCLUDE_DIR)/main.h \
                               $(INCLUDE_DIR)/fue_pre_reader.h
$(BUILD_DIR)/fue_bridge.o:    $(INCLUDE_DIR)/main.h \
                              $(INCLUDE_DIR)/fue_bridge.h

.PHONY: all clean distclean rebuild test test-verbose
