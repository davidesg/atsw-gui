# fuf_gui -- la ventana de fuf, GTK+3.
#
# SE LLAMA COMO LOS HERMANOS: gui/fue construye fue_gui, gui/drtran
# drtran_gui. El taller tiene un GUI por motor y la madre orquesta.
#
# No enlaza fuf: lo LANZA, como la madre lanza a los demas. Lo que comparte
# con ellos son las bibliotecas de lib/ -- y lib/outfcst es la misma que usa
# gui/drtran para leer SUS previsiones, que es justo lo que evita una tercera
# implementacion de "leer una prevision".

CC      ?= cc
CFLAGS  ?= -O2 -g -Wall
PREFIX  ?= /usr/local

LIB = ../../lib

SRC = src/main.c src/ventana.c src/fufgui.c \
      $(LIB)/outfcst/outfcst.c \
      $(LIB)/engine/engine.c \
      $(LIB)/proyecto/proyecto.c \
      $(LIB)/preview/preview.c \
      $(LIB)/fugdraw/fugdraw.c \
      $(LIB)/utils/ext.c \
      $(LIB)/rutas/rutas.c

HDR = include/fufgui.h

INC = -Iinclude -I$(LIB)/outfcst -I$(LIB)/engine -I$(LIB)/proyecto \
      -I$(LIB)/preview -I$(LIB)/fugdraw -I$(LIB)/utils -I$(LIB)/rutas

GTK_CFLAGS := $(shell pkg-config --cflags gtk+-3.0 2>/dev/null)
GTK_LIBS   := $(shell pkg-config --libs   gtk+-3.0 2>/dev/null)

TARGET = fuf_gui

all: $(TARGET)

$(TARGET): $(SRC) $(HDR)
	@if [ -z "$(GTK_LIBS)" ]; then \
	    echo "fuf_gui necesita los ficheros de desarrollo de GTK 3"; \
	    echo "(Debian/Ubuntu: sudo apt install libgtk-3-dev)"; exit 1; fi
	$(CC) $(CFLAGS) $(GTK_CFLAGS) $(INC) -o $@ $(SRC) $(GTK_LIBS) -lm

install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/

clean:
	rm -f $(TARGET)

.PHONY: all install clean
