# sim_gui -- el GUI de los sistemas simultaneos (SIM), GTK+3.
# docs/DISENO-sim.md. VARMA con drvarma; mas adelante, VEC con drvec.
#
# SE LLAMA POR EL MENU DE LA MADRE, no por un motor (lleva dos), y no
# «sima», que es el servidor MCP de sima-python.
#
# Como drtran_gui, NO tiene un lector del .pre: lib/sistema lee con
# lib/fuepre/fue_pre_reader.c y con el nlatools de drvarma. Lo que acepta la
# pagina 1 es lo que acepta el motor, por construccion.

CC      ?= cc
CFLAGS  ?= -O2 -g -Wall
PREFIX  ?= /usr/local

LIB = ../../lib
ENG = ../../engines/drvarma

SRC = src/main.c src/ventana.c src/series.c src/puerta.c src/rango.c src/restricciones.c src/proyecto.c \
      src/identifica.c src/modelo.c src/estima.c src/diagnosis.c src/pagina.c src/plotshim.c src/prevision.c src/estructura.c \
      $(LIB)/sistema/sistema.c \
      $(LIB)/outvarma/outvarma.c $(LIB)/outvec/outvec.c \
      $(LIB)/outdiag/outdiag.c \
      $(LIB)/fuepre/fue_pre_reader.c \
      $(LIB)/fuedet/fuedet.c \
      $(LIB)/dates/dates.c \
      $(ENG)/src/nlatools.c \
      $(ENG)/src/diagnose.c \
      $(LIB)/diagmv/diagmv.c \
      $(LIB)/proyecto/proyecto.c \
      $(LIB)/engine/engine.c \
      $(LIB)/coste/coste.c \
      $(LIB)/sitio/sitio.c \
      $(LIB)/barra/iconos.c \
      $(LIB)/barra/barra.c \
      $(LIB)/preview/preview.c \
      $(LIB)/fugdraw/fugdraw.c \
      $(LIB)/ccfplot/ccfplot.c \
      $(LIB)/utils/ext.c \
      $(LIB)/fugplot/fugplot.c \
      $(LIB)/fugplot/plotsupport.c \
      $(LIB)/equation/eqdraw.c \
      $(LIB)/cuaderno/cuaderno.c \
      $(LIB)/tabla/tabla.c \
      $(LIB)/fufplot/fufplot.c \
      $(LIB)/outfcst/outfcst.c \
      $(LIB)/outfcst/ofgraf.c

HDR = include/sim.h include/previewhost.h include/plothost.h

INC = -Iinclude -I$(ENG)/include -I$(LIB)/sistema -I$(LIB)/outvarma -I$(LIB)/outvec -I$(LIB)/outdiag \
      -I$(LIB)/fuepre -I$(LIB)/fuedet -I$(LIB)/dates -I$(LIB)/lik -I$(LIB)/optim \
      -I$(LIB)/diagmv -I$(LIB)/prewhiten -I$(LIB)/inpcheck -I$(LIB)/proyecto \
      -I$(LIB)/engine -I$(LIB)/coste -I$(LIB)/sitio -I$(LIB)/barra \
      -I$(LIB)/preview -I$(LIB)/fugdraw -I$(LIB)/ccfplot -I$(LIB)/utils \
      -I$(LIB)/fugplot -I$(LIB)/equation -I$(LIB)/cuaderno -I$(LIB)/tabla -I$(LIB)/fufplot -I$(LIB)/outfcst

GTK_CFLAGS := $(shell pkg-config --cflags gtk+-3.0 2>/dev/null)
GTK_LIBS   := $(shell pkg-config --libs   gtk+-3.0 2>/dev/null)
GSL_CFLAGS := $(shell pkg-config --cflags gsl 2>/dev/null)
GSL_LIBS   := $(shell pkg-config --libs gsl 2>/dev/null)
ifeq ($(GSL_LIBS),)
  GSL_LIBS = -lgsl -lgslcblas
endif

TARGET = sim_gui

# En Windows, sin -mwindows cada GUI abre ademas una consola negra.
ifeq ($(OS),Windows_NT)
  LDFLAGS += -mwindows
endif

all: $(TARGET)

include ../../lib/windows/utf8.mk

LIBHDR = $(wildcard $(LIB)/*/*.h)

$(TARGET): $(SRC) $(HDR) $(LIBHDR) $(UTF8_RES)
	@if [ -z "$(GTK_LIBS)" ]; then \
	    echo "sim_gui necesita los ficheros de desarrollo de GTK 3"; \
	    echo "(Debian/Ubuntu: sudo apt install libgtk-3-dev)"; exit 1; fi
	$(CC) $(CFLAGS) $(GTK_CFLAGS) $(GSL_CFLAGS) $(INC) $(LDFLAGS) -o $@ $(SRC) $(UTF8_RES) $(GTK_LIBS) $(GSL_LIBS) -lm

install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/

clean:
	rm -f $(TARGET)

check:
	sh tests/run_tests.sh

.PHONY: all install clean check
