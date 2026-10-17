# drtran_gui -- el GUI de drtran, GTK+3.
#
# SE LLAMA COMO SU MOTOR, no como el asistente. "mtram" es el nombre del
# SERVIDOR MCP de drtran-python (drtran.mcp_server:main), que pip instala en
# ~/.local/bin/mtram; llamar igual al binario de GTK hacia que una instalacion
# lo machacara sin avisar. El convenio del monorepo ya existia y es este:
# gui/fue construye fue_gui.
#
# Lo que hace distinto a este Makefile: NO tiene un lector del .pre. Enlaza el
# compartido -- lib/fuepre/fue_pre_reader.c -- tal cual, con nlatools
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

# diagnose.c entra por Ccf, Mean, Stdev y hosking_test: el estadistico de la
# identificacion es el que ya calcula el motor, no uno parecido.
SRC = src/main.c src/main_window.c src/barra_gui.c src/series.c src/identifica.c src/red.c src/modelo.c src/estima.c src/diagnosis.c src/prevision.c src/pagina.c src/plotshim.c src/proyecto_gui.c src/vuelta_gui.c \
      $(LIB)/fuepre/fue_pre_reader.c \
      $(ENG)/src/nlatools.c \
      $(ENG)/src/diagnose.c \
      $(LIB)/diagmv/diagmv.c \
      $(LIB)/prewhiten/prewhiten.c \
      $(LIB)/netfile/netfile.c \
      $(LIB)/nsop/nsop.c \
      $(LIB)/slots/slots.c \
      $(LIB)/verdict/verdict.c \
      $(LIB)/outdiag/outdiag.c \
      $(LIB)/gof/gof.c \
      $(LIB)/tabla/tabla.c \
      $(LIB)/proyecto/proyecto.c \
      $(LIB)/rutas/rutas.c \
      $(LIB)/outfcst/outfcst.c \
      $(LIB)/engine/engine.c \
      $(LIB)/ccfplot/ccfplot.c \
      $(LIB)/fugplot/fugplot.c \
      $(LIB)/fugplot/plotsupport.c \
      $(LIB)/fugdraw/fugdraw.c \
      $(LIB)/equation/eqtran.c \
      $(LIB)/preview/preview.c \
      $(LIB)/utils/ext.c \
      $(LIB)/dates/dates.c \
      $(LIB)/sitio/sitio.c \
      $(LIB)/barra/iconos.c \
      $(LIB)/barra/barra.c \
      $(LIB)/vuelta/vuelta.c \
      $(LIB)/inpdet/inpdet.c

HDR = include/gui.h include/series.h include/previewhost.h include/plothost.h

INC = -Iinclude -I$(ENG)/include -I$(LIB)/fuepre \
      -I$(LIB)/dates -I$(LIB)/prewhiten -I$(LIB)/netfile -I$(LIB)/nsop -I$(LIB)/slots -I$(LIB)/intervencion -I$(LIB)/verdict -I$(LIB)/outdiag -I$(LIB)/gof -I$(LIB)/tabla -I$(LIB)/rutas -I$(LIB)/proyecto -I$(LIB)/outfcst -I$(LIB)/engine -I$(LIB)/ccfplot -I$(LIB)/fugdraw -I$(LIB)/fugplot \
      -I$(LIB)/equation -I$(LIB)/preview -I$(LIB)/utils -I$(LIB)/inpcheck -I$(LIB)/lik -I$(LIB)/diagmv -I$(LIB)/sitio -I$(LIB)/barra -I$(LIB)/vuelta -I$(LIB)/inpdet

GTK_CFLAGS := $(shell pkg-config --cflags gtk+-3.0 2>/dev/null)
GTK_LIBS   := $(shell pkg-config --libs   gtk+-3.0 2>/dev/null)
# nlatools usa GSL para los autovalores
GSL_CFLAGS := $(shell pkg-config --cflags gsl 2>/dev/null)
GSL_LIBS   := $(shell pkg-config --libs gsl 2>/dev/null)
ifeq ($(GSL_LIBS),)
  GSL_LIBS = -lgsl -lgslcblas
endif

TARGET = drtran_gui

# En Windows, sin -mwindows cada GUI abre ademas una consola negra. gui/fug y
# gui/fue ya lo llevaban; faltaba en estos dos, y se veia al empaquetar.
ifeq ($(OS),Windows_NT)
  LDFLAGS += -mwindows
endif

all: $(TARGET)

# Las rutas en UTF-8 en Windows (lib/windows/utf8.mk). Despues de «all»: si
# no, su regla seria el objetivo por omision.
include ../../lib/windows/utf8.mk


# LAS CABECERAS DE lib/ TAMBIEN SON DEPENDENCIAS, y faltaban. Compilar de
# una sola llamada no es lo mismo que decidir bien CUANDO hay que compilar:
# make lo decide mirando las dependencias, y cambiar un valor en una
# cabecera de lib/ no reconstruia nada. Sin aviso y sin error. Ver la nota
# larga en gui/atsw/Makefile.
LIBHDR = $(wildcard $(LIB)/*/*.h)

# LA PUERTA DE fue NO SE COPIA: se compila el fichero del motor, aparte,
# porque el -D que le pone nombre solo puede tocar a ese fichero (como en
# gui/fue). lib/vuelta pasa por ella todo lo que escribe.
INPCHECK = inpcheck_fue.o

$(INPCHECK): ../../engines/fue/src/inpcheck.c $(LIB)/inpcheck/inpcheck.h
	$(CC) $(CFLAGS) -I$(LIB)/inpcheck -I../../engines/fue/include -Dinp_check=inp_check_fue -c $< -o $@

$(TARGET): $(SRC) $(HDR) $(LIBHDR) $(UTF8_RES) $(INPCHECK)
	@if [ -z "$(GTK_LIBS)" ]; then \
	    echo "drtran_gui necesita los ficheros de desarrollo de GTK 3"; \
	    echo "(Debian/Ubuntu: sudo apt install libgtk-3-dev)"; exit 1; fi
	$(CC) $(CFLAGS) $(GTK_CFLAGS) $(GSL_CFLAGS) $(INC) $(LDFLAGS) -o $@ $(SRC) $(INPCHECK) $(UTF8_RES) $(GTK_LIBS) $(GSL_LIBS) -lm

install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/

clean:
	rm -f $(TARGET) $(INPCHECK)

.PHONY: all install clean

check:
	sh tests/run_tests.sh

# Solo la ventana conducida desde el codigo (run_tests.sh tambien la llama al
# final). Necesita servidor grafico y drtran en el PATH o en engines/drtran/bin.
check-gui:
	sh tests/run_gui_tests.sh

.PHONY: check check-gui
