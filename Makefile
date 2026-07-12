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

.PHONY: all clean test

all: $(TARGET)

test: $(TARGET)
	./test_battery.sh

$(TARGET): $(OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BINDIR):
	mkdir -p $(BINDIR)

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

clean:
	rm -f $(SRCDIR)/*.o $(TARGET)

# Los objetos dependen de TODAS las cabeceras: sin esto, cambiar un struct en
# include/ deja objetos con layouts distintos y el binario falla de formas
# desconcertantes (pasó al añadir un campo a Tusmodel).
HDRS = $(wildcard include/*.h)
$(OBJS): $(HDRS)
