/*
 * outvarma.h -- el VARMA de la escalera, leido de lo que escribe drvarma.
 *
 *   drvarma A.pre B.pre ... p q [-diagcov] [-links ...] [-forecast H [-estwin N]] -o NOMBRE
 *     -> NOMBRE.out, NOMBRE.forecast, NOMBRE.recursive
 *
 * Como lib/outdiag y lib/outfcst para drtran: AQUI NO SE CALCULA NADA. El
 * motor ya lo hace todo; esto solo lee. Y como alli, el motor no emite nada
 * legible por maquina, asi que la prueba se corre contra ficheros DE VERDAD,
 * recien escritos por drvarma con los .pre de Jenkins y Alavi
 * (lib/outvarma/run_test.sh). Si el formato cambia, se sabe el mismo dia.
 *
 * LA LIBRERIA DEVUELVE EL HECHO, NO LA FRASE (docs/DISENO-sim.md §4.2). Las
 * paginas de sim_gui, atsw-cli y el asistente deciden como decirlo.
 *
 * LO QUE HAY EN EL .out, en su orden, y lo que se lee de cada cosa:
 *
 *   la cabecera   p, q, enlaces, Sigma completa o diagonal, deterministas y
 *                 ARMA univariantes fijos o reestimados, la ventana comun, y
 *                 la tabla de los modelos univariantes con su .pre
 *   LA PUERTA     por serie la logL, sigma2 y cuanto se movio del .pre (con la
 *                 razon: muestra recortada, o «no es un optimo»); la suma, la
 *                 conjunta diagonal, la diferencia y PASSED/FAILED. Si falla,
 *                 el motor sale con 5 y el .out acaba ahi
 *   el optimizador  convergio, se paro, o se paro EN LA PARED MA (las raices
 *                 MA a 5e-5 del circulo unidad: no es un optimo, aunque los
 *                 numeros salgan); o la estimacion fallo
 *   los parametros  nombre, estimacion, error tipico, t y p, tal cual
 *   la LR         el sistema contra el diagonal (los univariantes)
 *   Sigma         la matriz y las correlaciones
 *   las raices    el modulo mayor de las inversas de |phi(B)| y de |theta(B)|
 *   la IRF        ortogonalizada (Cholesky, en el orden del sistema)
 *   la FEVD       en porcentajes
 *   la diagnosis  la de lib/outdiag, que ya lee este .out: los residuos de
 *                 cada serie, Hosking y el Jarque-Bera multivariante
 *   la prevision  por serie, en su nivel, con la banda del 95 %
 *   la evaluacion recursiva, por serie y horizonte (con -estwin)
 *
 * Los tamanos: un VarmaOut ocupa unos 450 KB. Va en estatico o con malloc,
 * no en la pila.
 */

#ifndef ATSW_OUTVARMA_H
#define ATSW_OUTVARMA_H

#include "outdiag.h"

#define OV_MAX_SER    16
#define OV_MAX_PAR   512
#define OV_MAX_HOR    64
#define OV_MAX_FILA  256
#define OV_NOMBRE     64
#define OV_RUTA      512

/* Una fila de la tabla de modelos univariantes de la cabecera. */
typedef struct {
   char   nombre[OV_NOMBRE];
   char   pre[OV_RUTA];          /* el fichero, tal como lo nombra el motor */
   int    nobs;
   char   desde[16], hasta[16];
   double lambda;
   char   operador[32];          /* "D", "none", ...                       */
   char   ar[64], ma[64];        /* "(6)", "-"                             */
   char   media[16];             /* "fixed", "estimated", ...              */
} OvUni;

/* La puerta, por serie. */
typedef struct {
   char   nombre[OV_NOMBRE];
   double logl, sigma2, mov;
   int    recortada;     /* se movio, pero la ventana comun le quita datos */
   int    no_optimo;     /* se movio en su propia muestra: es un .inp      */
} OvPuertaSerie;

typedef struct {
   int           hay;
   OvPuertaSerie s[OV_MAX_SER];
   int           ns;
   double        suma, conjunta, diferencia;
   int           pasa;
} OvPuerta;

typedef enum {
   OV_OPT_NADA = 0,      /* no venia: la puerta fallo y el .out acaba ahi,
                            o es el sistema diagonal (p = q = 0), que no
                            escribe el bloque del optimizador          */
   OV_OPT_CONVERGIO,
   OV_OPT_PARADO,        /* limite de iteraciones, paso fallido...         */
   OV_OPT_PARED_MA       /* parado en la frontera de invertibilidad MA     */
} OvOptim;

typedef struct {
   char   nombre[OV_NOMBRE];     /* "AR1[MUSKRAT<-MINK]", "phi_MINK[B^1]" */
   double est, se, t, p;
   int    tiene_se;
} OvParam;

typedef struct {
   char   nombre[OV_NOMBRE];
   char   origen[16];
   char   fecha[OV_MAX_FILA][16];
   double nivel[OV_MAX_FILA], lo95[OV_MAX_FILA], hi95[OV_MAX_FILA];
   double var_per[OV_MAX_FILA], sd_per[OV_MAX_FILA];
   double var_anu[OV_MAX_FILA], sd_anu[OV_MAX_FILA];
   int    nf;
} OvPrev;

typedef struct {
   int    h, n;
   double mae, rmse, mape;
} OvHoriz;

typedef struct {
   char    nombre[OV_NOMBRE];
   OvHoriz h[OV_MAX_HOR];
   int     nh;
} OvEvalSerie;

