# drtran -- Box-Jenkins transfer function models.
# Copyright (C) 1995-2026 A.B. Treadway, J.A. Mauricio & D.E. Guerrero.
#
# This program is free software: you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 2 of the License, or (at your option) any later
# version.  Distributed WITHOUT ANY WARRANTY; see COPYING for details.

# Makefile for drtran — Box–Jenkins transfer function estimation
# Uses the drvarma exact-VARMA likelihood engine

CC       = gcc
CFLAGS   = -Wall -Wextra -O2 -fPIC
LDFLAGS  = -lm

SRCDIR   = src
INCDIR   = include
BINDIR   = bin

# Source files
SRCS = $(SRCDIR)/drtran.c       \
       $(SRCDIR)/tran_shootx.c  \
       $(SRCDIR)/fue_pre_reader.c \
       $(SRCDIR)/elfvarma.c     \
       $(SRCDIR)/drvmlest.c     \
       $(SRCDIR)/qnewtopt.c     \
       $(SRCDIR)/nlatools.c     \
       $(SRCDIR)/diagnose.c   \
       $(SRCDIR)/forecast.c

OBJS = $(SRCS:.c=.o)

TARGET = $(BINDIR)/drtran

# Destino de la instalación (misma convención que fue y drvarma).
# Sin sudo:  make install PREFIX=$$HOME/.local
PREFIX  ?= /usr/local
DESTDIR ?=

.PHONY: all clean test install uninstall help doc

all: $(TARGET)

test: $(TARGET)
	./test_battery.sh

install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/drtran
	@echo "instalado en $(DESTDIR)$(PREFIX)/bin/drtran"

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/drtran

# La nota tecnica (docs/drtran-note.pdf). Requiere pdflatex.
doc:
	cd docs && pdflatex -interaction=nonstopmode drtran-note.tex >/dev/null \
	        && pdflatex -interaction=nonstopmode drtran-note.tex >/dev/null
	@echo "docs/drtran-note.pdf"

help:
	@echo "drtran — modelos de transferencia Box–Jenkins (ML exacta)"
	@echo ""
	@echo "  make            compila -> bin/drtran"
	@echo "  make test       ejecuta la batería de comprobaciones"
	@echo "  make install    instala en \$$PREFIX/bin  (por defecto /usr/local)"
	@echo "  make uninstall  desinstala"
	@echo "  make doc        compila la nota tecnica -> docs/drtran-note.pdf"
	@echo "  make clean      borra objetos y binario"
	@echo ""
	@echo "  Sin sudo:  make install PREFIX=\$$HOME/.local"

$(TARGET): $(OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BINDIR):
	mkdir -p $(BINDIR)

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

clean:
	rm -f $(SRCDIR)/*.o $(TARGET)
	rm -f docs/*.aux docs/*.log docs/*.out docs/*.toc

# Los objetos dependen de TODAS las cabeceras: sin esto, cambiar un struct en
# include/ deja objetos con layouts distintos y el binario falla de formas
# desconcertantes (pasó al añadir un campo a Tusmodel).
HDRS = $(wildcard include/*.h)
$(OBJS): $(HDRS)
