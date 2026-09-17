/*
 * outfcst.h -- la prevision, leida del .out que escribe drtran.
 *
 * Como lib/outdiag: el motor ya lo calcula todo y aqui solo se lee. Y por la
 * misma razon que alli --el motor no emite nada legible por maquina-- la
 * prueba se corre contra un .out DE VERDAD, recien escrito.
 *
 * HAY TRES COSAS DISTINTAS EN EL .out Y LA PANTALLA NO PUEDE MEZCLARLAS:
 *
 * 1. LA PREVISION, por serie: el NIVEL y su variacion --del periodo y anual--
 *    cada una con su desviacion tipica. Es el formato de fuf/forsil.
 *
 * 2. LA DESCOMPOSICION DE LA VARIANZA del error: de que innovacion viene cada
 *    parte del error de prever la salida. El motor la da SOLO si Sigma es
 *    diagonal, y si no, se niega y explica por que: con innovaciones
 *    correlacionadas la descomposicion NO ES UNICA --hay que dar la parte
 *    comun a alguien, y eso exige una ORDENACION (Cholesky)-- que es
 *    exactamente el problema del VAR. Se evita mientras Sigma sea diagonal y
 *    se DECLARA cuando no lo es. Esa negativa hay que conservarla: es un
 *    resultado, no un hueco.
 *
 * 3. LA EVALUACION FUERA DE MUESTRA: MAE, RMSE y MAPE por horizonte, con los
 *    parametros estimados UNA VEZ sobre la ventana y luego CLAVADOS mientras
 *    el origen rueda.
 *
 * Y LA DISTINCION QUE HAY QUE GRITAR, porque es la que se confunde: las
 * desviaciones tipicas de (1) son TEORICAS. Dicen lo que el modelo implica, no
 * lo que pasa fuera de muestra, donde la incertidumbre de los parametros y el
 * cambio estructural tienen su parte. (3) es la unica respuesta empirica, y la
 * unica forma de decidir si un modelo predice mejor que otro.
 *
 * Eso es ademas lo que TASTE no podia hacer: tenia UNA ranura de residuos, asi
 * que estimar un segundo modelo borraba el primero y no habia con que comparar.
 */

#ifndef ATSW_OUTFCST_H
#define ATSW_OUTFCST_H

#include <stddef.h>

#define OF_MAX_SER    16
#define OF_MAX_FILA  256
#define OF_MAX_HOR    64
#define OF_NOMBRE     64

/* Una fila de la tabla de prevision. Las observadas no traen desviacion y si
 * traen error; las previstas, al reves. tiene_sd las distingue.          */
typedef struct {
   char   fecha[16];
   double nivel,  sd_nivel;
   double var_per, sd_per;
   double var_anu, sd_anu;
   double err;
   int    tiene_sd;      /* es una prevision, no una observacion */
   int    tiene_err;
} OfFila;

typedef struct {
   char   nombre[OF_NOMBRE];
   char   origen[16];
   int    lead;
   OfFila f[OF_MAX_FILA];
   int    nf;
   int    nprev;         /* cuantas de las filas son prevision */
} OfSerie;

/* Un horizonte de la evaluacion recursiva. */
typedef struct {
   int    h, n;
   double mae, rmse, mape;
} OfHoriz;

typedef struct {
   char    salida[OF_NOMBRE];
   int     origenes, desde, hasta, horizonte;
   OfHoriz h[OF_MAX_HOR];
   int     nh;
} OfEval;

typedef struct {
   OfSerie s[OF_MAX_SER];
   int     ns;

   OfEval  ev;
   int     tiene_ev;

   /* La descomposicion de la varianza: 1 si la dio, 0 si se nego porque Sigma
    * no es diagonal, -1 si no venia en el fichero.                       */
   int     decomp;
} Forecast;

/* Devuelve 0 si encontro prevision, 1 si no. */
int of_parse( const char *texto, Forecast *f );
int of_parse_file( const char *path, Forecast *f );

/* La serie que se llama asi, o NULL. */
const OfSerie *of_serie( const Forecast *f, const char *nombre );

#endif /* ATSW_OUTFCST_H */
