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
             $(SRC_DIR)/fue_bridge.c \
             $(SRC_DIR)/diagnose_mv.c

ALL_SRC = $(DRVEC_SRC) $(ENGINE_SRC) $(SUITE_SRC)
OBJS    = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(ALL_SRC))

EXEC = $(BIN_DIR)/drvec

# Arnes de pruebas: ensena lo que el lector de .pre ve en un fichero.  Es lo
# unico que ejercita la SERIE y el REFACTOR del .pre -- la estimacion solo usa
# el bloque MA --, ver tests/pre_probe.c.
PROBE      = $(BIN_DIR)/pre_probe
#  Arnes de la chi2 del motor: drvec no llama a chisq() -- usa GSL --, asi que
#  sin esto su bateria no protegeria el arreglo de BUG-13 en un fichero que
#  drvarma y drtran SI usan.  Ver tests/chisq_probe.c.
CHIPROBE   = $(BIN_DIR)/chisq_probe
PROBE_OBJS = $(BUILD_DIR)/fue_pre_reader.o $(BUILD_DIR)/fue_bridge.o \
             $(BUILD_DIR)/nlatools.o

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
# free_fue_pre libera arrays de punteros que el lector reservo con el idioma
# `malloc(n*size) - 1`, asi que el bloque real empieza en p+1.  gcc no ve la
# resta -- esta en otra unidad de compilacion -- y avisa de free-nonheap-object
# sobre un free que es correcto por construccion.  Comprobado con valgrind: 0
# bytes perdidos y 0 errores en las rutas de siembra.  Se silencia SOLO este
# aviso y SOLO en este objeto.
$(BUILD_DIR)/fue_bridge.o: $(SRC_DIR)/fue_bridge.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Wno-free-nonheap-object -c $< -o $@

$(BUILD_DIR)/fue_pre_reader.o: $(SRC_DIR)/fue_pre_reader.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Wno-unused-result -Wno-misleading-indentation -c $< -o $@

$(EXEC): $(OBJS) | $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(PROBE): tests/pre_probe.c $(PROBE_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ tests/pre_probe.c $(PROBE_OBJS) $(LIBS)

$(CHIPROBE): tests/chisq_probe.c $(BUILD_DIR)/nlatools.o | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ tests/chisq_probe.c $(BUILD_DIR)/nlatools.o $(LIBS)

test: $(EXEC) $(PROBE) $(CHIPROBE)
	@tests/run_tests.sh

test-verbose: $(EXEC) $(PROBE) $(CHIPROBE)
	@tests/run_tests.sh -v

#  P6 — INSTALACION.  PREFIX se puede sobreescribir en la linea de ordenes
#  (make install PREFIX=$HOME/.local) y DESTDIR existe para los empaquetadores,
#  que instalan en una raiz falsa antes de hacer el paquete.  Se instala el
#  binario, la pagina de uso no existe todavia, y la documentacion va aparte
#  porque un usuario que solo quiere el programa no quiere 30 ficheros .md.
PREFIX  ?= /usr/local
DESTDIR ?=
BINDIR   = $(DESTDIR)$(PREFIX)/bin
DOCDIR   = $(DESTDIR)$(PREFIX)/share/doc/drvec

install: $(EXEC)
	install -d $(BINDIR)
	install -m 755 $(EXEC) $(BINDIR)/drvec
	install -d $(DOCDIR)
	install -m 644 README.md CHANGELOG.md LICENSE CITATION.cff $(DOCDIR)
	@echo "drvec installed in $(BINDIR); docs in $(DOCDIR)"

uninstall:
	rm -f $(BINDIR)/drvec
	rm -rf $(DOCDIR)

clean:
	rm -rf $(BUILD_DIR)/*.o $(EXEC) $(PROBE) $(CHIPROBE) *.eps *.out *.txt

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

.PHONY: all clean distclean rebuild test test-verbose install uninstall
