/*
 * raices.h -- las raices, para identificar: dos herramientas de art que NO
 * son su formal_tests, y por buenas razones.
 *
 * formal_tests (Shin-Fuller, DCD, MEG) contrasta hipotesis sobre el modelo
 * estimado, reestimandolo. Estas dos no contrastan el modelo: ayudan a
 * elegirlo. unit_root_analysis es, en palabras de art, «exploratory ... not
 * a formal hypothesis test»: da la d de partida, y el contraste formal de d
 * es Shin-Fuller. ar_factorization describe las raices del AR estimado; si
 * un par es un AR estacional se contrasta despues (MEG), no aqui.
 *
 * AQUI NO SE DECIDE NADA. Cada funcion devuelve los numeros y lo que dicen
 * por separado; la ventana los enseña y el analista decide. Es la regla de
 * lib/dictamen, y aqui pesa mas: el veredicto encadenado de art
 * (describe_formal_tests) es donde viven sus fallos abiertos.
 *
 * Dos piezas, las que no necesitan correr el motor otra vez:
 *
 *   rz_raiz_unitaria   ADF y KPSS para d = 0..max_d sobre la serie con su
 *                      Box-Cox. Es el unit_root_analysis de art. El calculo
 *                      NO se copia: se compila engines/art/src/unit_root_tests.c,
 *                      el que ya se coteja con statsmodels.
 *   rz_factores_ar     las raices de cada factor AR regular del .out, con el
 *                      amortiguamiento, la frecuencia y el periodo de cada
 *                      factor complejo, y sus errores tipicos por el metodo
 *                      delta cuando el factor es un AR(2) estimado entero.
 *                      Es el ar_factorization de art (art/roots.py).
 */

#ifndef ATSW_RAICES_H
#define ATSW_RAICES_H

#include <stddef.h>

/* ---- ADF y KPSS ------------------------------------------------------- */

#define RZ_MAX_D 2

typedef struct {
    int    d;                 /* diferencias regulares aplicadas           */
    int    n;                 /* observaciones de la serie diferenciada    */
    int    ok;                /* 0: la regresion ADF fallo, no hay numeros */
    double adf, adf_p, adf_crit5;
    int    adf_lags;
    double kpss, kpss_p;      /* kpss_p, acotado a [0.01, 0.10] (tabla)    */
    int    kpss_lags;
    int    adf_rechaza;       /* p < 0.05: rechaza la raiz unitaria        */
    int    kpss_rechaza;      /* p < 0.05: rechaza la estacionariedad      */
} RzRaiz;

/* Para d = 0..max_d, sobre BoxCox(x, lam) con d diferencias regulares.
 * Para cuando la serie diferenciada baja de 10 observaciones (como art) o
 * no es positiva con lam != 1. Devuelve cuantas filas lleno.            */
int rz_raiz_unitaria( const double *x, int n, double lam, int max_d,
                      RzRaiz *r );

/* El punto de partida que da art (recommended_d): la menor d en que ADF
 * rechaza; si no, la menor con ADF rechazando y KPSS no; si no, la ultima.
 * ADF manda sobre KPSS (art BUG-0002). Es un punto de partida, no un
 * contraste: el formal sobre el modelo estimado es Shin-Fuller.          */
int rz_d_inicial( const RzRaiz *r, int nr );

/* ---- La factorizacion del AR ------------------------------------------ */
/* Describe, no contrasta: los ± del metodo delta son errores tipicos, no
 * un veredicto.                                                          */

#define RZ_MAX_AR   16        /* factores AR regulares                     */
#define RZ_MAX_ORDEN 24       /* orden de cada uno                         */

typedef struct {
    int    complejo;
    double a1, a2;            /* (1 - a1 B) o (1 - a1 B - a2 B^2)          */
    double modulo;            /* |z| de la raiz en B; > 1 estacionario     */
    double r;                 /* amortiguamiento, 1/|z| (complejo)         */
    double omega;             /* frecuencia angular, radianes              */
    double armonico;          /* omega * s / (2 pi)                        */
    double periodo;           /* 2 pi / omega, en observaciones            */
    double vida_media;        /* ln 0.5 / ln r: lo que tarda en la mitad   */
    int    con_et;            /* hay errores tipicos (metodo delta)        */
    double r_et, periodo_et;
    int    estacional;        /* armonico a 0.15 de un entero 1..s/2       */
    int    casi_unitaria;     /* 1 - r < 0.05                              */
} RzFactor;

typedef struct {
    int      orden;
    double   c[RZ_MAX_ORDEN];       /* P(B) = 1 - c1 B - ... - cp B^p     */
    int      idx[RZ_MAX_ORDEN];     /* numero del parametro, 0 si fijo    */
    double   zre[RZ_MAX_ORDEN], zim[RZ_MAX_ORDEN];  /* raices, por modulo */
    int      nf;
    RzFactor f[RZ_MAX_ORDEN];
} RzAr;

typedef struct {
    int    s;                 /* periodo estacional                        */
    int    nar;
    RzAr   ar[RZ_MAX_AR];
    int    sin_et;            /* la covarianza es la semilla: sin ±        */
    /* art BUG-0103: modulos casi iguales son la FIRMA de un operador en
       B^N, no la prueba. Se avisa con 4 raices o mas y dispersion < 20 %. */
    int    modulos_parecidos;
    int    nmod;
    double mod_min, mod_max, mod_disp;
} RzFactores;

/* Lee los factores AR regulares del .out de fue y los factoriza. s es el
 * periodo; sin_et, que la covarianza no vale (cero iteraciones: es la
 * semilla del optimizador, art BUG-0027). Devuelve 0 si pudo leer el .out
 * (aunque no tenga AR: nar = 0) y el motivo en why si no.               */
int rz_factores_ar( const char *out, int s, int sin_et, RzFactores *f,
                    char *why, size_t nwhy );

/* Factoriza P(B) = 1 - c1 B - ... - cp B^p. cov2, si no es NULL, es la
 * covarianza 2x2 de (c1, c2) de un AR(2) con los dos libres: da los ±.
 * Devuelve 0 si pudo.                                                    */
int rz_factoriza( const double *c, int p, int s, const double cov2[2][2],
                  RzAr *a );

#endif /* ATSW_RAICES_H */
