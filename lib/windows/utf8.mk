# LAS RUTAS EN UTF-8 EN WINDOWS. Lo incluyen los GUIs:
#
#   include ../../lib/windows/utf8.mk
#   ... y $(UTF8_RES) en el enlazado (vacio fuera de Windows)
#
# GTK da las rutas en UTF-8, y en Windows fopen() las lee en la pagina de
# codigos del sistema: un fichero dentro de «Econometría» o de «C:\Users\Iñaki»
# no se abria. Con el manifiesto de utf8.manifest (activeCodePage, Windows 10
# 1903 o posterior) la pagina de codigos del proceso es UTF-8 y todas las
# funciones de C entienden esas rutas, sin tocar los 45 fopen de gui/ y lib/.
#
# El mismo recurso lleva el icono de ATSW (atsw.ico): cada GUI lo tiene en
# el explorador y en la barra de tareas.
#
# Los motores no lo llevan ni les hace falta: reciben las rutas en la pagina
# de codigos del sistema, que tiene todas las letras del espanol.
UTF8_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
UTF8_RES :=
ifneq ($(findstring MINGW,$(shell uname -s))$(findstring MSYS,$(shell uname -s)),)
UTF8_RES := $(UTF8_DIR)utf8.o
$(UTF8_RES): $(UTF8_DIR)utf8.rc $(UTF8_DIR)utf8.manifest $(UTF8_DIR)atsw.ico
	cd $(UTF8_DIR) && windres utf8.rc -O coff -o utf8.o
endif
