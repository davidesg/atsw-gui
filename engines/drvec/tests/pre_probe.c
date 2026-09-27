/*****************************************************************************/
/*  tests/pre_probe.c -- part of drvec (VEC estimation, Mauricio 2006).
 *
 *  Copyright (C) 1995-2026 A.B. Treadway, J.A. Mauricio & D.E. Guerrero.
 *
 *  This program is free software: you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.  Distributed WITHOUT ANY WARRANTY; see the
 *  GNU General Public License (file COPYING) for details.
 *****************************************************************************/

/*  Ensena lo que read_fue_pre() ve en un .pre, para que la bateria pueda
 *  comprobarlo contra lo que el fichero dice.
 *
 *  POR QUE EXISTE.  La siembra de drvec solo usa el bloque MA del .pre, que se
 *  lee ANTES de la seccion de factores de la diferencia anual.  Asi que el
 *  fallo que esa seccion tenia -- y que esta arreglado en nuestra copia del
 *  lector, ver docs/PLAN_BETA.md F2.1 y BUG-11 de drtran -- no lo detecta
 *  ninguna corrida de estimacion: comprobado por mutacion, restaurarlo levanta
 *  cero fallos.  Lo que ese fallo estropea es el REFACTOR y la SERIE, y este
 *  programa es lo unico que los mira.
 *
 *  Imprime una linea:   nobs freq refactor d1 d2 dn1 dn
 *  (las cuatro ultimas: primera, segunda, penultima y ultima observacion).
 *  La bateria saca esos mismos valores del fichero con awk y los compara, asi
 *  que la comprobacion no envejece: no hay ningun numero de oro que mantener.
 */

#include "main.h"
#include "fue_pre_reader.h"

/*  WHAT diagnose.c EXPECTS FROM ITS HOST, declared here because this harness
 *  is the host.
 *
 *  ObsToDate belongs to the suite's diagnose.c (vendored whole on 2026-08-24);
 *  fue_pre_reader calls it to date the deterministic terms, so this harness has
 *  to link that object -- and diagnose.c, being the report writer of a full
 *  program, expects a handful of globals from it.  Giving them here keeps ONE
 *  definition of ObsToDate, its owner's, instead of a private copy: a copy is
 *  what docs/PLAN_PRODUCCION.md P3 exists to stop, and it is how the two
 *  implementations of a shared function start.
 *
 *  The CI found this on a clean tree; locally a stale bin/pre_probe from before
 *  the vendoring was still linked, and the suite had been passing over it.     */
FILE  *outputv         = NULL;
char **series_names    = NULL;
int    data_freq       = 1;
int    data_start_year = 1;
int    data_start_sub  = 1;
int    trans_d         = 0;
int    trans_D         = 0;

real macheps;

int main(int argc, char **argv)
{
    struct Tusmodel Tm;
    struct Tseries  Ts;
    real **DataMat = NULL;
    int n;

    if (argc < 2) {
        fprintf(stderr, "uso: pre_probe fichero.pre\n");
        return 2;
    }
    if (read_fue_pre(argv[1], &Tm, &Ts, &DataMat) != 0) {
        fprintf(stderr, "pre_probe: no se pudo leer %s\n", argv[1]);
        return 1;
    }
    n = Ts.nobs;
    if (n < 2) { fprintf(stderr, "pre_probe: nobs = %d\n", n); return 1; }

    printf("%d %d %.10g %.10f %.10f %.10f %.10f\n",
           n, Ts.freq, Ts.refactor,
           Ts.data[1], Ts.data[2], Ts.data[n-1], Ts.data[n]);
    return 0;
}
