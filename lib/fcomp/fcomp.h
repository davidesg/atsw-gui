/*
 * fcomp.h -- dos modelos frente a frente, por su error FUERA DE MUESTRA.
 *
 * Lee dos CSV de «drtran -C» (origin,horizon,actual,forecast,error): el
 * backtest de PARAMETROS FIJOS (-estwin), con los mismos origenes. Por
 * horizonte da el RMSE de cada uno, el cociente B/A y el contraste de
 * Diebold-Mariano: perdida cuadratica, varianza HAC rectangular hasta el
 * retardo h-1 y la correccion de muestra pequeña de Harvey, Leybourne y
 * Newbold (1997), con la t de N-1 grados de libertad.
 *
 * Es la replica en C de engines/drtran/examples/passthrough/forecast_compare.py
 * (y este, del sps/forecast_compare.py de SF_MEG): los mismos numeros.
 *
 * El convenio: A es la vara de medir (el univariante), B el modelo que se
 * prueba. ratio < 1 y DM > 0 dicen lo mismo: B preve mejor que A.
 */

#ifndef ATSW_FCOMP_H
#define ATSW_FCOMP_H

#define FC_MAX_HOR 64

typedef struct {
   int    h;
   int    n;            /* pares (origen, h) que entran                  */
   double rmse_a, rmse_b;
   double ratio;        /* rmse_b / rmse_a                               */
   double dm, p;        /* Diebold-Mariano (HLN) y su p bilateral         */
   int    dm_ok;        /* 0 si la varianza era degenerada o n < 2        */
} FcHoriz;

typedef struct {
   FcHoriz h[FC_MAX_HOR];
   int     nh;
   int     origenes;    /* origenes comunes a los dos                     */
} FcComp;

/* Devuelve 0 si pudo comparar; 1 si falta un fichero o no hay pares. */
int fc_compara_csv( const char *csv_a, const char *csv_b, FcComp *c );

#endif /* ATSW_FCOMP_H */
