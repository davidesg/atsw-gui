/*
 * residuos.h -- los anomalos SOBRE LOS RESIDUOS DE UN .out: lo que la
 * ventana de la madre (lib/analisis/an_anomalos.c) y atsw-cli anomalos
 * calculan antes de dibujar o de escribir. Aparte de anomalos.h, que es
 * aritmetica elemental y no sabe de ficheros: esto lee un FueOut y usa la
 * regla de retardos del motor (fp_lags, lib/fugplot/lags.h).
 *
 * Una sola composicion para los dos anfitriones: la misma tipificacion, el
 * mismo umbral, los mismos episodios, la misma frecuencia y los mismos
 * retardos. Si cada uno la hiciera a su manera, la cli y la ventana dirian
 * episodios distintos del mismo modelo.
 */
#ifndef ATSW_RESIDUOS_H
#define ATSW_RESIDUOS_H

#include "anomalos.h"
#include "outfile.h"

typedef struct {
   int        n;                   /* residuos                             */
   double     z[FO_MAX_RES];       /* tipificados: e / raiz(media e^2)     */
   double     umbral;              /* an_umbral(n)                         */
   AnEpisodio ep[AN_MAX_EP];
   int        nep;
   int        freq;                /* de las fechas antes que del modelo   */
   int        lags;                /* default_lags, como el motor          */
} AnResiduos;

/* Lo de arriba, de los residuos de o. 0 si pudo; 1 si no hay residuos.   */
int an_residuos( const FueOut *o, AnResiduos *r );

/* Los indices de los episodios marcados (marcado[i] != 0), para omitir. NULL
 * marca todos. Devuelve cuantos.                                         */
int an_omite( const AnResiduos *r, const int *marcado, int *omit, int max );

#endif /* ATSW_RESIDUOS_H */
