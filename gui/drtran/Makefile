# mtram -- el GUI de drtran, GTK+3.
#
# Lo que hace distinto a este Makefile: NO tiene un lector del .pre. Enlaza el
# del motor -- engines/drtran/src/fue_pre_reader.c -- tal cual, con nlatools
# para sus asignadores y dates de lib/. Lo que ese lector acepte es lo que
# acepta drtran, por construccion y no por parecido.
#
# El estudio del contrato conto SEIS implementaciones del formato univariante,
# y cada divergencia que encontro salia de que el mismo fichero viviera en dos
# sitios. Aqui no se abre la septima.

CC      ?= cc
CFLAGS  ?= -O2 -g -Wall
PREFIX  ?= /usr/local

LIB = ../../lib
ENG = ../../engines/drtran

SRC = src/main.c src/main_window.c src/series.c \
      $(ENG)/src/fue_pre_reader.c \
      $(ENG)/src/nlatools.c \
      $(LIB)/dates/dates.c

HDR = include/gui.h include/series.h

INC = -Iinclude -I$(ENG)/include -I$(LIB)/dates

GTK_CFLAGS := $(shell pkg-config --cflags gtk+-3.0 2>/dev/null)
GTK_LIBS   := $(shell pkg-config --libs   gtk+-3.0 2>/dev/null)
# nlatools usa GSL para los autovalores
GSL_LIBS   := $(shell pkg-config --libs gsl 2>/dev/null)
ifeq ($(GSL_LIBS),)
  GSL_LIBS = -lgsl -lgslcblas
endif

TARGET = mtram

all: $(TARGET)

$(TARGET): $(SRC) $(HDR)
	@if [ -z "$(GTK_LIBS)" ]; then \
	    echo "mtram necesita los ficheros de desarrollo de GTK 3"; \
	    echo "(Debian/Ubuntu: sudo apt install libgtk-3-dev)"; exit 1; fi
	$(CC) $(CFLAGS) $(GTK_CFLAGS) $(INC) -o $@ $(SRC) $(GTK_LIBS) $(GSL_LIBS) -lm

install: $(TARGET)
	install -d $(PREFIX)/bin
	install -m 755 $(TARGET) $(PREFIX)/bin/

clean:
	rm -f $(TARGET)

.PHONY: all install clean

check:
	sh tests/run_tests.sh

.PHONY: check
