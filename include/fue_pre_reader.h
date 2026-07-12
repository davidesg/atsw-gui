#ifndef FUE_PRE_READER_H
#define FUE_PRE_READER_H

#include "main.h"

/*
 * Lee un archivo .pre (formato de salida de FUE) y llena las estructuras
 * Tm (modelo) y Ts (serie temporal). Devuelve 0 si éxito.
 * La matriz DataMat se dimensiona como DataMat[0..Tm->NdetVar][1..Ts.nobs]
 * y contiene las variables deterministas (fila 0 = serie transformada).
 */
int read_fue_pre(const char *filename,
                 struct Tusmodel *Tm,
                 struct Tseries *Ts,
                 real ***DataMat);

/* Componente determinista sobre un horizonte extendido (1..nobs_ext): las
   deterministas son funciones del tiempo, asi que su futuro se CONOCE. */
void build_det_component(struct Tusmodel *Tm, struct Tseries *Ts,
                         int nobs_ext, real *det_out);

#endif
