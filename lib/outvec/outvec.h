/*
 * outvec.h -- el lector del .out de drvec: el rango y el VEC.
 *
 * Es a drvec lo que lib/outvarma es a drvarma (DISENO-sim.md §2, S10): lo
 * que sim_gui, atsw-cli y el asistente leen de una corrida, SIN VENTANA. Da
 * HECHOS -- numeros y estados --, no frases: cada front end los redacta.
 *
 * Lee los dos .out que escribe drvec por la ruta de los .pre:
 *
 *   la prueba del rango   drvec A.pre ... p q 0 -case c -lrtest -bootstrap B
 *                         -name RANGO: la escalera bajo el rango, la puerta
 *                         y, por cada r, su ajuste; al final las tablas
 *                         (logL por rango, la LR con los valores asintoticos
 *                         y, con -bootstrap, los del bootstrap y el p-valor).
 *   el VEC de un rango    drvec A.pre ... p q r -case c [-fdhess] -name VEC:
 *                         la puerta, el ajuste, la tabla de parametros (con
 *                         alpha y beta), las matrices del VEC y, si se pidio
 *                         una restriccion (-weakex, -fixb2 v, ...), su LR.
 *
 * LOS MARCADORES son los de drvec 0.14 (engines/drvec/src/drvec.c): el
 * banner de 61 '=', las cabeceras «=== ... ===» y «--- ... ---», las lineas
 * «clave : valor» de arriba. La prueba (run_test.sh) corre el drvec del arbol
 * cada vez: si el formato cambia, se sabe el mismo dia.
 *
 * EL ORDEN DE LAS SERIES es el de la orden: las primeras M - r forman el
 * bloque nabla Y2 y las ultimas r, Y1, sobre el que se normaliza beta. alpha
 * va con el signo de Johansen: NEGATIVO corrige el error.
 *
 * Sin dependencias: libc y libm.
 */

#ifndef ATSW_OUTVEC_H
#define ATSW_OUTVEC_H

#include <stddef.h>

#define OVEC_MAX_SER   16
#define OVEC_MAX_PAR  512
#define OVEC_MAX_LR     8
#define OVEC_NOMBRE    64

/* Una fila de la tabla de parametros. */
typedef struct {
   char   nombre[OVEC_NOMBRE];  /* «D.LRM <- ec1», «LRY in ec1», «var IBO» */
   char   seccion[96];          /* la linea que la precede: «Adjustment ...»  */
   double est, se, t, p;
} OvecPar;

/* El rango r: su ajuste en la prueba. */
typedef struct {
   int    r, npar;
   double logL, aic, bic;
} OvecRango;

/* La comparacion r -> r + 1: la LR y sus valores criticos (asintoticos o del
 * bootstrap). p y reps solo con el bootstrap (p < 0: no consta). Sin valores
 * criticos (el caso 3 en la tabla asintotica), hay_cv = 0.               */
typedef struct {
   int    r, mr;
   double lr;
   int    hay_cv;
   double cv10, cv5, cv1;
   double p;
   int    reps;
} OvecPrueba;

/* Una restriccion contrastada por LR («--- H1(r): alpha = A*psi ... ---»). */
typedef struct {
   char   que[160];             /* la cabecera, sin los guiones              */
   double libre, restringido, lr, p;
   int    gl;
} OvecLR;

