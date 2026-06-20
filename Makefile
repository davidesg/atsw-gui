# Makefile for ART (Automatic ARMA/SARIMA Detection)
# Soporta Linux, macOS, Windows (MSYS2/MinGW) y cross‑compilación con MXE
# Compila:
#   - GUI: bin/art_gui (requiere GTK+3 y GSL)
#   - CLI: bin/art_cli (solo requiere GSL)

ifdef CROSS
    OS = windows
    EXE_EXT = .exe
    CC = $(CROSS)gcc
    PKG_CONFIG = $(CROSS)pkg-config
    LDFLAGS += -static
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        OS = linux
        EXE_EXT =
    endif
    ifeq ($(UNAME_S),Darwin)
        OS = macos
        EXE_EXT =
    endif
    ifeq ($(OS),)
        OS = windows
        EXE_EXT = .exe
    endif
    CC = gcc
    PKG_CONFIG = pkg-config
endif

CFLAGS   = -O2 -g -Wall -Iinclude
LDFLAGS  +=
LIBS     = -lm

SRC_DIR   = src
GUI_DIR   = gui
CLI_DIR   = cli
INCLUDE_DIR = include
BUILD_DIR = build
BIN_DIR   = bin

# ---------- Fuentes y objetos del núcleo (sin GUI) ----------
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

# ---------- Fuentes y objetos de la GUI ----------
GUI_SRCS = $(wildcard $(GUI_DIR)/*.c)
GUI_OBJS = $(patsubst $(GUI_DIR)/%.c,$(BUILD_DIR)/gui/%.o,$(GUI_SRCS))

# ---------- Fuente y objeto de la CLI ----------
CLI_SRC = $(CLI_DIR)/main_cli.c
CLI_OBJ = $(BUILD_DIR)/main_cli.o

# ---------- Ejecutables finales ----------
TARGET_GUI = $(BIN_DIR)/art_gui$(EXE_EXT)
TARGET_CLI = $(BIN_DIR)/art_cli$(EXE_EXT)

# ---------- Flags para GTK+3 y GSL ----------
GTK_CFLAGS := $(shell $(PKG_CONFIG) --cflags gtk+-3.0 2>/dev/null)
GTK_LIBS   := $(shell $(PKG_CONFIG) --libs   gtk+-3.0 2>/dev/null)
GSL_CFLAGS := $(shell $(PKG_CONFIG) --cflags gsl 2>/dev/null)
GSL_LIBS   := $(shell $(PKG_CONFIG) --libs   gsl 2>/dev/null)

# Fallbacks por si pkg-config falla
ifeq ($(GTK_CFLAGS),)
    GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0 2>/dev/null || echo "")
    GTK_LIBS   = $(shell pkg-config --libs   gtk+-3.0 2>/dev/null || echo "-lgtk-3 -lgdk-3 -lgobject-2.0 -lglib-2.0")
endif
ifeq ($(GSL_CFLAGS),)
    GSL_CFLAGS = $(shell pkg-config --cflags gsl 2>/dev/null || echo "")
    GSL_LIBS   = $(shell pkg-config --libs   gsl 2>/dev/null || echo "-lgsl -lgslcblas")
endif

ifeq ($(OS),macos)
    PKG_CONFIG_PATH ?= /usr/local/lib/pkgconfig:/opt/homebrew/lib/pkgconfig
    export PKG_CONFIG_PATH
endif

# Flags específicos para la GUI (con GTK)
GUI_CFLAGS = $(CFLAGS) $(GTK_CFLAGS) $(GSL_CFLAGS)
GUI_LIBS   = $(GTK_LIBS) $(GSL_LIBS) -lm

# ---------- Reglas por defecto ----------
all: $(TARGET_GUI)   # Solo GUI por defecto (opcional: añadir $(TARGET_CLI))

# Compilar solo la CLI
cli: $(TARGET_CLI)

# ---------- Crear directorios ----------
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/gui:
	mkdir -p $(BUILD_DIR)/gui

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# ---------- Compilar objetos del núcleo (sin GTK) ----------
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ---------- Compilar objetos de la GUI (con GTK) ----------
$(BUILD_DIR)/gui/%.o: $(GUI_DIR)/%.c | $(BUILD_DIR)/gui
	$(CC) $(GUI_CFLAGS) -c $< -o $@

# ---------- Compilar objeto de la CLI (sin GTK) ----------
$(CLI_OBJ): $(CLI_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ---------- Enlazar ejecutable GUI ----------
$(TARGET_GUI): $(OBJS) $(GUI_OBJS) | $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(GUI_LIBS)

# ---------- Enlazar ejecutable CLI ----------
$(TARGET_CLI): $(OBJS) $(CLI_OBJ) | $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(GSL_LIBS) -lm -lpthread

# ---------- Limpieza ----------
clean:
	rm -rf $(BUILD_DIR)/*.o $(BUILD_DIR)/gui/*.o $(TARGET_GUI) $(TARGET_CLI)

distclean: clean
	rm -rf $(BUILD_DIR) $(BIN_DIR)

# ---------- Instalación (solo GUI, opcional) ----------
install: $(TARGET_GUI)
	cp $(TARGET_GUI) /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/art_gui$(EXE_EXT)

# ---------- Ayuda ----------
help:
	@echo "Targets disponibles:"
	@echo "  all       - compilar la GUI (por defecto)"
	@echo "  cli       - compilar la versión CLI (art_cli)"
	@echo "  clean     - eliminar objetos y ejecutables"
	@echo "  distclean - eliminar build/ y bin/"
	@echo "  install   - instalar art_gui en /usr/local/bin"
	@echo "  uninstall - desinstalar"
	@echo "  help      - mostrar este mensaje"
	@echo ""
	@echo "Compilación cruzada a Windows (estática) desde Linux con MXE:"
	@echo "  make CROSS=i686-w64-mingw32.static-      # 32 bits"
	@echo "  make CROSS=x86_64-w64-mingw32.static-    # 64 bits"
	@echo "  (Asegurar que MXE está en PATH y PKG_CONFIG_PATH apunta a sus librerías)"

.PHONY: all cli clean distclean install uninstall help
