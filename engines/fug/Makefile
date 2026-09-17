# FUG 1.15 - el motor. Su GUI (gtk_fmg) esta en gui/fug y tiene su propio
# Makefile: se separaron al montar atsw-gui, y el GUI paso a GTK3.
# for Linux, macOS and Windows
#
#   make                 fug and, if the GTK 2 development files are installed, gtk_fmg
#   make fug             only the engine (needs only a C compiler)
#   make gui             only the GUI (needs GTK 2: libgtk2.0-dev)
#   make windows         fug.exe with MinGW-w64 and gtk_fmg.exe with MXE (if found)
#   make check           regression tests of the engine
#   make check-gui       end-to-end test of the GUI (needs a display)
#   make install         install fug and gtk_fmg in $(PREFIX)/bin (default /usr/local)
#   make clean
#
# fug draws its EPS and PDF files itself (src/fugdraw.c): it needs no other
# program or library (neither gnuplot nor pdflatex). The GUI runs the fug
# that is next to it and shows the graphs in its own window, with the same
# fugdraw code (../../gui/fug/src/preview.c).

CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -Wno-unused-parameter
# fugdraw es el mismo fichero en los cuatro programas: vive en lib/
CFLAGS  += -I../../lib/fugdraw -I../../lib/fugplot
LDLIBS  = -lm
PREFIX  ?= /usr/local

# Windows: MinGW-w64 for fug.exe; MXE (https://mxe.cc) for gtk_fmg.exe,
# since it needs GTK 2 built for Windows
MINGW      ?= x86_64-w64-mingw32-gcc
MXE        ?= $(HOME)/Dropbox/SRC/mxe
MXE_TARGET ?= i686-w64-mingw32.static
MXE_CC      = $(MXE)/usr/bin/$(MXE_TARGET)-gcc
MXE_PKG     = $(MXE)/usr/bin/$(MXE_TARGET)-pkg-config

SRC = src/fug.c src/diagnose.c src/delop.c src/nlutils.c src/port.c \
      ../../lib/fugdraw/fugdraw.c ../../lib/fugplot/fugplot.c src/inpfile.c
HDR = src/fug.h src/delop.h src/port.h ../../lib/fugdraw/fugdraw.h ../../lib/fugplot/fugplot.h ../../lib/fugdraw/fd_metrics.h src/inpfile.h src/plothost.h


ifeq ($(OS),Windows_NT)
  EXE = .exe
  GUI_LDFLAGS = -mwindows
endif

ifneq ($(GTK_LIBS),)
  ALL = fug$(EXE) gtk_fmg$(EXE)
else
  ALL = fug$(EXE) 
endif

all: $(ALL)

gui: gtk_fmg$(EXE)
ifneq ($(EXE),)
fug: fug$(EXE)
endif

fug$(EXE): $(SRC) $(HDR)
	$(CC) $(CFLAGS) -std=c99 -o $@ $(SRC) $(LDLIBS)

windows: fug.exe
	@if [ -x "$(MXE_CC)" ]; then $(MAKE) --no-print-directory gtk_fmg.exe; \
	 else echo "Note: MXE not found in $(MXE): gtk_fmg.exe not built (make windows MXE=/path/to/mxe)."; fi

fug.exe: $(SRC) $(HDR)
	$(MINGW) -O2 -Wall -std=c99 -static -s -o $@ $(SRC) -lm

# GLib runs programs on Windows through gspawn-win32-helper*.exe, which must
# be next to gtk_fmg.exe: copy the four .exe files together
gtk_fmg.exe: $(GUI_SRC) $(GUI_HDR)
	$(MXE_CC) $(GUI_CFLAGS) $(GUI_INC) -mwindows -s -o $@ $(GUI_SRC) \
	    `$(MXE_PKG) --cflags --libs gtk+-2.0` -lm
	cp $(MXE)/usr/$(MXE_TARGET)/bin/gspawn-win32-helper.exe \
	   $(MXE)/usr/$(MXE_TARGET)/bin/gspawn-win32-helper-console.exe .
	$(MXE)/usr/bin/$(MXE_TARGET)-strip gspawn-win32-helper.exe gspawn-win32-helper-console.exe

check: fug$(EXE)
	sh tests/run_tests.sh ./fug$(EXE)

check-gui: fug$(EXE) gtk_fmg$(EXE)
	sh tests/gui/run_gui_test.sh

install: $(ALL)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 fug$(EXE) $(DESTDIR)$(PREFIX)/bin/
	if [ -f gtk_fmg$(EXE) ]; then install -m 755 gtk_fmg$(EXE) $(DESTDIR)$(PREFIX)/bin/; fi

clean:
	rm -f fug fug.exe gtk_fmg gtk_fmg.exe gspawn-win32-helper.exe gspawn-win32-helper-console.exe
	rm -rf tests/work tests/gui/work

.PHONY: all gui  windows check check-gui install clean
