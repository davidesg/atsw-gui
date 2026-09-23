# ATSW -- construir el taller entero.
#
# POR QUE EXISTE ESTE FICHERO, que antes no estaba.
#
# Los cuatro GUIs enlazan las MISMAS bibliotecas de lib/ -- lib/proyecto la
# primera. Cuando el manifiesto gano una seccion nueva, gui/fug se quedo sin
# recompilar: su lector, de dos dias antes, rechazaba la clave que no conocia
# y el programa se cerraba nada mas arrancar. La madre lo lanzaba, no pasaba
# nada, y no habia forma de ver por que.
#
# Un cambio en lib/ afecta a todo lo que lo enlaza, asi que tiene que haber
# UNA orden que lo construya todo. Si no la hay, "funciona en mi copia" es
# cuestion de tiempo.
#
#   make          construye motores y GUIs
#   make check    pasa las baterias
#   make clean

MOTORES = engines/fue engines/fuf engines/fug engines/drtran engines/drvarma
GUIS    = gui/fue gui/fug gui/drtran gui/atsw

# Las baterias, en el orden en que conviene leerlas: primero los motores
# --si el motor no cumple, lo de arriba no significa nada-- y despues los
# GUIs. La de gui/drtran lleva ademas las de lib/.
BANCOS  = engines/fue engines/fuf engines/fug gui/fue gui/drtran

all: motores guis

motores:
	@for d in $(MOTORES); do \
	    printf '== %s\n' "$$d"; \
	    $(MAKE) -s -C $$d || exit 1; \
	done

# LOS GUIS DESPUES DE LOS MOTORES, y no por gusto: el editor del .inp corre
# fue, y la madre busca a los motores en el arbol de construccion ANTES que
# en el PATH -- para que una instalacion vieja en /usr/local no se cuele.
guis: motores
	@for d in $(GUIS); do \
	    printf '== %s\n' "$$d"; \
	    $(MAKE) -s -C $$d || exit 1; \
	done

check: all
	@for d in $(BANCOS); do \
	    printf '\n===== %s\n' "$$d"; \
	    ( cd $$d && sh tests/run_tests.sh ) || exit 1; \
	done
	@echo
	@echo "todas las baterias pasan"

clean:
	@for d in $(MOTORES) $(GUIS); do $(MAKE) -s -C $$d clean; done

.PHONY: all motores guis check clean
