# atsw-cli -- la puerta sin ventana (docs/ESTUDIO-terminal-mcp.md §2.2).
#
# Se compila con las MISMAS bibliotecas de lib/ que la madre, sin GTK: solo
# GLib (y GSL, por las raices). Lo que la madre calcula en una ventana, esto
# lo calcula en una orden, con la misma funcion.

CC      ?= cc
CFLAGS  ?= -O2 -g -Wall

LIB = ../../lib

SRC = src/main.c \
      $(LIB)/proyecto/proyecto.c \
      $(LIB)/gesto/gesto.c \
      $(LIB)/outfile/outfile.c \
      $(LIB)/dictamen/dictamen.c \
      $(LIB)/intervencion/intervencion.c \
      $(LIB)/inpdet/inpdet.c \
      $(LIB)/raices/factor.c \
      $(LIB)/raices/parte.c \
      $(LIB)/coste/coste.c \
      $(LIB)/engine/engine.c \
      $(LIB)/sitio/sitio.c \
      $(LIB)/ficha/ficha.c

INC = -I$(LIB)/proyecto -I$(LIB)/gesto -I$(LIB)/outfile -I$(LIB)/dictamen -I$(LIB)/intervencion \
      -I$(LIB)/inpdet -I$(LIB)/raices -I$(LIB)/inpcheck -I$(LIB)/coste \
      -I$(LIB)/engine -I$(LIB)/sitio -I$(LIB)/ficha

GLIB_CFLAGS := $(shell pkg-config --cflags glib-2.0 2>/dev/null)
GLIB_LIBS   := $(shell pkg-config --libs glib-2.0 2>/dev/null)
GSL_CFLAGS  := $(shell pkg-config --cflags gsl 2>/dev/null)
GSL_LIBS    ?= $(shell pkg-config --libs gsl 2>/dev/null)
ifeq ($(strip $(GSL_LIBS)),)
  GSL_LIBS = -lgsl -lgslcblas
endif

ATSW_VERSION ?= $(shell git describe --tags --always 2>/dev/null)
ifneq ($(strip $(ATSW_VERSION)),)
  VERFLAGS = -DATSW_VERSION='"$(ATSW_VERSION)"'
endif

TARGET = atsw-cli

# inpcheck NO se copia: el fichero DEL MOTOR, renombrando la funcion, como
# en la madre. Lo que fue acepta es lo que se acepta aqui.
INPCHECK = inpcheck_fue.o

all: $(TARGET)

# Las rutas en UTF-8 en Windows (y el icono), como los GUIs.
include ../../lib/windows/utf8.mk

$(INPCHECK): ../../engines/fue/src/inpcheck.c $(LIB)/inpcheck/inpcheck.h
	$(CC) $(CFLAGS) $(INC) -I../../engines/fue/include \
	    -Dinp_check=inp_check_fue -c $< -o $@

$(TARGET): $(SRC) $(wildcard $(LIB)/*/*.h) $(INPCHECK) $(UTF8_RES)
	@if [ -z "$(GLIB_LIBS)" ]; then \
	    echo "atsw-cli necesita los ficheros de desarrollo de GLib"; exit 1; fi
	$(CC) $(CFLAGS) $(VERFLAGS) $(GLIB_CFLAGS) $(GSL_CFLAGS) $(INC) -o $@ $(SRC) \
	    $(INPCHECK) $(UTF8_RES) $(GLIB_LIBS) $(GSL_LIBS) -lm

install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/

clean:
	rm -f $(TARGET) $(INPCHECK)

check: $(TARGET)
	sh tests/run_tests.sh

.PHONY: all install clean check