typedef struct {
   /* --- la cabecera ------------------------------------------------------ */
   char   programa[32];         /* «DRVEC 0.14»                              */
   char   determinista[160];    /* «case 2 -- E[nabla Y2] = 0, E[W] free»    */
   char   caso[8];              /* «1», «2», «3», «4», «pre»                 */
   int    m;                    /* cuantas series                            */
   char   serie[OVEC_MAX_SER][OVEC_NOMBRE];   /* en el orden de la orden     */
   int    ny2;                  /* cuantas en el bloque nabla Y2             */
   int    nobs;                 /* las observaciones que usa                 */
   int    nobs_raw;             /* las de los ficheros («(raw 55)»)          */
   int    es_rango;             /* 1: la prueba del rango; 0: un VEC de un r */
   int    r;                    /* el rango del VEC (-1 en la prueba)        */
   int    p, q;                 /* VARMA-VECM(p,q)                           */

   /* --- la puerta (la escalera bajo el rango) ----------------------------- */
   int    puerta;               /* 1 verificada, 0 NO, -1 no consta          */
   double puerta_suma, puerta_conjunta, puerta_dif;

   /* --- el optimizador: el ULTIMO banner es el del ajuste ----------------- */
   int    n_conv, n_parado;     /* cuantos banners CONVERGED y STOPPED       */
   int    ultimo_conv;          /* el ultimo: 1 CONVERGED, 0 STOPPED, -1 no  */
   int    ultimo_iter;
   char   ultimo_criterio[96];
   /* «Search: 3 starting points, 3 converged; logL from a to b.» (el ultimo) */
   int    arranques, arranques_conv;
   double busca_min, busca_max;

   /* --- la prueba del rango ----------------------------------------------- */
   int        nrango;
   OvecRango  rango[OVEC_MAX_SER];
   int        nasint;
   OvecPrueba asint[OVEC_MAX_SER];
   int        nboot, B;
   OvecPrueba boot[OVEC_MAX_SER];

   /* --- el ajuste de un VEC ----------------------------------------------- */
   double sigma2, logL, aic, bic;
   int    npar;
   int    hay_vec;              /* se leyo el bloque del VEC                 */
   int    nr;                   /* columnas de alpha y beta (= r)            */
   double alpha[OVEC_MAX_SER][OVEC_MAX_SER];  /* M x r                       */
   double beta[OVEC_MAX_SER][OVEC_MAX_SER];   /* M x r                       */
   int    new_;                 /* cuantos valores tiene E[W]                */
   double ew[OVEC_MAX_SER];
   double sigma[OVEC_MAX_SER][OVEC_MAX_SER];  /* simetrica                   */

   int     npar_t;
   OvecPar par[OVEC_MAX_PAR];

   int     nlr;
   OvecLR  lr[OVEC_MAX_LR];
} VecOut;

/* Lee el .out. 0 si se pudo abrir; los apartados que no esten quedan a
 * cero (nrango = 0, hay_vec = 0...): un .out cortado no es un error de
 * lectura, es un hecho que el llamador mira.                            */
int ovec_parse_file( const char *path, VecOut *v );
int ovec_parse( const char *texto, VecOut *v );

/* La fila de la tabla de parametros con ese nombre, o NULL. */
const OvecPar *ovec_param( const VecOut *v, const char *nombre );

/* El rango que propone la prueba (la regla de sima, propose_rank): el primer
 * r cuya comparacion r -> r+1 NO se rechaza al 5 % con los valores del
 * bootstrap (o, sin bootstrap, los asintoticos). Si todas se rechazan,
 * m - 1 y *al_menos = 1 (drvec no contrasta el rango completo). -1 si no se
 * puede leer (una comparacion que falta o sin valores criticos).        */
int ovec_rango_propuesto( const VecOut *v, int *al_menos );

/* --- la prevision y la evaluacion (S10c) -------------------------------- */

#define OVEC_MAX_H 120

/* El .forecast de -f H: por serie, el nivel con su banda del 95 % (teorica). */
typedef struct {
   char   nombre[OVEC_NOMBRE];
   int    nf;
   char   fecha[OVEC_MAX_H][16];
   double nivel[OVEC_MAX_H], lo[OVEC_MAX_H], hi[OVEC_MAX_H], se[OVEC_MAX_H];
} OvecPrev;

/* Cuantas series lee (hasta max), 0 ninguna, -1 si no se abre. */
int ovec_forecast_file( const char *path, OvecPrev *prev, int max );

/* El .recursive de -f H -estwin E: «origin,h,series,actual,forecast,error»,
 * el origen como fila de nabla Y (sima lo cuenta hacia atras desde el final:
 * nobs_raw - (origen + 1)).                                            */
typedef struct {
   int    origen, h;
   char   serie[OVEC_NOMBRE];
   double real, prevision, error;
} OvecRecFila;

typedef struct {
   int          estwin, horizonte;
   OvecRecFila *f;            /* reservado por ovec_recursive_file           */
   int          nf;
} OvecRec;

int  ovec_recursive_file( const char *path, OvecRec *r );   /* 0 bien, 1 vacio, -1 no se abre */
void ovec_recursive_free( OvecRec *r );

/* El texto de una seccion del .out por su titulo de banner («Impulse
 * Response of the Levels»): desde despues del banner hasta el siguiente.
 * g_free/free del llamador; NULL si no esta.                          */
char *ovec_seccion_file( const char *path, const char *titulo );

/* El progreso de un bootstrap en marcha: cuantas replicas hay ya en el
 * fichero .boot (las lineas de datos). -1 si no se puede leer.          */
int ovec_boot_hechas( const char *path_boot );

#endif /* ATSW_OUTVEC_H */
