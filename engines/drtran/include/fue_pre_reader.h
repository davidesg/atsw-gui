/*****************************************************************************/
/*  fue_pre_reader.h -- part of drtran (Box-Jenkins transfer function models).
 *
 *  Original to drtran.
 *
 *  Copyright (C) 1995-2026 A.B. Treadway, J.A. Mauricio & D.E. Guerrero.
 *
 *  This program is free software: you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.  Distributed WITHOUT ANY WARRANTY; see the
 *  GNU General Public License (file COPYING) for details.
 *****************************************************************************/

#ifndef FUE_PRE_READER_H
#define FUE_PRE_READER_H

#include "main.h"

/*
 * Lee un archivo .pre (formato de salida de FUE) y llena las estructuras
 * Tm (modelo) y Ts (serie temporal). Devuelve 0 si éxito.
 * La matriz DataMat se dimensiona como DataMat[0..Tm->NdetVar][1..Ts.nobs]
 * y contiene las variables deterministas (fila 0 = serie transformada).
 */
/*  BUG-12: lo que read_fue_pre reserva, soltado.  Ver fue_pre_reader.c.   */
void free_fue_pre( struct Tusmodel *Tm, struct Tseries *Ts, real **DataMat );

int read_fue_pre(const char *filename,
                 struct Tusmodel *Tm,
                 struct Tseries *Ts,
                 real ***DataMat);

/* Componente determinista sobre un horizonte extendido (1..nobs_ext): las
   deterministas son funciones del tiempo, asi que su futuro se CONOCE. */
void build_det_component(struct Tusmodel *Tm, struct Tseries *Ts,
                         int nobs_ext, real *det_out);

/* Los dos operadores no estacionarios, comparados por su POLINOMIO. La usan
   el motor (operators_differ) y el GUI, que necesita decir antes de lanzar
   si un enlace va a forzar el cast por resta. */
int operators_differ_tm( const struct Tusmodel *a, const struct Tusmodel *b );

#endif
