# gtk_fmg -- el GUI de fug, GTK+3.
#
# Antes se construia desde el Makefile del motor, porque motor y GUI vivian en
# el mismo arbol. Ahora el GUI esta en gui/fug y tiene el suyo; el motor sigue
# en engines/fug y se construye aparte.
#
#   make            gtk_fmg
#   make clean
#   make install    en $(PREFIX)/bin   (por omision /usr/local)
#
# Lo compartido viene de lib/ -- preview.c es LITERALMENTE el mismo fichero que
# usa el GUI de fue, con su zoom y su lupa; lo unico propio de fug es el
# previewhost.h de include/. Y fugdraw es el mismo de los cinco programas.

CC      ?= cc
CFLAGS  ?= -O2 -g -Wall
PREFIX  ?= /usr/local

LIB = ../../lib
ENG = ../../engines/fug

SRC = src/main.c src/callbacks.c src/data_load.c src/nlutils.c src/fug_run.c \
      $(LIB)/preview/preview.c $(LIB)/fugdraw/fugdraw.c $(LIB)/datos/datos.c \
      $(LIB)/proyecto/proyecto.c \
      $(ENG)/src/inpfile.c

HDR = include/callbacks.h include/data_load.h include/fug_run.h include/gui.h \
      include/nlutils.h include/previewhost.h \
      $(LIB)/preview/preview.h $(LIB)/fugdraw/fugdraw.h \
      $(LIB)/fugdraw/fd_metrics.h $(ENG)/src/inpfile.h

INC = -Iinclude -I$(LIB)/preview -I$(LIB)/fugdraw -I$(LIB)/datos -I$(LIB)/proyecto -I$(ENG)/src

GTK_CFLAGS := $(shell pkg-config --cflags gtk+-3.0 2>/dev/null)
GTK_LIBS   := $(shell pkg-config --libs   gtk+-3.0 2>/dev/null)

ifeq ($(OS),Windows_NT)
  EXE = .exe
  LDFLAGS += -mwindows
endif

TARGET = gtk_fmg$(EXE)

all: $(TARGET)

$(TARGET): $(SRC) $(HDR)
	@if [ -z "$(GTK_LIBS)" ]; then \
	    echo "El GUI necesita los ficheros de desarrollo de GTK 3"; \
	    echo "(Debian/Ubuntu: sudo apt install libgtk-3-dev)"; \
	    exit 1; fi
	$(CC) $(CFLAGS) $(GTK_CFLAGS) $(INC) $(LDFLAGS) -o $@ $(SRC) $(GTK_LIBS) -lm

install: $(TARGET)
	install -d $(PREFIX)/bin
	install -m 755 $(TARGET) $(PREFIX)/bin/

clean:
	rm -f $(TARGET)

.PHONY: all install clean
