# atsw_gui -- la interfaz madre, GTK+3.
#
# LA FAMILIA se llama atsw y EL PROGRAMA atsw_gui, por el mismo convenio que
# gui/fue -> fue_gui y gui/drtran -> drtran_gui. Un nombre señala una cosa
# sola, y esa regla ya costo un renombrado.
#
# No enlaza ningun motor: la madre LANZA los tres GUIs con --proyecto, no los
# absorbe. Lo unico que comparte con ellos son las bibliotecas de lib/, que es
# justo lo que hace innecesario absorberlos.

CC      ?= cc
CFLAGS  ?= -O2 -g -Wall
PREFIX  ?= /usr/local

LIB = ../../lib

SRC = src/main.c src/ventana.c src/gestos.c src/datos_gui.c src/vistazo.c \
      src/editor.c src/serie_gui.c src/genera.c src/anfitrion.c \
      $(LIB)/proyecto/proyecto.c \
      $(LIB)/outdiag/outdiag.c \
      $(LIB)/outfile/outfile.c \
      $(LIB)/dictamen/dictamen.c \
      $(LIB)/anomalos/anomalos.c \
      $(LIB)/analisis/an_comun.c \
      $(LIB)/analisis/an_anomalos.c \
      $(LIB)/analisis/an_diagnosis.c \
      $(LIB)/analisis/an_sugerir.c \
      $(LIB)/analisis/plotstats.c \
      $(LIB)/intervencion/intervencion.c \
      $(LIB)/inpdet/inpdet.c \
      $(LIB)/engine/engine.c \
      $(LIB)/tabla/tabla.c \
      $(LIB)/rutas/rutas.c \
      $(LIB)/preview/preview.c \
      $(LIB)/utils/ext.c \
      $(LIB)/fugdraw/fugdraw.c \
      $(LIB)/fugplot/fugplot.c \
      $(LIB)/fugplot/plotsupport.c \
      $(LIB)/datos/datos.c \
      $(LIB)/xlsx/xlsx.c \
      ../../engines/fug/src/inpfile.c \
      $(LIB)/dates/dates.c

HDR = include/atsw.h

INC = -Iinclude -I$(LIB)/proyecto -I$(LIB)/outdiag -I$(LIB)/outfile -I$(LIB)/analisis -I$(LIB)/dictamen -I$(LIB)/anomalos -I$(LIB)/intervencion -I$(LIB)/inpdet -I$(LIB)/engine -I$(LIB)/inpcheck -I$(LIB)/tabla \
      -I$(LIB)/rutas -I$(LIB)/dates -I$(LIB)/datos -I$(LIB)/xlsx \
      -I../../engines/fug/src -I$(LIB)/preview -I$(LIB)/fugdraw -I$(LIB)/fugplot -I$(LIB)/utils

GTK_CFLAGS := $(shell pkg-config --cflags gtk+-3.0 2>/dev/null)
GTK_LIBS   := $(shell pkg-config --libs   gtk+-3.0 2>/dev/null)

TARGET = atsw_gui

# inpcheck NO se copia: se compila el fichero DEL MOTOR, renombrando la
# funcion. Lo que el motor acepta es lo que el editor acepta, por
# construccion y no por parecido. Es el mismo convenio que gui/fue.
INPCHECK = inpcheck_fue.o

all: $(TARGET)

$(INPCHECK): ../../engines/fue/src/inpcheck.c $(LIB)/inpcheck/inpcheck.h
	$(CC) $(CFLAGS) $(INC) -I../../engines/fue/include \
	    -Dinp_check=inp_check_fue -c $< -o $@

$(TARGET): $(SRC) $(HDR) $(INPCHECK)
	@if [ -z "$(GTK_LIBS)" ]; then \
	    echo "atsw_gui necesita los ficheros de desarrollo de GTK 3"; \
	    echo "(Debian/Ubuntu: sudo apt install libgtk-3-dev)"; exit 1; fi
	$(CC) $(CFLAGS) $(GTK_CFLAGS) $(INC) -o $@ $(SRC) $(INPCHECK) \
	    $(GTK_LIBS) -lz -lm

install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/

clean:
	rm -f $(TARGET) $(INPCHECK)

.PHONY: all install clean