/* LA CCF DE LOS RESIDUOS de un par (diagnose.c, «CROSS CORRELATION a[j]
 * (B) - a[i] (A)», i < j). Es la evidencia de la identificacion (N2, sima
 * identify_cross): los residuos del sistema diagonal son las innovaciones
 * de cada serie --preblanqueadas por construccion--, asi que una
 * correlacion fuera de la banda es dinamica entre las series.
 *
 *   r[k + nlags], k = -nlags..nlags
 *   k > 0   B --> A: B entra en la ecuacion de A  (en -links, «A<-B»)
 *   k < 0   A --> B: A entra en la ecuacion de B  («B<-A»)
 *   k = 0   contemporanea: apunta a Sigma completa
 *
 * i y j son indices 0..m-1 en el orden del sistema.                      */
#define OV_MAX_LAG     64
#define OV_MAX_PARES  120
typedef struct {
   int    i, j;
   char   a[OV_NOMBRE], b[OV_NOMBRE];   /* A = serie i, B = serie j       */
   double banda;                        /* 2/sqrt(n)                       */
   int    nlags;
   double r[2 * OV_MAX_LAG + 1];
   double q_ge0, q_gt0, q_le0, q_lt0;   /* Q de k>=0, k>0, k<=0, k<0      */
   int    df_ge0, df_gt0;               /* sus grados (los de k<=0, k<0 son los mismos) */
} OvCcf;

typedef struct {
   /* la cabecera */
   int      p, q;
   char     enlaces[OV_RUTA];    /* "" = todos los pares                    */
   int      sigma_diagonal;
   int      det_reestimados;
   int      arma_fijos;
   char     desde[16], hasta[16];
   int      nobs;                /* observaciones estacionarias comunes     */
   int      horizonte;           /* 0 si no se pidio prevision              */
   OvUni    uni[OV_MAX_SER];
   int      m;                   /* numero de series                        */

   OvPuerta puerta;

   /* el optimizador y la estimacion */
   OvOptim  optim;
   int      iteraciones;
   char     criterio[128];
   int      pared_n, pared_raices;   /* "1 of 2 inverse roots"             */
   int      fallo;                   /* la estimacion fallo                */
   char     fallo_msg[128];          /* la frase del motor                 */
   int      solo_diagonal;           /* el modelo es el sistema diagonal   */

   OvParam  par[OV_MAX_PAR];
   int      npar;
   char     se_como[32];             /* "fdhess", ...                      */
   double   logl;
   int      tiene_logl;

   /* la LR contra los univariantes */
   int      tiene_lr;
   double   lr_logl_diag, lr_logl_full, lr, lr_p;
   int      lr_df;

   /* Sigma = sigma2 * Q */
   int      tiene_sigma;
   double   sigma[OV_MAX_SER][OV_MAX_SER];
   double   correl[OV_MAX_SER][OV_MAX_SER];

   /* el modulo mayor de las raices inversas (0 si no hay) */
   double   raiz_ar_max, raiz_ma_max;
   int      n_raices_ar, n_raices_ma;

   /* IRF ortogonalizada: irf[choque][h][serie], h = 0..nh_irf-1 */
   int      nh_irf;
   double   irf[OV_MAX_SER][OV_MAX_HOR][OV_MAX_SER];
   /* FEVD: fevd[serie][h][choque] en %, h = 1..nh_fevd (indice h-1) */
   int      nh_fevd;
   double   fevd[OV_MAX_SER][OV_MAX_HOR][OV_MAX_SER];

   /* las CCF de los residuos, un par cada una (i < j) */
   OvCcf    ccf[OV_MAX_PARES];
   int      nccf;

   /* la diagnosis, leida por lib/outdiag */
   Diagnosis diag;
   int       tiene_diag;

   OvPrev      prev[OV_MAX_SER];
   int         nprev;

   OvEvalSerie ev[OV_MAX_SER];
   int         nev;
   int         ev_origenes;
} VarmaOut;

/* Lee el .out (o la salida por pantalla). 0 si es un .out de la escalera,
 * 1 si no lo es.                                                          */
int ov_parse( const char *texto, VarmaOut *v );
int ov_parse_file( const char *path, VarmaOut *v );   /* -1: no se abre    */

/* La CCF del par de series (en cualquier orden), o NULL. */
const OvCcf *ov_ccf( const VarmaOut *v, int i, int j );

/* La correlacion de «b entra en la ecuacion de a con retardo k» (k >= 0;
 * k = 0 es la contemporanea, la misma en los dos sentidos), leida de la
 * CCF de su par con el signo de k que toque. 0 si no la hay; *banda, si
 * no es NULL, recibe la banda 2/sqrt(n) (0 si no hay CCF).              */
double ov_ccf_r( const VarmaOut *v, int a, int b, int k, double *banda );

/* El parametro que se llama asi, o NULL. */
const OvParam *ov_param( const VarmaOut *v, const char *nombre );

/* El indice de la serie que se llama asi (0..m-1), o -1. */
int ov_serie( const VarmaOut *v, const char *nombre );

/* El .forecast: las mismas tablas de prevision que el .out. Devuelve el
 * numero de series leidas, 0 si no hay ninguna, -1 si no se abre.         */
int ov_forecast_file( const char *path, OvPrev *prev, int max );

/* El .recursive: una fila por origen, serie y horizonte. */
typedef struct {
   char   origen[16];
   char   serie[OV_NOMBRE];
   int    h;
   double nivel;
} OvRecFila;

typedef struct {
   int       estwin, horizonte;
   OvRecFila *f;          /* reservado por ov_recursive_file; ov_recursive_free */
   int       nf;
} OvRecursive;

int  ov_recursive_file( const char *path, OvRecursive *r );  /* 0 bien, 1 vacio, -1 no se abre */
void ov_recursive_free( OvRecursive *r );

#endif /* ATSW_OUTVARMA_H */
