/*
 * escalera.h -- drvarma en la escalera: los .pre de fue como entrada.
 *
 *     drvarma A.pre B.pre [C.pre ...] p q [-diagcov] [-redet] [-fixarma]
 *                                         [-m metodo] [-o NOMBRE]
 *
 * Cada serie trae su modelo univariante completo de su .pre; sobre las
 * series estacionarias se estima un VARMA cuya DIAGONAL es ese modelo y
 * cuya dinamica CRUZADA (ordenes p y q) es libre. Ver
 * docs/DESIGN-v5-ladder.md.
 */
#ifndef DRVARMA_ESCALERA_H
#define DRVARMA_ESCALERA_H

/* 1 si la linea de ordenes pide el modo escalera (primer argumento .pre). */
int escalera_requested(int argc, char *argv[]);

/* El programa entero en modo escalera. Devuelve el codigo de salida. */
int escalera_main(int argc, char *argv[]);

void escalera_usage(const char *prog);

#endif
