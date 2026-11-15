/*
 * artrun.h -- correr el identificador (engines/art) y leer lo que propone.
 *
 * Sacado de la ventana de identificacion de la madre (lib/analisis/
 * an_identifica.c) para que atsw-cli identifica corra art igual: los mismos
 * argumentos, la misma regla de los armonicos y el mismo lector del .cand
 * (artcand.h). Lo que la ventana y la cli digan del mismo modelo es lo mismo.
 *
 * Con GLib y lib/engine (artcand.c, el lector, sigue siendo C puro).
 */
#ifndef ATSW_ARTRUN_H
#define ATSW_ARTRUN_H

#include <stddef.h>
#include "artcand.h"

/* La serie como la lee art: «# freq», «# start», un valor por linea. 0 si
 * pudo escribirla.                                                      */
int ac_escribe_datos( const char *ruta, const double *x, int n,
                      int freq, int per, int anio );

/* art sobre datos (el fichero de ac_escribe_datos), con la transformacion
 * lam (0 log, 1 niveles), d y D, o sobre unos residuos (residuos != 0:
 * --harmonics off --no-tests). exe es el binario de art.
 *
 * LA REGLA DE LOS ARMONICOS (la de la ventana): con D = 0 y freq > 1, art
 * retira los armonicos siempre, aunque su propia F diga que no hay patron
 * estacional; si la F no lo detecta, se vuelve a identificar con
 * --harmonics off y *sin_armonicos queda a 1.
 *
 * 0 y c lleno, o 1 con el motivo en why.                                */
int ac_corre( const char *exe, const char *datos, int freq, double lam, int d, int D,
              int residuos, ArtCand *c, int *sin_armonicos, char *why, size_t n );

#endif /* ATSW_ARTRUN_H */
