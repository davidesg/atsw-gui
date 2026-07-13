/*****************************************************************************/
/*  drtran.c -- part of drtran (Box-Jenkins transfer function models).
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

/*****************************************************************************/
/*  DRTRAN.C                                                                  */
/*  Programa principal de estimación de funciones de transferencia            */
/*  Box–Jenkins (DRTRAN).                                                     */
/*  Lee dos modelos univariantes de FUE (.pre), aplica preblanqueo para       */
/*  identificar órdenes (b, r, s) y estima el modelo de transferencia         */
/*  mediante verosimilitud exacta VARMA bivariante (drvarma).                 */
/*****************************************************************************/

#include "main.h"
#include "drtran.h"
#include "fue_pre_reader.h"
#include "forecast.h"
#include <unistd.h>   /* getopt */

/* -------------------------------------------------------------------------- */
/* Definición de variables globales del modelo                                */
/* -------------------------------------------------------------------------- */
struct Tusmodel TmX, TmY;
struct Tseries TsX, TsY;
real **DataMatX = NULL, **DataMatY = NULL;
int n_stat = 0;
real *w_X = NULL, *w_Y = NULL;
int r_ord = 0, s_ord = 0, b_delay = 0;
int fix_X = 0, fix_noise = 0;
int fix_det_X = 0, fix_det_Y = 0;   /* 0 = seguir los flags del .pre */
int fix_mu_Y = 0, fix_mu_X = 0; /* se fijan desde el .pre (Tm->Imu) tras leerlo */
real mu_Y = 0.0, mu_X = 0.0;    /* valor de la media de cada serie */
int p_N = 0, q_N = 0, p_X = 0, q_X = 0;
real *phi_N = NULL, *theta_N = NULL, *phi_X = NULL, *theta_X = NULL;
int diag_cov = 1;

/* Variables globales requeridas por el motor drvarma */
real macheps;          /* épsilon de máquina (inicializado con cmacheps())  */
FILE *outputv;         /* archivo de salida global (usado por diagnose.c)  */
int quiet_mode = 1;    /* suprimir traza del optimizador (0 = verbose) */

#define DRTRAN_PI 3.14159265358979323846
#define DRTRAN_VERSION "1.0"

/* ── Resumen para la consola ────────────────────────────────────────────
   El .out lleva el detalle completo (tablas, gráficos, diagnósticos). Por
   pantalla solo va lo que el usuario necesita ver de un vistazo: si convergió
   y qué modelo se ha estimado. Estas variables lo recogen por el camino.   */
static real sum_p_transfer = -1.0;   /* p-valor de adecuación de la transferencia */
static real sum_p_exog     = -1.0;   /* p-valor de exogeneidad de la entrada      */
static real sum_logl       = 0.0;
static int  sum_npar       = 0;
static char outfile_path[600];
static const char *sum_conv = "";
static const char *sum_why  = "";
static int  sum_fault = 0;

/* Nombre base de una ruta, sin directorio ni extension: "a/b/ES_CPI.pre" -> "ES_CPI" */
static void base_name(const char *path, char *out, size_t n)
{
    const char *b = strrchr(path, '/');
    char *dot;
    b = (b == NULL) ? path : b + 1;
    snprintf(out, n, "%s", b);
    dot = strrchr(out, '.');
    if (dot != NULL) *dot = '\0';
}

/* Resultado real del optimizador (definidos en qnewtopt.c) */
extern int opt_iters, opt_termcode;

/* Un factor de frecuencia fija exige c₂ < 0 (su módulo es r = sqrt(−c₂)).
   fue devuelve ifault si no se cumple; drtran rechaza el punto igual.      */
int invalid_fixfreq(struct Tusmodel *Tm)
{
    int i;
    for (i = 1; i <= Tm->NumAr1f; i++)
        if (Tm->Ar1f[i][2] >= 0.0) return 1;
    for (i = 1; i <= Tm->NumMa1f; i++)
        if (Tm->Ma1f[i][2] >= 0.0) return 1;
    return 0;
}

/* shootx externa (definida en tran_shootx.c) */
extern void shootx(real *x, struct Tvarma *armax, int *ifaultx,
                   int firstx, int lastx);

/* -------------------------------------------------------------------------- */
/* Funciones auxiliares: extraer órdenes y coeficientes ARMA de Tusmodel      */
/* -------------------------------------------------------------------------- */

/* Calcula el orden AR total (suma de órdenes de todos los factores) */
static int total_ar_order(struct Tusmodel *Tm)
{
    int ord = 0, i;
    for (i = 1; i <= Tm->NumAr1;  i++) ord += Tm->p1[i];
    /* Un factor ANUAL de orden p actúa en los retardos sper, 2·sper, …, p·sper */
    for (i = 1; i <= Tm->NumAr2;  i++) ord += Tm->p2[i] * Tm->sper;
    for (i = 1; i <= Tm->NumAr1f; i++) ord += 2;   /* cada factor fijo es orden 2 */
    return ord;
}

/* Calcula el orden MA total */
static int total_ma_order(struct Tusmodel *Tm)
{
    int ord = 0, i;
    for (i = 1; i <= Tm->NumMa1;  i++) ord += Tm->q1[i];
    for (i = 1; i <= Tm->NumMa2;  i++) ord += Tm->q2[i] * Tm->sper;
    for (i = 1; i <= Tm->NumMa1f; i++) ord += 2;
    return ord;
}

/* ── Factores ARMA: solo los coeficientes que el .pre marca como estimables ──
   fue lleva un flag por coeficiente (Ia1/Ia2/Ia1f para el AR, Im1/Im2/Im1f para
   el MA): "0.0000  0" es un coeficiente FIJO, no un valor inicial. Igual que la
   media y los deterministas, drtran respeta esa especificación.             */

/* Número de parámetros AR LIBRES (coeficientes de factores, no expandidos) */
static int n_ar_free_params(struct Tusmodel *Tm)
{
    int n = 0, i, j;
    for (i = 1; i <= Tm->NumAr1;  i++)
        for (j = 1; j <= Tm->p1[i]; j++) if (Tm->Ia1[i][j] == 1) n++;
    for (i = 1; i <= Tm->NumAr2;  i++)
        for (j = 1; j <= Tm->p2[i]; j++) if (Tm->Ia2[i][j] == 1) n++;
    for (i = 1; i <= Tm->NumAr1f; i++)   /* un solo coef libre por factor: c₂ */
        if (Tm->Ia1f[i] == 1) n++;
    return n;
}

/* Número de parámetros MA LIBRES */
static int n_ma_free_params(struct Tusmodel *Tm)
{
    int n = 0, i, j;
    for (i = 1; i <= Tm->NumMa1;  i++)
        for (j = 1; j <= Tm->q1[i]; j++) if (Tm->Im1[i][j] == 1) n++;
    for (i = 1; i <= Tm->NumMa2;  i++)
        for (j = 1; j <= Tm->q2[i]; j++) if (Tm->Im2[i][j] == 1) n++;
    for (i = 1; i <= Tm->NumMa1f; i++)
        if (Tm->Im1f[i] == 1) n++;
    return n;
}

/* Empaqueta en x[] los coeficientes AR LIBRES de Tm (índice base idx). */
static int pack_ar_factors(struct Tusmodel *Tm, real *x, int idx)
{
    int i, j, base = idx;
    for (i = 1; i <= Tm->NumAr1; i++)
        for (j = 1; j <= Tm->p1[i]; j++)
            if (Tm->Ia1[i][j] == 1) x[idx++] = Tm->Ar1[i][j];
    for (i = 1; i <= Tm->NumAr2; i++)
        for (j = 1; j <= Tm->p2[i]; j++)
            if (Tm->Ia2[i][j] == 1) x[idx++] = Tm->Ar2[i][j];
    for (i = 1; i <= Tm->NumAr1f; i++)
        if (Tm->Ia1f[i] == 1) x[idx++] = Tm->Ar1f[i][2];
    return idx - base;
}

/* Empaqueta en x[] los coeficientes MA LIBRES de Tm */
static int pack_ma_factors(struct Tusmodel *Tm, real *x, int idx)
{
    int i, j, base = idx;
    for (i = 1; i <= Tm->NumMa1; i++)
        for (j = 1; j <= Tm->q1[i]; j++)
            if (Tm->Im1[i][j] == 1) x[idx++] = Tm->Ma1[i][j];
    for (i = 1; i <= Tm->NumMa2; i++)
        for (j = 1; j <= Tm->q2[i]; j++)
            if (Tm->Im2[i][j] == 1) x[idx++] = Tm->Ma2[i][j];
    for (i = 1; i <= Tm->NumMa1f; i++)
        if (Tm->Im1f[i] == 1) x[idx++] = Tm->Ma1f[i][2];
    return idx - base;
}

/* ── Impresión de los factores ARMA estimados ───────────────────────────
   x[] guarda los coeficientes de los FACTORES tal como los especifica fue (sin
   expandir), no el polinomio expandido. Recorrerlos con p_N/q_N (que son los
   órdenes EXPANDIDOS: un MA anual de orden 1 expande a 12) se sale del vector.
   Este helper camina exactamente en el mismo orden que pack_ar_factors /
   pack_ma_factors, y etiqueta cada coeficiente con su retardo real.         */
static void print_arma_factors(struct Tusmodel *Tm, const char *tag, int is_ar,
                               real *x, real **cov, real *dev, int *pi,
                               FILE *out)
{
    int i, j;
    int Num1 = is_ar ? Tm->NumAr1  : Tm->NumMa1;
    int Num2 = is_ar ? Tm->NumAr2  : Tm->NumMa2;
    int Numf = is_ar ? Tm->NumAr1f : Tm->NumMa1f;
    int *ord1 = is_ar ? Tm->p1 : Tm->q1;
    int *ord2 = is_ar ? Tm->p2 : Tm->q2;
    const char *sym = is_ar ? "phi" : "theta";

    int **fl1 = is_ar ? Tm->Ia1  : Tm->Im1;
    int **fl2 = is_ar ? Tm->Ia2  : Tm->Im2;
    int  *flf = is_ar ? Tm->Ia1f : Tm->Im1f;
    real **c1 = is_ar ? Tm->Ar1  : Tm->Ma1;
    real **c2 = is_ar ? Tm->Ar2  : Tm->Ma2;
    real **cf = is_ar ? Tm->Ar1f : Tm->Ma1f;
    int k;

    for (k = 0; k < 3; k++) {
        int nfac = (k == 0) ? Num1 : (k == 1) ? Num2 : Numf;
        for (i = 1; i <= nfac; i++) {
            int ncoef = (k == 0) ? ord1[i] : (k == 1) ? ord2[i] : 1;
            for (j = 1; j <= ncoef; j++) {
                real tstat, pval;
                char label[40];
                const char *note;
                int eff;   /* retardo efectivo en el polinomio expandido */
                int free_i = (k == 0) ? fl1[i][j] : (k == 1) ? fl2[i][j] : flf[i];
                real val   = (k == 0) ? c1[i][j]  : (k == 1) ? c2[i][j]  : cf[i][2];

                eff  = (k == 0) ? j : (k == 1) ? j * Tm->sper : 2;
                note = (k == 1) ? "  (anual)" : (k == 2) ? "  (frec. fija)" : "";

                /* etiqueta en UN solo token: los tests la leen por columnas.
                   Los factores de frecuencia fija se identifican por su
                   frecuencia (puede haber varios, todos de orden 2).        */
                if (k == 2) {
                    int fr = is_ar ? Tm->pfre1[i] : Tm->qfre1[i];
                    snprintf(label, sizeof label, "%s_%s[f=%d]", sym, tag, fr);
                } else {
                    snprintf(label, sizeof label, "%s_%s[B^%d]", sym, tag, eff);
                }

                /* coeficiente fijado en el .pre: no ocupa hueco en x[] */
                if (free_i != 1) {
                    fprintf(out, "%-20s (fixed %12.6f)%s\n", label, val, note);
                    continue;
                }

                dev[*pi] = (cov[*pi][*pi] > 0) ? sqrt(cov[*pi][*pi]) : 0.0;
                tstat = (dev[*pi] > 1e-15) ? x[*pi] / dev[*pi] : 0.0;
                pval  = 2.0 * (1.0 - normal_cdf(fabs(tstat)));

                fprintf(out, "%-20s %12.6f %12.6f %8.3f %6.4f %s%s\n",
                        label, x[*pi], dev[*pi], tstat, pval,
                        (pval < 0.001) ? "***" : (pval < 0.01) ? "**" :
                        (pval < 0.05) ? "*" : (pval < 0.1) ? "." : "",
                        note);
                (*pi)++;
            }
        }
    }
}

/* ── Construcción del par de series estacionarias ───────────────────────
   Cada serie pierde tantas observaciones iniciales como el orden de su propio
   operador no estacionario (p.ej. ∇∇₁₂ pierde 13; ∇ pierde 1). Si los dos
   modelos difieren en diferenciación, las w resultantes tienen distinta
   longitud: hay que recortarlas a la VENTANA COMÚN, quedándose con las últimas
   n = min(n_X, n_Y) observaciones de cada una (ambas series arrancan en la
   misma fecha, así que alinear por el final las alinea en el calendario).   */
static void trim_to_common(real *w, int nstat, int ncommon)
{
    int t, off = nstat - ncommon;
    if (off <= 0) return;
    for (t = 1; t <= ncommon; t++) w[t] = w[t + off];
}

void build_stationary_pair(void)
{
    int nstat_X = 0, nstat_Y = 0;

    if (w_X) free_vector(w_X, 1, n_stat);
    if (w_Y) free_vector(w_Y, 1, n_stat);
    w_X = NULL; w_Y = NULL;

    apply_univariate_model(&TmX, &TsX, DataMatX, &w_X, &nstat_X);
    apply_univariate_model(&TmY, &TsY, DataMatY, &w_Y, &nstat_Y);

    if (w_X == NULL || w_Y == NULL || nstat_X <= 0 || nstat_Y <= 0) {
        n_stat = 0;
        return;
    }

    n_stat = (nstat_X < nstat_Y) ? nstat_X : nstat_Y;
    trim_to_common(w_X, nstat_X, n_stat);
    trim_to_common(w_Y, nstat_Y, n_stat);
}

/* ── Deterministas: ω(B)/δ(B) por variable ─────────────────────────────
   fue marca con un flag (Imega/Ielta) qué coeficientes estima. drtran respeta
   esa especificación: solo los marcados entran en x[]; el resto queda fijo en
   el valor del .pre.                                                        */

/* Número de coeficientes deterministas LIBRES (ω y δ) de un modelo */
static int n_det_free_params(struct Tusmodel *Tm)
{
    int n = 0, i, j;
    for (i = 1; i <= Tm->NdetVar; i++) {
        for (j = 0; j <= Tm->Nomega[i]; j++)
            if (Tm->Imega[i][j] == 1) n++;
        for (j = 1; j <= Tm->Ndelta[i]; j++)
            if (Tm->Ielta[i][j] == 1) n++;
    }
    return n;
}

/* Empaqueta en x[] los coeficientes deterministas libres */
static int pack_det_params(struct Tusmodel *Tm, real *x, int idx)
{
    int i, j, base = idx;
    for (i = 1; i <= Tm->NdetVar; i++) {
        for (j = 0; j <= Tm->Nomega[i]; j++)
            if (Tm->Imega[i][j] == 1) x[idx++] = Tm->Omega[i][j];
        for (j = 1; j <= Tm->Ndelta[i]; j++)
            if (Tm->Ielta[i][j] == 1) x[idx++] = Tm->Delta[i][j];
    }
    return idx - base;
}

/* Desempaqueta desde x[] a Tm los coeficientes deterministas libres */
void unpack_det_params(struct Tusmodel *Tm, real *x, int *idx)
{
    int i, j;
    for (i = 1; i <= Tm->NdetVar; i++) {
        for (j = 0; j <= Tm->Nomega[i]; j++)
            if (Tm->Imega[i][j] == 1) Tm->Omega[i][j] = x[(*idx)++];
        for (j = 1; j <= Tm->Ndelta[i]; j++)
            if (Tm->Ielta[i][j] == 1) Tm->Delta[i][j] = x[(*idx)++];
    }
}

/* Desempaqueta coeficientes AR desde x[] a Tm (usado en shootx) */
void unpack_ar_factors(struct Tusmodel *Tm, real *x, int *idx)
{
    int i, j;
    for (i = 1; i <= Tm->NumAr1; i++)
        for (j = 1; j <= Tm->p1[i]; j++)
            if (Tm->Ia1[i][j] == 1) Tm->Ar1[i][j] = x[(*idx)++];
    for (i = 1; i <= Tm->NumAr2; i++)
        for (j = 1; j <= Tm->p2[i]; j++)
            if (Tm->Ia2[i][j] == 1) Tm->Ar2[i][j] = x[(*idx)++];
    for (i = 1; i <= Tm->NumAr1f; i++)
        if (Tm->Ia1f[i] == 1) Tm->Ar1f[i][2] = x[(*idx)++];
}

/* Desempaqueta coeficientes MA desde x[] a Tm */
void unpack_ma_factors(struct Tusmodel *Tm, real *x, int *idx)
{
    int i, j;
    for (i = 1; i <= Tm->NumMa1; i++)
        for (j = 1; j <= Tm->q1[i]; j++)
            if (Tm->Im1[i][j] == 1) Tm->Ma1[i][j] = x[(*idx)++];
    for (i = 1; i <= Tm->NumMa2; i++)
        for (j = 1; j <= Tm->q2[i]; j++)
            if (Tm->Im2[i][j] == 1) Tm->Ma2[i][j] = x[(*idx)++];
    for (i = 1; i <= Tm->NumMa1f; i++)
        if (Tm->Im1f[i] == 1) Tm->Ma1f[i][2] = x[(*idx)++];
}

/* Expande todos los factores AR en un único polinomio Φ(B)=1-φ₁B-φ₂B²-...
   phi_out[1..p] recibe los coeficientes (phi_out[0] = 1 implícito).
   Se asume que phi_out está pre-dimensionado con vector(1, p).            */
void expand_ar_factors(struct Tusmodel *Tm, real *phi_out, int p)
{
    int i, j, k;
    real *work;

    if (p == 0) return;

    work = vector(0, p);

    /* Inicializar: polinomio identidad 1 (coefs en work[1..p] = 0) */
    work[0] = 1.0;
    for (k = 1; k <= p; k++) work[k] = 0.0;

    /* Factores AR regulares: P_i(B) = -1 + a₁B + a₂B² + ...
       Convertido a VARMA:  -P_i(B) = 1 - a₁B - a₂B² - ...            */
    for (i = 1; i <= Tm->NumAr1; i++) {
        int ord_i = Tm->p1[i];
        for (k = p; k >= 0; k--) {
            real acc = work[k];
            for (j = 1; j <= ord_i && j <= k; j++)
                acc -= Tm->Ar1[i][j] * work[k - j];
            work[k] = acc;
        }
    }

    /* Factores AR ANUALES: 1 - a₁B^s - a₂B^2s - … (retardos múltiplos de sper) */
    for (i = 1; i <= Tm->NumAr2; i++) {
        int ord_i = Tm->p2[i];
        for (k = p; k >= 0; k--) {
            real acc = work[k];
            for (j = 1; j <= ord_i; j++) {
                int lag = j * Tm->sper;
                if (lag <= k) acc -= Tm->Ar2[i][j] * work[k - lag];
            }
            work[k] = acc;
        }
    }

    /* Factores AR de FRECUENCIA FIJA (irreducibles). fue los parametriza con un
       ÚNICO coeficiente libre c₂ (<0); el término en B se DERIVA de él y de la
       frecuencia (fue.c:4064):
           c₁ = 2·cos(2πf/s)·sqrt(−c₂)     →   1 − c₁B − c₂B²
       que es (1 − 2r·cos(ω)B + r²B²) con r = sqrt(−c₂).                     */
    for (i = 1; i <= Tm->NumAr1f; i++) {
        real c2 = Tm->Ar1f[i][2];
        real r  = (c2 < 0.0) ? sqrt(-c2) : 0.0;
        real c1 = 2.0 * cos(2.0 * DRTRAN_PI * Tm->pfre1[i] / Tm->sper) * r;

        Tm->Ar1f[i][1] = c1;   /* fue guarda el término en B junto al factor */

        for (k = p; k >= 0; k--) {
            real acc = work[k];
            if (k >= 1) acc -= c1 * work[k - 1];
            if (k >= 2) acc -= c2 * work[k - 2];
            work[k] = acc;
        }
    }

    /* Copiar a phi_out[1..p] (negar porque work almacena P(B),
       pero VARMA usa Φ(B)=1-φ₁B-φ₂B²-... = 1 - work[1]B - work[2]B²-...) */
    for (k = 1; k <= p; k++) phi_out[k] = -work[k];

    free_vector(work, 0, p);
}

/* Expande todos los factores MA en un único polinomio Θ(B)=1-θ₁B-θ₂B²-...
   Misma lógica que expand_ar_factors.                                   */
void expand_ma_factors(struct Tusmodel *Tm, real *theta_out, int q)
{
    int i, j, k;
    real *work;

    if (q == 0) return;

    work = vector(0, q);
    work[0] = 1.0;
    for (k = 1; k <= q; k++) work[k] = 0.0;

    for (i = 1; i <= Tm->NumMa1; i++) {
        int ord_i = Tm->q1[i];
        for (k = q; k >= 0; k--) {
            real acc = work[k];
            for (j = 1; j <= ord_i && j <= k; j++)
                acc -= Tm->Ma1[i][j] * work[k - j];
            work[k] = acc;
        }
    }

    /* Factores MA ANUALES: 1 - θ₁B^s - θ₂B^2s - … (retardos múltiplos de sper) */
    for (i = 1; i <= Tm->NumMa2; i++) {
        int ord_i = Tm->q2[i];
        for (k = q; k >= 0; k--) {
            real acc = work[k];
            for (j = 1; j <= ord_i; j++) {
                int lag = j * Tm->sper;
                if (lag <= k) acc -= Tm->Ma2[i][j] * work[k - lag];
            }
            work[k] = acc;
        }
    }

    /* Factores MA de FRECUENCIA FIJA: igual que los AR (fue.c:4202) */
    for (i = 1; i <= Tm->NumMa1f; i++) {
        real c2 = Tm->Ma1f[i][2];
        real r  = (c2 < 0.0) ? sqrt(-c2) : 0.0;
        real c1 = 2.0 * cos(2.0 * DRTRAN_PI * Tm->qfre1[i] / Tm->sper) * r;

        Tm->Ma1f[i][1] = c1;

        for (k = q; k >= 0; k--) {
            real acc = work[k];
            if (k >= 1) acc -= c1 * work[k - 1];
            if (k >= 2) acc -= c2 * work[k - 2];
            work[k] = acc;
        }
    }

    /* Copiar a theta_out[1..q] (misma convención de signos que AR) */
    for (k = 1; k <= q; k++) theta_out[k] = -work[k];

    free_vector(work, 0, q);
}

/* -------------------------------------------------------------------------- */
/* DateToObs: convierte (año, periodo) a número de observación               */
/* -------------------------------------------------------------------------- */
void DateToObs(int beg_per, int beg_sub, int per, int sub, int freq,
               int *obs_no)
{
    int srest, pcad, sad;

    srest = freq - beg_sub + 1;
    if (sub == freq) {
        pcad = per - beg_per;
        *obs_no = srest + freq * pcad;
    } else {
        pcad = per - beg_per - 1;
        sad  = sub;
        *obs_no = srest + freq * pcad + sad;
    }
}

/* -------------------------------------------------------------------------- */
/* apply_univariate_model: aplica las transformaciones del modelo univariante */
/* (Box-Cox, sustracción de deterministas, diferenciación) y devuelve la      */
/* serie estacionaria w[1..nstat_out].                                        */
/*                                                                           */
/* DataMat se asume con DataMat[0][1..nobs] disponible para la serie          */
/* transformada, y DataMat[1..NdetVar][1..nobs] para las deterministas.       */
/* -------------------------------------------------------------------------- */
void apply_univariate_model(struct Tusmodel *Tm, struct Tseries *Ts,
                            real **DataMat, real **w_out, int *nstat_out)
{
    int nobs = Ts->nobs;
    real lam = Tm->boxlam;
    int i, t, j;
    real *detrended;
    real *filt_num;   /* resultado intermedio del filtro numerador */
    real *filt_out;   /* resultado del filtro completo Ω(B)/Δ(B)  */

    /* 1. Box-Cox: DataMat[0][t] = refactor * (data[t]^λ - 1)/λ  (o log si λ≈0).
       El factor de reescalado de FUE MULTIPLICA la serie transformada: deja las
       varianzas en O(10), que es el rango en el que el optimizador puede
       trabajar (el paso de diferencias finitas de cdgrad es ~6e-6 absoluto).  */
    for (t = 1; t <= nobs; t++) {
        real y = Ts->data[t];
        if (y <= 0.0) {
            fprintf(stderr, "Error: dato no positivo para Box-Cox (t=%d, y=%g)\n",
                    t, y);
            *w_out = NULL;
            *nstat_out = 0;
            return;
        }
        if (fabs(lam) < 1e-8)
            DataMat[0][t] = log(y) * Ts->refactor;
        else
            DataMat[0][t] = ((pow(y, lam) - 1.0) / lam) * Ts->refactor;
    }

    /* 2. Sustraer componentes deterministas */
    detrended = vector(1, nobs);
    for (t = 1; t <= nobs; t++) detrended[t] = DataMat[0][t];

    if (Tm->NdetVar > 0) {
        /* DataMat lo rellena read_fue_pre a partir de la especificación del
           .pre (impulse/compimp/step/ramp/easter/trend/cos/sin/alter).      */
        filt_num = vector(1, nobs);
        filt_out = vector(1, nobs);

        for (i = 1; i <= Tm->NdetVar; i++) {
            int nw = Tm->Nomega[i];
            int nd = Tm->Ndelta[i];

            /* Inicializar a cero */
            for (t = 1; t <= nobs; t++) {
                filt_num[t] = 0.0;
                filt_out[t] = 0.0;
            }

            /* --- Aplicar numerador Ω(B) --- */
            for (t = 1; t <= nobs; t++) {
                real sum = 0.0;
                for (j = 0; j <= nw; j++) {
                    if (t - j >= 1)
                        sum += Tm->Omega[i][j] * DataMat[i][t - j];
                }
                filt_num[t] = sum;
            }

            /* --- Aplicar denominador 1/Δ(B) = 1/(1-δ₁B-δ₂B²-...) --- */
            /* El filtro recursivo: out[t] = num[t] + Σ δⱼ·out[t-j]   */
            if (nd > 0) {
                for (t = 1; t <= nobs; t++) {
                    real sum = filt_num[t];
                    for (j = 1; j <= nd; j++) {
                        if (t - j >= 1)
                            sum += Tm->Delta[i][j] * filt_out[t - j];
                    }
                    filt_out[t] = sum;
                }
            } else {
                /* Sin denominador: out = num directamente */
                for (t = 1; t <= nobs; t++)
                    filt_out[t] = filt_num[t];
            }

            /* Restar la contribución filtrada */
            for (t = 1; t <= nobs; t++)
                detrended[t] -= filt_out[t];
        }

        free_vector(filt_out, 1, nobs);
        free_vector(filt_num, 1, nobs);
    }

    /* 3. Aplicar operador no estacionario (diferenciación) */
    {
        int ornsop = Tm->ornsop;
        int nstat  = nobs - ornsop;

        if (nstat <= 0) {
            fprintf(stderr, "Error: demasiadas diferencias (ornsop=%d >= nobs=%d)\n",
                    ornsop, nobs);
            free_vector(detrended, 1, nobs);
            *w_out = NULL;
            *nstat_out = 0;
            return;
        }

        *w_out = vector(1, nstat);

        /* w[t] = Σ_{j=0}^{ornsop} (-rnsop[j]) * detrended[t+ornsop-j]
           donde rnsop[0] = -1, así que -rnsop[0] = +1                  */
        for (t = 1; t <= nstat; t++) {
            real sum = 0.0;
            int base = t + ornsop;   /* índice en la serie original */
            for (j = 0; j <= ornsop; j++) {
                sum += (-Tm->rnsop[j]) * detrended[base - j];
            }
            (*w_out)[t] = sum;
        }

        *nstat_out = nstat;
    }

    free_vector(detrended, 1, nobs);
}

/* -------------------------------------------------------------------------- */
/* prewhiten_and_identify — identificación Box–Jenkins de (b, r, s)           */
/*                                                                            */
/* 1. Preblanquea la ENTRADA con su propio modelo ARMA:  a_t = φ(B)/θ(B) w_X  */
/* 2. Filtra la SALIDA con EL MISMO filtro:              β_t = φ(B)/θ(B) w_Y  */
/* 3. CCF en la convención de Box–Jenkins:                                    */
/*        r(k) = corr( β_t , a_{t−k} )                                        */
/*    es decir: k > 0 significa "Y responde a X con k periodos de retardo".   */
/*    (El código anterior calculaba corr(a_{t+k}, β_t), que coloca la         */
/*     respuesta en los lags NEGATIVOS, y luego buscaba los picos entre los   */
/*     positivos: nunca encontraba nada.)                                     */
/* 4. Pesos de la respuesta impulso:  ν̂(k) = r(k) · s_β / s_a                 */
/* 5. Retroalimentación: si hay CCF significativa en k < 0, la entrada NO es  */
/*    exógena y el modelo de transferencia de una entrada no es válido.       */
/* 6. Propone varios (b, r, s) razonados y recomienda uno.                    */
/* -------------------------------------------------------------------------- */
void prewhiten_and_identify(real *w_X, real *w_Y, int n,
                            real *phi_X_in, int p_X_in,
                            real *theta_X_in, int q_X_in,
                            int *r, int *s, int *b,
                            FILE *outputv)
{
    real *a_X, *beta_Y, *ccf, *nu;
    int nlags, t, k, lag;
    real threshold, s_a, s_b, mean_a, mean_b;
    int b_hat = -1, last_sig = -1, nsig_neg = 0;

    nlags = (n / 4 < 24) ? n / 4 : 24;
    if (nlags < 10) nlags = 10;

    a_X    = vector(1, n);
    beta_Y = vector(1, n);
    ccf    = vector(-nlags, nlags);
    nu     = vector(-nlags, nlags);

    /* --- 1 y 2: preblanquear X y filtrar Y con el MISMO filtro ---
       φ(B) w  →  u ;  luego invertir θ(B):  a[t] = u[t] + Σ θⱼ a[t−j]        */
    for (t = 1; t <= n; t++) {
        real u = w_X[t];
        for (k = 1; k <= p_X_in && k < t; k++) u -= phi_X_in[k] * w_X[t - k];
        a_X[t] = u;
        for (k = 1; k <= q_X_in && k < t; k++) a_X[t] += theta_X_in[k] * a_X[t - k];
    }
    for (t = 1; t <= n; t++) {
        real u = w_Y[t];
        for (k = 1; k <= p_X_in && k < t; k++) u -= phi_X_in[k] * w_Y[t - k];
        beta_Y[t] = u;
        for (k = 1; k <= q_X_in && k < t; k++) beta_Y[t] += theta_X_in[k] * beta_Y[t - k];
    }

    /* --- 3: CCF con la rutina del motor (diagnose.c: Ccf) ---
       Ccf(d1, d2, ...) devuelve corr[j] = corr( d1_t , d2_{t+j-1} ), o sea el
       retardo k = j-1 >= 0. Por tanto:
         Ccf(a_X, beta_Y)  -> r(k), k>=0 : la ENTRADA antecede a la salida
                                           (es la transferencia que buscamos)
         Ccf(beta_Y, a_X)  -> r(-k)      : la SALIDA antecede a la entrada
                                           (retroalimentacion; deberia ser ruido)  */
    mean_a = Mean(a_X, n);      s_a = Stdev(a_X, n);
    mean_b = Mean(beta_Y, n);   s_b = Stdev(beta_Y, n);

    if (s_a < 1e-12 || s_b < 1e-12) {
        fprintf(outputv, "\nPrewhitening: series has no variability; cannot identify.\n");
        *b = 0; *r = 0; *s = 0;
        free_vector(nu, -nlags, nlags); free_vector(ccf, -nlags, nlags);
        free_vector(beta_Y, 1, n); free_vector(a_X, 1, n);
        return;
    }

    {
        real *cpos = vector(1, nlags + 1);   /* k >= 0 */
        real *cneg = vector(1, nlags + 1);   /* k <= 0 */

        Ccf(a_X, beta_Y, n, nlags, cpos, mean_a, mean_b, s_a, s_b);
        Ccf(beta_Y, a_X, n, nlags, cneg, mean_b, mean_a, s_b, s_a);

        for (lag = 0; lag <= nlags; lag++) {
            ccf[ lag] = cpos[lag + 1];
            ccf[-lag] = cneg[lag + 1];
        }
        /* ambos coinciden en k=0 por construccion */

        for (lag = -nlags; lag <= nlags; lag++)
            nu[lag] = ccf[lag] * s_b / s_a;      /* 4: pesos impulso nu(k) */

        free_vector(cneg, 1, nlags + 1);
        free_vector(cpos, 1, nlags + 1);
    }

    threshold = 2.0 / sqrt((real)n);

    /* --- Informe: grafico de la CCF en caracteres (diagnose.c: PlotCCF) --- */
    fprintf(outputv, "\n");
    fprintf(outputv, "=============================================================\n");
    fprintf(outputv, "  IDENTIFICATION - prewhitening and CCF (Box-Jenkins)         \n");
    fprintf(outputv, "=============================================================\n");
    fprintf(outputv, "  The input is prewhitened with its own ARMA and the SAME filter\n");
    fprintf(outputv, "  is applied to the output.  r(k) = corr(beta_t, a_{t-k}):\n");
    fprintf(outputv, "    k > 0  ->  Y responds to X with a k-period lag (the transfer)\n");
    fprintf(outputv, "    k < 0  ->  Y leads X (feedback: there should be NONE)\n\n");

    {
        struct Tseries ser;
        real *plot = vector(1, 2 * nlags + 1);

        for (lag = -nlags; lag <= nlags; lag++)
            plot[lag + nlags + 1] = ccf[lag];

        ser.nobs = n;
        ser.freq = TsX.freq;
        PlotCCF(plot, nlags, &ser);

        free_vector(plot, 1, 2 * nlags + 1);
    }

    /* Pesos de la respuesta impulso en los retardos significativos */
    fprintf(outputv, "\n  Impulse response weights  nu(k) = r(k) * s_beta / s_a\n");
    fprintf(outputv, "  (only lags with a significant CCF)\n\n");
    fprintf(outputv, "     k      r(k)      nu(k)\n");
    fprintf(outputv, "    ------------------------------\n");
    for (lag = -nlags; lag <= nlags; lag++) {
        if (fabs(ccf[lag]) <= threshold) continue;
        fprintf(outputv, "   %4d  %8.4f  %9.4f  %s\n", lag, ccf[lag], nu[lag],
                (lag < 0) ? "<-- feedback?" : "");
        if (lag < 0) nsig_neg++;
        if (lag >= 0 && b_hat < 0) b_hat = lag;   /* primer k significativo */
    }
    if (b_hat < 0 && nsig_neg == 0)
        fprintf(outputv, "   (none)\n");
    fprintf(outputv, "\n");

    /* La estructura de la transferencia es el bloque CONTIGUO de pesos que
       arranca en b: un pico significativo aislado en un retardo lejano es ruido
       muestral (con bandas al 5% se espera 1 de cada 20 fuera), no parte de
       nu(B). Tomar el ULTIMO significativo de toda la CCF disparaba s a valores
       absurdos (s=24).                                                        */
    if (b_hat >= 0) {
        last_sig = b_hat;
        while (last_sig + 1 <= nlags && fabs(ccf[last_sig + 1]) > threshold)
            last_sig++;
    }

    /* --- 5: exogeneidad ---
       Con bandas al 5% se espera que ~1 de cada 20 retardos las cruce por puro
       azar: contar "alguno significativo en k<0" dispara el aviso casi siempre.
       Se compara el numero observado con el esperado bajo la hipotesis nula.  */
    {
        real *cn = vector(1, nlags + 1);
        real Qn, pvaln;
        int kk;

        for (kk = 0; kk <= nlags; kk++) cn[kk + 1] = ccf[-kk];
        Qn    = ChiTestC(cn + 1, nlags, n);   /* se salta el retardo 0 */
        pvaln = 1.0 - chisq(Qn, nlags);

        fprintf(outputv, "  Exogeneity - portmanteau of the CCF at k < 0:\n");
        fprintf(outputv, "    Q(%d) = %.4f   p-value = %.4f   [%d significant out of %d]\n",
                nlags, Qn, pvaln, nsig_neg, nlags);

        if (pvaln < 0.05) {
            fprintf(outputv, "\n  WARNING: the output leads the input. There may be\n");
            fprintf(outputv, "  FEEDBACK (Y -> X). The single-input transfer model assumes X is\n");
            fprintf(outputv, "  EXOGENOUS; with feedback, its estimates are not reliable.\n\n");
            fprintf(outputv, "");
        } else {
            fprintf(outputv, "    X behaves as exogenous. OK\n\n");
        }
        free_vector(cn, 1, nlags + 1);
    }

    /* --- 6: propuestas --- */
    if (b_hat < 0) {
        fprintf(outputv, "  No significant CCF at k >= 0: no relationship detected.\n");
        fprintf(outputv, "  Proposal: b=0, r=0, s=0 (no transfer).\n");
        *b = 0; *r = 0; *s = 0;
    } else {
        int nblock = last_sig - b_hat + 1;   /* amplitud del bloque significativo */
        int r1 = 0, s1 = 0, r2 = -1, s2 = 0, b2 = b_hat;
        int decays = 0;
        real ratio = 0.0;

        /* Candidato A: todos los pesos significativos como omegas libres. */
        r1 = 0;
        s1 = last_sig - b_hat;
        if (s1 > MAX_S) {
            fprintf(outputv, "  WARNING: the significant block (s=%d) exceeds MAX_S=%d;\n"
                             "  it is truncated. Inspect the CCF by hand.\n", s1, MAX_S);
            s1 = MAX_S;
        }

        /* Candidato B: si la cola decae de forma aproximadamente geometrica,
           un denominador de orden 1 la resume con un solo parametro.        */
        if (nblock >= 3) {
            real q1 = fabs(nu[last_sig])     / (fabs(nu[last_sig - 1]) + 1e-12);
            real q2 = fabs(nu[last_sig - 1]) / (fabs(nu[last_sig - 2]) + 1e-12);
            if (q1 < 0.95 && q2 < 0.95 && fabs(q1 - q2) < 0.25) {
                decays = 1;
                ratio  = 0.5 * (q1 + q2);
                r2 = 1;
                s2 = (last_sig - 2) - b_hat;   /* omegas antes de que empiece el decaimiento */
                if (s2 < 0) s2 = 0;
            }
        }

        fprintf(outputv, "  Significant weights: k = %d..%d  (b = first significant k)\n\n",
                b_hat, last_sig);
        fprintf(outputv, "  PROPOSALS:\n");
        fprintf(outputv, "    [A]  b=%d  r=%d  s=%d   -- every significant weight as a free omega\n",
                b_hat, r1, s1);
        if (decays)
            fprintf(outputv, "    [B]  b=%d  r=%d  s=%d   -- the tail decays (ratio ~ %.2f): a\n"
                             "                             delta denominator sums it up with 1 parameter\n",
                    b2, r2, s2, ratio);
        else
            fprintf(outputv, "    [B]  (not applicable: the tail does not decay geometrically)\n");

        /* Recomendacion: la parsimoniosa si hay decaimiento claro, si no la directa */
        if (decays) { *b = b2; *r = r2; *s = s2; }
        else        { *b = b_hat; *r = r1; *s = s1; }

        fprintf(outputv, "\n  RECOMMENDED: b=%d, r=%d, s=%d\n", *b, *r, *s);
        fprintf(outputv, "  (use -b/-r/-s to impose a different specification)\n");
    }
    fprintf(outputv, "=============================================================\n\n");

    free_vector(nu, -nlags, nlags);
    free_vector(ccf, -nlags, nlags);
    free_vector(beta_Y, 1, n);
    free_vector(a_X, 1, n);
}

/* -------------------------------------------------------------------------- */
/* transfer_adequacy — el chequeo de adecuacion de Box-Jenkins                */
/*                                                                            */
/* En el cast bivariante los residuos son, por construccion:                  */
/*    a[.][2] = la ENTRADA PREBLANQUEADA  (serie 2 = w_X con su propio ARMA)   */
/*    a[.][1] = la innovacion del RUIDO   (serie 1 = w_Y - transferencia)      */
/*                                                                            */
/* Si la especificacion (b, r, s) es correcta, el ruido no debe conservar      */
/* NINGUNA huella de la entrada: la CCF entre ambos residuos debe ser ruido    */
/* blanco en TODOS los retardos.                                              */
/*   - CCF significativa en k >= 0  ->  la TRANSFERENCIA esta mal especificada */
/*                                      (falta peso en ese retardo)            */
/*   - CCF significativa en k <  0  ->  RETROALIMENTACION: X no es exogena y   */
/*                                      el modelo de una entrada no es valido  */
/* -------------------------------------------------------------------------- */
static void transfer_adequacy(real **a, int n, int b, int r, int s, FILE *out,
                              real *p_transfer, real *p_exog)
{
    int    nlags = (n / 4 < 24) ? n / 4 : 24;
    real  *aN, *aX, *cpos, *cneg, *plot;
    real   mN, mX, sN, sX, threshold, Q;
    int    t, k, lag, npar_tr, df, nsig_pos = 0, nsig_neg = 0;

    if (nlags < 10) nlags = 10;

    if (p_transfer) *p_transfer = -1.0;
    if (p_exog)     *p_exog     = -1.0;

    aN   = vector(1, n);
    aX   = vector(1, n);
    cpos = vector(1, nlags + 1);
    cneg = vector(1, nlags + 1);
    plot = vector(1, 2 * nlags + 1);

    for (t = 1; t <= n; t++) { aN[t] = a[t][1]; aX[t] = a[t][2]; }

    mN = Mean(aN, n);  sN = Stdev(aN, n);
    mX = Mean(aX, n);  sX = Stdev(aX, n);

    if (sN < 1e-12 || sX < 1e-12) goto cleanup;

    /* misma convencion que la identificacion: r(k) = corr(aN_t, aX_{t-k}) */
    Ccf(aX, aN, n, nlags, cpos, mX, mN, sX, sN);   /* k >= 0 */
    Ccf(aN, aX, n, nlags, cneg, mN, mX, sN, sX);   /* k <= 0 */

    for (lag = 0; lag <= nlags; lag++) {
        plot[ lag + nlags + 1] = cpos[lag + 1];
        plot[-lag + nlags + 1] = cneg[lag + 1];
    }

    threshold = 2.0 / sqrt((real)n);

    fprintf(out, "\n");
    fprintf(out, "=============================================================\n");
    fprintf(out, "  TRANSFER FUNCTION ADEQUACY                                  \n");
    fprintf(out, "  CCF between the estimated noise and the prewhitened input    \n");
    fprintf(out, "=============================================================\n");
    fprintf(out, "  If (b, r, s) is correct, this CCF must be white noise.\n");
    fprintf(out, "    significant at k >= 0 -> structure missing in the TRANSFER\n");
    fprintf(out, "    significant at k <  0 -> FEEDBACK (X is not exogenous)\n\n");

    {
        struct Tseries ser;
        ser.nobs = n;
        ser.freq = TsX.freq;
        PlotCCF(plot, nlags, &ser);
    }

    for (k = 0; k <= nlags; k++) if (fabs(cpos[k + 1]) > threshold) nsig_pos++;
    for (k = 1; k <= nlags; k++) if (fabs(cneg[k + 1]) > threshold) nsig_neg++;

    /* Portmanteau sobre los retardos k >= 0 (incluye el contemporaneo).
       Grados de libertad: nº de correlaciones menos los parametros de nu(B). */
    npar_tr = (s >= 0 ? s + 1 : 0) + r;
    Q  = ChiTestC(cpos, nlags + 1, n);
    df = (nlags + 1) - npar_tr;
    if (df < 1) df = 1;

    fprintf(out, "\n  Portmanteau of the CCF (k >= 0): this is the test of the transfer:\n");
    fprintf(out, "    Q(%d) = %.4f   [%d lags - %d parameters of nu(B)]\n",
            df, Q, nlags + 1, npar_tr);
    fprintf(out, "    p-value = %.4f\n", 1.0 - chisq(Q, df));

    /* El veredicto lo dicta el test CONJUNTO (portmanteau), no la presencia de
       algun pico suelto: con bandas al 5% se espera que ~1 de cada 20 retardos
       las cruce por azar, asi que exigir cero significativos condenaria incluso
       a una especificacion correcta.                                          */
    {
        real pval     = 1.0 - chisq(Q, df);
        real expected = 0.05 * (nlags + 1);

        if (p_transfer) *p_transfer = pval;

        fprintf(out, "\n  VERDICT:\n");

        if (pval < 0.05) {
            fprintf(out, "    *** The transfer is NOT adequate (p = %.4f).\n", pval);
            fprintf(out, "    The noise still carries a trace of the input at:\n");
            for (k = 0; k <= nlags; k++)
                if (fabs(cpos[k + 1]) > threshold)
                    fprintf(out, "      k = %2d   r = %7.4f\n", k, cpos[k + 1]);
            fprintf(out, "    Widen nu(B) to cover those lags: raise s up to the last one, or\n");
            fprintf(out, "    try r=1 if the weights decay. Then re-estimate.\n");
        } else {
            fprintf(out, "    The transfer is ADEQUATE (p = %.4f): the CCF is consistent\n", pval);
            fprintf(out, "    with white noise at k >= 0.\n");
            if (nsig_pos > 0) {
                fprintf(out, "    (%d lag(s) cross the individual bands; ~%.1f would be expected\n",
                        nsig_pos);
                fprintf(out, "     by chance, so they do not contradict the joint test:\n",
                        expected);
                for (k = 0; k <= nlags; k++)
                    if (fabs(cpos[k + 1]) > threshold)
                        fprintf(out, "       k = %2d   r = %7.4f\n", k, cpos[k + 1]);
            }
        }
    }

    /* Retroalimentacion: mismo criterio, un test CONJUNTO sobre los retardos
       k < 0 (cneg[1] es el retardo 0, compartido; se salta con cneg+1).      */
    {
        real Qn    = ChiTestC(cneg + 1, nlags, n);
        real pvaln = 1.0 - chisq(Qn, nlags);

        if (p_exog) *p_exog = pvaln;

        fprintf(out, "\n  Portmanteau of the CCF (k < 0): this is the EXOGENEITY test:\n");
        fprintf(out, "    Q(%d) = %.4f   p-value = %.4f   [%d significant]\n",
                nlags, Qn, pvaln, nsig_neg);
        if (pvaln < 0.05) {
            fprintf(out, "\n    *** WARNING: the output leads the input. There are signs of\n");
            fprintf(out, "    FEEDBACK (Y -> X), and the single-input transfer model assumes\n");
            fprintf(out, "    X is EXOGENOUS: its estimates would not be reliable.\n");
        } else {
            fprintf(out, "    X behaves as exogenous. OK\n");
        }
    }
    fprintf(out, "=============================================================\n\n");

cleanup:
    free_vector(plot, 1, 2 * nlags + 1);
    free_vector(cneg, 1, nlags + 1);
    free_vector(cpos, 1, nlags + 1);
    free_vector(aX, 1, n);
    free_vector(aN, 1, n);
}

/* -------------------------------------------------------------------------- */
/* transfer_forecast — prevision de Y dado el modelo de X                     */
/*                                                                            */
/* El VARMA bivariante prevé sus DOS series con el motor de drvarma:          */
/*    serie 1 = w_Y - transferencia = el RUIDO N                              */
/*    serie 2 = w_X                 = la ENTRADA                              */
/* De ahi se recompone la salida:                                             */
/*    w_Y(n+l) = N(n+l) + SUM_k v_k * w_X(n+l-k)                              */
/* usando w_X observada para el pasado y PREVISTA para el futuro (por eso     */
/* prever Y exige prever X: es la diferencia con una regresion).              */
/*                                                                            */
/* Varianza del error. Y se alimenta de DOS fuentes de innovacion, y con la   */
/* covarianza diagonal son independientes:                                    */
/*    w_Y(t) = nu(B)*psi_X(B) a_X(t) + psi_N(B) a_N(t)                        */
/* luego, con g = nu * psi_X (convolucion),                                   */
/*    Var(l) = Sigma_N * SUM_{i<l} psi_N(i)^2  +  Sigma_X * SUM_{i<l} g(i)^2  */
/* El segundo termino solo aparece cuando el horizonte alcanza retardos de X  */
/* que hay que prever: con retardo puro b, los primeros b pasos NO lo tienen. */
/*                                                                            */
/* Nivel: se integra con el operador no estacionario (rnsop), se le suma el   */
/* componente determinista futuro (que se CONOCE: son funciones del tiempo) y */
/* se deshace el reescalado y la Box-Cox.                                     */
/* -------------------------------------------------------------------------- */
static void transfer_forecast(real *x, int npar, int L, real sigma2, FILE *out)
{
    struct Tvarma vf;
    int    ifault = 0;
    int    m = 2, i, j, k, l, t, p, q;
    int    nobs   = TsY.nobs;
    int    ornsop = TmY.ornsop;
    int    K      = n_stat + L + 1;      /* pesos nu suficientes */

    real **sigma, **f1, ***v1, ***v2, ***v3, ***psi;
    real  *nu, *omega, *delta, *psiN, *psiX, *g, *u, *UN, *Ug;
    real  *detY, *bc, *lvl, *sd;
    real   SN, SX;

    /* --- 1. Reconstruir el VARMA estimado y su covarianza --- */
    shootx(x, &vf, &ifault, 1, 0);
    if (ifault != 0) { fprintf(out, "\nCould not build the model for forecasting.\n"); return; }

    p = vf.p; q = vf.q;

    sigma = matrix(1, m, 1, m);
    for (i = 1; i <= m; i++)
        for (j = 1; j <= m; j++)
            sigma[i][j] = sigma2 * vf.qq[i][j];
    SN = sigma[1][1];   /* varianza de la innovacion del RUIDO   */
    SX = sigma[2][2];   /* varianza de la innovacion de la ENTRADA */

    /* --- 2. Prevision del VARMA (motor de drvarma) --- */
    f1 = matrix(1, m, 1, L);
    v1 = tensor(1, L, 1, m, 1, m);
    v2 = tensor(1, L, 1, m, 1, m);
    v3 = tensor(1, L, 1, m, 1, m);

    forecast_model(m, n_stat, p, q, vf.mu, vf.phi, vf.theta, sigma,
                   vf.w, vf.a, f1, v1, v2, v3, 0, L, TsY.freq, NULL);

    /* --- 3. Pesos nu(B) de la transferencia --- */
    omega = vector(0, MAX_S);
    delta = vector(0, MAX_R);
    nu    = vector(1, K);
    {
        int idx = 1;
        for (j = 0; j <= s_ord; j++) omega[j] = x[idx++];
        for (j = 1; j <= r_ord; j++) delta[j] = x[idx++];
    }
    /* misma recursion que en shootx: nu[j] pesa a w_X en el retardo j-1 */
    for (t = 1; t <= K; t++) {
        real sum = 0.0;
        int lag = t - 1 - b_delay;
        if (lag >= 0 && lag <= s_ord) sum = omega[lag];
        for (j = 1; j <= r_ord; j++)
            if (t > j) sum += delta[j] * nu[t - j];
        nu[t] = sum;
    }

    /* --- 4. Prevision de w_Y = N + transferencia --- */
    {
        real *wYf = vector(1, L);
        real *wXe = vector(1, n_stat + L);      /* entrada: observada + prevista */

        for (t = 1; t <= n_stat; t++)      wXe[t] = w_X[t];
        for (l = 1; l <= L; l++)           wXe[n_stat + l] = f1[2][l];

        for (l = 1; l <= L; l++) {
            real tr = 0.0;
            int  tt = n_stat + l;
            for (j = 1; j <= tt && j <= K; j++)
                tr += nu[j] * wXe[tt - j + 1];
            wYf[l] = f1[1][l] + tr;             /* f1[1] ya incluye mu_Y */
        }

        /* --- 5. Pesos psi y varianza del error de prevision --- */
        psi  = tensor(0, L, 1, m, 1, m);
        compute_psi_weights(m, p, q, vf.phi, vf.theta, L, psi);

        psiN = vector(0, L);
        psiX = vector(0, L);
        g    = vector(0, L);
        for (i = 0; i <= L; i++) {
            psiN[i] = psi[i][1][1];             /* VARMA diagonal */
            psiX[i] = psi[i][2][2];
        }
        /* g = nu * psi_X   (v_k = nu[k+1]) */
        for (i = 0; i <= L; i++) {
            real sum = 0.0;
            for (k = 0; k <= i; k++)
                if (k + 1 <= K) sum += nu[k + 1] * psiX[i - k];
            g[i] = sum;
        }

        /* --- 6. Integracion al nivel --- */
        detY = vector(1, nobs + L);
        bc   = vector(1, nobs + L);
        lvl  = vector(1, L);
        sd   = vector(1, L);

        build_det_component(&TmY, &TsY, nobs + L, detY);

        /* historia observada en la escala Box-Cox * reescalado, sin deterministas */
        for (t = 1; t <= nobs; t++) {
            real y = TsY.data[t];
            real b0 = (fabs(TmY.boxlam) < 1e-8)
                    ? log(y) * TsY.refactor
                    : ((pow(y, TmY.boxlam) - 1.0) / TmY.boxlam) * TsY.refactor;
            bc[t] = b0 - detY[t];               /* = "detrended" */
        }

        /* deshacer la diferenciacion: w[t] = SUM_j (-rnsop[j]) d[t+ornsop-j] */
        for (l = 1; l <= L; l++) {
            real acc = wYf[l];
            int  tt  = nobs + l;
            for (j = 1; j <= ornsop; j++)
                acc -= (-TmY.rnsop[j]) * bc[tt - j];
            bc[tt] = acc;
        }

        /* --- 7. Pesos psi del NIVEL: convolucion con 1/rnsop(B) --- */
        u  = vector(0, L);
        UN = vector(0, L);
        Ug = vector(0, L);
        u[0] = 1.0;
        for (i = 1; i <= L; i++) {
            real sum = 0.0;
            for (j = 1; j <= ornsop && j <= i; j++)
                sum += (-TmY.rnsop[j]) * u[i - j];
            u[i] = -sum;
        }
        for (i = 0; i <= L; i++) {
            real sN = 0.0, sG = 0.0;
            for (k = 0; k <= i; k++) {
                sN += u[k] * psiN[i - k];
                sG += u[k] * g[i - k];
            }
            UN[i] = sN;
            Ug[i] = sG;
        }

        /* --- 8. Informe --- */
        fprintf(out, "\n");
        fprintf(out, "=============================================================\n");
        fprintf(out, "  FORECAST OF Y GIVEN THE MODEL OF X                          \n");
        fprintf(out, "=============================================================\n");
        fprintf(out, "  Forecasting Y requires forecasting X: the transfer needs the\n");
        fprintf(out, "  future of the input. The forecast error of Y therefore has TWO\n");
        fprintf(out, "  sources: the noise innovation and the input innovation, the\n");
        fprintf(out, "  latter propagated through nu(B).\n\n");
        fprintf(out, "  Stationary series (w) and LEVEL, with 95%% bands:\n\n");
        fprintf(out, "   l     w_Y fcst    sd(w)   |     LEVEL         lower         upper\n");
        fprintf(out, "  ---------------------------------------------------------------------\n");

        for (l = 1; l <= L; l++) {
            real vw = 0.0, vl = 0.0, lo, hi, center;

            for (i = 0; i <= l - 1; i++) {
                vw += SN * psiN[i] * psiN[i] + SX * g[i]  * g[i];
                vl += SN * UN[i]   * UN[i]   + SX * Ug[i] * Ug[i];
            }
            sd[l] = sqrt(vl);

            /* volver al nivel original: sumar deterministas, deshacer escala y Box-Cox */
            center = bc[nobs + l] + detY[nobs + l];
            lo     = center - 1.96 * sd[l];
            hi     = center + 1.96 * sd[l];

            if (fabs(TmY.boxlam) < 1e-8) {
                lvl[l] = exp(center / TsY.refactor);
                lo     = exp(lo / TsY.refactor);
                hi     = exp(hi / TsY.refactor);
            } else {
                real lam = TmY.boxlam;
                lvl[l] = pow(lam * (center / TsY.refactor) + 1.0, 1.0 / lam);
                lo     = pow(lam * (lo     / TsY.refactor) + 1.0, 1.0 / lam);
                hi     = pow(lam * (hi     / TsY.refactor) + 1.0, 1.0 / lam);
            }

            fprintf(out, "  %3d  %10.4f  %8.4f  |  %10.4f  %10.4f  %10.4f\n",
                    l, wYf[l], sqrt(vw), lvl[l], lo, hi);
        }
        fprintf(out, "=============================================================\n\n");

        free_vector(Ug, 0, L); free_vector(UN, 0, L); free_vector(u, 0, L);
        free_vector(sd, 1, L); free_vector(lvl, 1, L);
        free_vector(bc, 1, nobs + L); free_vector(detY, 1, nobs + L);
        free_vector(g, 0, L); free_vector(psiX, 0, L); free_vector(psiN, 0, L);
        free_tensor(psi, 0, L, 1, m, 1, m);
        free_vector(wXe, 1, n_stat + L);
        free_vector(wYf, 1, L);
    }

    free_vector(nu, 1, K);
    free_vector(delta, 0, MAX_R);
    free_vector(omega, 0, MAX_S);
    free_tensor(v3, 1, L, 1, m, 1, m);
    free_tensor(v2, 1, L, 1, m, 1, m);
    free_tensor(v1, 1, L, 1, m, 1, m);
    free_matrix(f1, 1, m, 1, L);
    free_matrix(sigma, 1, m, 1, m);
    shootx(x, &vf, &ifault, 0, 1);
    (void)npar;
}

/* -------------------------------------------------------------------------- */
/* usage                                                                      */
/* -------------------------------------------------------------------------- */
static void usage(const char *prog)
{
    fprintf(stderr,
"DRTRAN %s: Box-Jenkins transfer function models by exact maximum likelihood\n"
"\n"
"A bridge between fue (univariate models) and drvarma (exact VARMA likelihood).\n"
"Reads two models already specified in fue (.pre) and estimates them JOINTLY:\n"
"\n"
"    Y_t = [omega(B)/delta(B)] B^b X_t + N_t\n"
"\n"
"Usage: %s output.pre input.pre [options]\n"
"       The FIRST file is the output (Y); the SECOND is the exogenous input (X).\n"
"\n"
"With no options, drtran runs the full Box-Jenkins cycle:\n"
"  1. prewhiten the input, read the CCF   -> propose (b, r, s)\n"
"  2. estimate everything jointly by exact ML\n"
"  3. validate (CCF noise vs input)       -> adequacy and exogeneity\n"
"\n"
"MODEL AND OUTPUT\n"
"  -m NAME  model name; results go to NAME.out\n"
"           (default: <output>_<input>, from the two .pre file names)\n"
"  -o FILE  write the results to FILE instead of NAME.out\n"
"\n"
"IDENTIFICATION\n"
"  -p       PREWHITEN ONLY: filter the input with its own ARMA, apply the same\n"
"           filter to the output, plot the CCF and suggest (b, r, s).\n"
"           Does NOT estimate and does NOT iterate.\n"
"\n"
"TRANSFER FUNCTION\n"
"  -b N     pure delay B^b                    (default: identified)\n"
"  -r N     order of the denominator delta(B) (default: identified)\n"
"  -s N     order of the numerator omega(B)   (default: identified)\n"
"  -0       NO transfer: fit the two univariate models jointly and diagonally.\n"
"           This is the homologation mode against fue (it must reproduce fue\n"
"           run separately on each series).\n"
"\n"
"WHAT IS ESTIMATED\n"
"  By default THE .pre RULES: every ARMA coefficient, every omega/delta of the\n"
"  deterministic variables and every mean is free or fixed according to its flag\n"
"  in the file (\"0.0000  0\" is a FIXED coefficient, not a starting value).\n"
"  To override:\n"
"  -N       fix the noise ARMA parameters (those of Y)\n"
"  -X       fix the input ARMA parameters\n"
"  -D       fix ALL deterministic coefficients of Y\n"
"  -E       fix ALL deterministic coefficients of X\n"
"  -M       fix both means at their .pre values\n"
"\n"
"FORECASTING\n"
"  -f L     forecast L periods ahead, with 95%% bands. Forecasting Y requires\n"
"           forecasting X: the error of Y adds the noise innovation and the\n"
"           input innovation, the latter propagated through nu(B).\n"
"\n"
"OTHER\n"
"  -v       optimizer trace\n"
"  -h       this help\n"
"\n"
"EXAMPLES\n"
"  %s CPI.pre WTI.pre -p              prewhiten only: look before committing\n"
"  %s CPI.pre WTI.pre                 full cycle: identify, estimate, validate\n"
"  %s CPI.pre WTI.pre -b 0 -s 1       impose (b, r, s)\n"
"  %s CPI.pre WTI.pre -0              homologation with fue (no transfer)\n"
"  %s CPI.pre WTI.pre -m oil -f 12    name the model and forecast 12 periods\n"
"\n"
"drtran is free software under the GNU General Public License v2 or later.\n"
"It embeds the exact VARMA likelihood engine of drvarma/ART. See COPYING.\n",
        DRTRAN_VERSION, prog, prog, prog, prog, prog, prog);
}

/* -------------------------------------------------------------------------- */
/* main                                                                       */
/* -------------------------------------------------------------------------- */
int main(int argc, char *argv[])
{
    int opt;
    char *outfile = NULL;
    int auto_id = 1;   /* 1 = no se dieron órdenes, ejecutar preblanqueo */
    int force_fix_mu = 0;   /* -M: fijar ambas medias, ignorando el .pre */
    int fc_horizon = 0;     /* -f L: horizonte de prevision (0 = no prever) */
    int prewhiten_only = 0; /* -p: solo preblanquear (filtrar + CCF), sin estimar */
    char *model_name = NULL;
    char outname[512];

    /* Inicializar variables globales del motor */
    macheps = cmacheps();
    outputv = stdout;

    /* --- Procesar argumentos --- */
    while ((opt = getopt(argc, argv, "r:s:b:f:m:p0XNDEMvho:")) != -1) {
        switch (opt) {
        case 'r': r_ord    = atoi(optarg); auto_id = 0; break;
        case 's': s_ord    = atoi(optarg); auto_id = 0; break;
        case 'b': b_delay  = atoi(optarg); auto_id = 0; break;
        case 'X': fix_X    = 1;           break;  /* fijar ARMA */
        case 'N': fix_noise = 1;          break;  /* fijar ARMA */
        case 'D': fix_det_Y = 1;          break;  /* fijar TODOS los det de Y */
        case 'E': fix_det_X = 1;          break;  /* fijar TODOS los det de X */
        case 'M': force_fix_mu = 1;       break;  /* fijar ambas medias */
        case 'f': fc_horizon = atoi(optarg); break;  /* horizonte de prevision */
        case 'm': model_name = optarg;    break;  /* nombre del modelo */
        case 'p': prewhiten_only = 1;     break;  /* solo preblanqueo */
        case '0': s_ord = -1; r_ord = 0; b_delay = 0; auto_id = 0;
                  break;  /* sin transferencia: dos univariantes conjuntos */
        case 'v': quiet_mode = 0;         break;
        case 'o': outfile  = optarg;      break;
        case 'h': usage(argv[0]); return 0;
        default:  usage(argv[0]); return 1;
        }
    }

    if (optind + 1 >= argc) {
        fprintf(stderr, "Error: se requieren dos archivos .pre "
                "(entrada y salida)\n");
        usage(argv[0]);
        return 1;
    }

    /* Nombre del modelo: -m, o por defecto <salida>_<entrada> a partir de los
       dos .pre (sin ruta ni extension). Los resultados van a <modelo>.out; la
       consola solo recibe un resumen.                                        */
    if (model_name == NULL) {
        char yb[128], xb[128];
        base_name(argv[optind],     yb, sizeof yb);
        base_name(argv[optind + 1], xb, sizeof xb);
        snprintf(outname, sizeof outname, "%s_%s", yb, xb);
        model_name = outname;
    }

    {
        char path[600];
        if (outfile != NULL) snprintf(path, sizeof path, "%s", outfile);
        else                 snprintf(path, sizeof path, "%s.out", model_name);

        outputv = fopen(path, "w");
        if (outputv == NULL) {
            fprintf(stderr, "Error opening output file: %s\n", path);
            return 1;
        }
        snprintf(outfile_path, sizeof outfile_path, "%s", path);
    }

    /* --- Banner y cabecera de consola (convencion de fue/drvarma) --- */
    printf("\n");
    printf("DRTRAN %s: Box-Jenkins transfer function models by exact ML\n",
           DRTRAN_VERSION);
    printf("Copyright (C) 1995-2026 A.B. Treadway, J.A. Mauricio & D.E. Guerrero\n");
    printf("Free software under the GNU GPL v2 or later; comes with NO WARRANTY.\n");
    printf("See the file COPYING for details.\n");
    printf("Non-final version. May contain errors. Please report.\n\n");
    printf("Model                  : %s\n", model_name);
    printf("Output (Y)             : %s\n", argv[optind]);
    printf("Input  (X)             : %s\n", argv[optind + 1]);
    printf("Results file           : %s\n", outfile_path);
    if (prewhiten_only)
        printf("Method                 : prewhitening only (no estimation)\n");
    else
        printf("Method                 : exact maximum likelihood "
               "(bivariate VARMA cast)\n");

    /* --- Leer los dos modelos univariantes --- */
    /* Convención: primer argumento = endógena (Y), segundo = exógena (X) */
    if (read_fue_pre(argv[optind], &TmY, &TsY, &DataMatY) != 0) {
        fprintf(stderr, "Error reading %s\n", argv[optind]);
        return 2;
    }
    if (read_fue_pre(argv[optind + 1], &TmX, &TsX, &DataMatX) != 0) {
        fprintf(stderr, "Error leyendo %s\n", argv[optind + 1]);
        return 3;
    }

    /* --- Medias: cada serie hereda del .pre su valor y su condición de
           libre/fija. FUE marca con un flag las medias que estima (p.ej.
           ES_CPI: "0.154472 1"); las que no forman parte del modelo vienen
           como un simple "0" (p.ej. WTI). -M fuerza a fijar ambas.        */
    mu_Y = TmY.mu;   fix_mu_Y = force_fix_mu ? 1 : !TmY.Imu;
    mu_X = TmX.mu;   fix_mu_X = force_fix_mu ? 1 : !TmX.Imu;

    /* Verificar que las longitudes coincidan */
    if (TsX.nobs != TsY.nobs) {
        fprintf(stderr,
                "Error: las series tienen distinto número de observaciones "
                "(X: %d, Y: %d)\n", TsX.nobs, TsY.nobs);
        return 4;
    }

    /* --- Series estacionarias w_X, w_Y, recortadas a la ventana común --- */
    build_stationary_pair();
    if (n_stat <= 0) {
        fprintf(stderr, "Error: could not build the stationary series\n");
        return 5;
    }

    /* --- Cabecera del .out --- */
    fprintf(outputv, "DRTRAN %s: Box-Jenkins transfer function models by exact ML\n",
            DRTRAN_VERSION);
    fprintf(outputv, "Copyright (C) 1995-2026 A.B. Treadway, J.A. Mauricio & D.E. Guerrero\n");
    fprintf(outputv, "Free software under the GNU GPL v2 or later; NO WARRANTY.\n\n");
    fprintf(outputv, "Model            : %s\n", model_name);
    fprintf(outputv, "Output (Y)       : %s\n", argv[optind]);
    fprintf(outputv, "Input  (X)       : %s\n", argv[optind + 1]);
    if (prewhiten_only)
        fprintf(outputv, "Transfer model   : (to be identified)\n");
    else
        fprintf(outputv, "Transfer model   : b=%d, r=%d, s=%d\n",
                b_delay, r_ord, s_ord);
    fprintf(outputv, "Diagonal AR/MA   : yes\n");
    fprintf(outputv, "Diagonal cov     : yes\n");
    fprintf(outputv, "Frequency        : %d\n", TsY.freq);
    fprintf(outputv, "Start            : %d %d\n", TsY.begtime, TsY.begyear);
    fprintf(outputv, "Box-Cox lambda   : %.2f (Y), %.2f (X)\n", TmY.boxlam, TmX.boxlam);
    fprintf(outputv, "Rescale factor   : %.0f (Y), %.0f (X)\n", TsY.refactor, TsX.refactor);
    fprintf(outputv, "Differences      : d=%d, D=%d (Y)   d=%d, D=%d (X)\n",
            TmY.nrdiff, TmY.nadiff, TmX.nrdiff, TmX.nadiff);
    fprintf(outputv, "Deterministics   : %d (Y), %d (X)\n", TmY.NdetVar, TmX.NdetVar);
    fprintf(outputv, "Observations     : %d (raw %d)\n\n", n_stat, TsY.nobs);
    fflush(outputv);

    /* --- Extraer parámetros ARMA fijos --- */
    p_X = total_ar_order(&TmX);  q_X = total_ma_order(&TmX);
    p_N = total_ar_order(&TmY);  q_N = total_ma_order(&TmY);

    if (p_X > 0) {
        phi_X = vector(1, p_X);
        expand_ar_factors(&TmX, phi_X, p_X);
    }
    if (q_X > 0) {
        theta_X = vector(1, q_X);
        expand_ma_factors(&TmX, theta_X, q_X);
    }
    if (p_N > 0) {
        phi_N = vector(1, p_N);
        expand_ar_factors(&TmY, phi_N, p_N);
    }
    if (q_N > 0) {
        theta_N = vector(1, q_N);
        expand_ma_factors(&TmY, theta_N, q_N);
    }

    /* --- Preblanqueo e identificación (si no se dieron órdenes) --- */
    if (auto_id || prewhiten_only) {
        prewhiten_and_identify(w_X, w_Y, n_stat,
                               phi_X, p_X, theta_X, q_X,
                               &r_ord, &s_ord, &b_delay, outputv);
    }

    /* ── -p: SOLO PREBLANQUEO ──────────────────────────────────────────
       Filtra y dibuja la CCF. No estima, no itera: es el paso de
       identificacion aislado, para mirar la relacion antes de comprometerse
       con una especificacion.                                              */
    if (prewhiten_only) {
        printf("Observations           : %d\n", n_stat);
        printf("\n**** PREWHITENING ONLY: no estimation performed\n\n");
        printf("Suggested transfer function orders:\n");
        printf("  b (delay)             : %d\n", b_delay);
        printf("  r (denominator)       : %d\n", r_ord);
        printf("  s (numerator)         : %d\n", s_ord);
        printf("\nCCF plot and impulse response weights written to %s\n\n",
               outfile_path);
        fclose(outputv);
        return 0;
    }

    fprintf(outputv, "\nTransfer function orders:\n");
    fprintf(outputv, "  b (delay)       = %d\n", b_delay);
    fprintf(outputv, "  r (denominator) = %d\n", r_ord);
    fprintf(outputv, "  s (numerator)   = %d\n", s_ord);

    if (r_ord > MAX_R) {
        fprintf(stderr, "Error: r=%d exceeds MAX_R=%d\n", r_ord, MAX_R);
        return 8;
    }
    if (s_ord > MAX_S) {
        fprintf(stderr, "Error: s=%d exceeds MAX_S=%d\n", s_ord, MAX_S);
        return 8;
    }

    /* --- Calcular número de parámetros --- */
    {
        int npar = (s_ord + 1) + r_ord;   /* ω₀…ω_s, δ₁…δ_r */
        int ndet_Y = (!fix_det_Y) ? n_det_free_params(&TmY) : 0;
        int ndet_X = (!fix_det_X) ? n_det_free_params(&TmX) : 0;

        if (!fix_noise)  npar += n_ar_free_params(&TmY) + n_ma_free_params(&TmY);
        if (!fix_X)      npar += n_ar_free_params(&TmX) + n_ma_free_params(&TmX);
        npar += ndet_Y + ndet_X;   /* coefs deterministas */
        if (!fix_mu_Y)   npar += 1;
        if (!fix_mu_X)   npar += 1;
        npar += 1;   /* log(var_X/var_Y): la escala la concentra sigma2 */

        fprintf(outputv, "Parameters to estimate: %d\n", npar);
        fprintf(outputv, "  fix_noise=%d, fix_X=%d, fix_det_Y=%d, fix_det_X=%d,"
                         " fix_mu_Y=%d, fix_mu_X=%d\n\n",
                fix_noise, fix_X, fix_det_Y, fix_det_X, fix_mu_Y, fix_mu_X);

        /* --- Construir vector de parámetros iniciales --- */
        {
            real *x = vector(1, npar);
            int idx = 1;
            int j;

            /* ω₀…ω_s: inicializar en cero */
            for (j = 0; j <= s_ord; j++) x[idx++] = 0.0;
            /* δ₁…δ_r: inicializar en cero */
            for (j = 1; j <= r_ord; j++) x[idx++] = 0.0;

            /* φ_N (ruido): factores ARMA (no expandidos) */
            if (!fix_noise) {
                pack_ar_factors(&TmY, x, idx);
                idx += n_ar_free_params(&TmY);
                pack_ma_factors(&TmY, x, idx);
                idx += n_ma_free_params(&TmY);
            }

            /* φ_X: factores ARMA (no expandidos) */
            if (!fix_X) {
                pack_ar_factors(&TmX, x, idx);
                idx += n_ar_free_params(&TmX);
                pack_ma_factors(&TmX, x, idx);
                idx += n_ma_free_params(&TmX);
            }

            /* Coeficientes deterministas ω/δ: inicializar con los de FUE */
            if (!fix_det_Y) idx += pack_det_params(&TmY, x, idx);
            if (!fix_det_X) idx += pack_det_params(&TmX, x, idx);

            /* Medias y varianzas: inicializar con los momentos muestrales de w.
               Las varianzas DEBEN arrancar en su orden de magnitud real: el paso
               de diferencias finitas de cdgrad es eta^(1/3)*max(|x|,1) ≈ 6e-6
               absoluto, así que un arranque muy por debajo de la escala de la
               serie deja el parámetro fuera del alcance del optimizador.       */
            {
                double mY = 0.0, mX = 0.0, vY = 0.0, vX = 0.0;
                int t;
                for (t = 1; t <= n_stat; t++) { mY += w_Y[t]; mX += w_X[t]; }
                mY /= n_stat; mX /= n_stat;
                for (t = 1; t <= n_stat; t++) {
                    vY += (w_Y[t] - mY) * (w_Y[t] - mY);
                    vX += (w_X[t] - mX) * (w_X[t] - mX);
                }

                if (!fix_mu_Y) x[idx++] = mY;   /* mu_Y inicial */
                if (!fix_mu_X) x[idx++] = mX;   /* mu_X inicial */

                /* log(var_X/var_Y) inicial, a partir de las varianzas muestrales */
                x[idx++] = log((vX / n_stat) / (vY / n_stat));
            }

            /* --- Configurar optimizador --- */
            {
                struct Tvarma varma1;
                real *dev = vector(1, npar);
                real **cov = matrix(1, npar, 1, npar);
                real **a_est = matrix(1, n_stat, 1, 2);  /* m=2 */
                int maxits = 500, nrits = 10, ifault = 0;
                real gradtol = 1e-7, steptol = 1e-7;

                varma1.xitol = -1e-3;   /* verosimilitud exacta */

                fprintf(outputv, "Estimating transfer function model...\n");

                est(shootx, npar, x, dev, cov, maxits, nrits,
                    gradtol, steptol, varma1.xitol, a_est,
                    &varma1.sigma2, &varma1.logelf, &ifault);

                /* Informe honesto de la terminación: qnewtopt deja el número
                   real de iteraciones y el código de parada en opt_iters /
                   opt_termcode. Solo 1 y 2 son convergencia.                */
                {
                    const char *why, *verdict;

                    /* termcode (Dennis–Schnabel, umstop):
                         1,2 = convergencia (gradiente / paso)
                         3   = la búsqueda lineal no encontró un punto mejor. NO es
                               un fallo: es lo típico cuando se ARRANCA en el óptimo
                               (p.ej. con las preestimaciones de fue). Perturbando el
                               arranque, el mismo modelo converge por gradiente al
                               MISMO óptimo.
                         4,5 = fallo real (límite de iteraciones / pasos máximos) */
                    switch (opt_termcode) {
                    case 1:  verdict = "CONVERGENCE OBTAINED";
                             why = "gradient stopping criterium satisfied";       break;
                    case 2:  verdict = "CONVERGENCE OBTAINED";
                             why = "parameter stopping criterium satisfied";      break;
                    case 3:  verdict = "STOPPED AT A POINT WITH NO IMPROVEMENT";
                             why = "last global step failed to locate a lower point "
                                   "(usual when starting AT the optimum)";        break;
                    case 4:  verdict = "*** NO CONVERGENCE ***";
                             why = "ITERATION LIMIT REACHED";                     break;
                    case 5:  verdict = "*** NO CONVERGENCE ***";
                             why = "five consecutive steps of max length";        break;
                    default: verdict = "*** NO CONVERGENCE ***";
                             why = "unknown";                                     break;
                    }

                    sum_logl  = varma1.logelf;
                    sum_npar  = npar;
                    sum_conv  = verdict;
                    sum_why   = why;
                    sum_fault = ifault;

                    fprintf(outputv, "\n**** %s AFTER %d ITERATIONS (of %d)\n",
                            verdict, opt_iters, maxits);
                    fprintf(outputv, "**** %s\n", why);
                    if (ifault != 0)
                        fprintf(outputv, "**** ifault = %d (estimates not reliable)\n", ifault);
                    fprintf(outputv, "\nLog-likelihood = %.6f\n\n", varma1.logelf);
                }

                /* --- Tabla de parámetros estilo drvarma --- */
                {
                    real tstat, pval;
                    int pi;  /* parameter index */

                    fprintf(outputv, "=============================================================\n");
                    fprintf(outputv, "  Estimated Parameters and Standard Deviations               \n");
                    fprintf(outputv, "=============================================================\n\n");
                    fprintf(outputv, "                        Parameter     Estimate    Std.Error   t-stat  p-val\n");
                    fprintf(outputv, "--------------------------------------------------------------------\n");

                    pi = 1;

                    /* Transfer numerator ω₀...ω_s */
                    for (j = 0; j <= s_ord; j++) {
                        dev[pi] = (cov[pi][pi] > 0) ? sqrt(cov[pi][pi]) : 0.0;
                        tstat = (dev[pi] > 1e-15) ? x[pi] / dev[pi] : 0.0;
                        pval  = 2.0 * (1.0 - normal_cdf(fabs(tstat)));
                        fprintf(outputv, "omega[%d]             %12.6f %12.6f %8.3f %6.4f %s\n",
                                j, x[pi], dev[pi], tstat, pval,
                                (pval < 0.001) ? "***" : (pval < 0.01) ? "**" :
                                (pval < 0.05) ? "*" : (pval < 0.1) ? "." : "");
                        pi++;
                    }

                    /* Transfer denominator δ₁...δ_r */
                    for (j = 1; j <= r_ord; j++) {
                        dev[pi] = (cov[pi][pi] > 0) ? sqrt(cov[pi][pi]) : 0.0;
                        tstat = (dev[pi] > 1e-15) ? x[pi] / dev[pi] : 0.0;
                        pval  = 2.0 * (1.0 - normal_cdf(fabs(tstat)));
                        fprintf(outputv, "delta[%d]             %12.6f %12.6f %8.3f %6.4f %s\n",
                                j, x[pi], dev[pi], tstat, pval,
                                (pval < 0.001) ? "***" : (pval < 0.01) ? "**" :
                                (pval < 0.05) ? "*" : (pval < 0.1) ? "." : "");
                        pi++;
                    }

                    /* ARMA del ruido (modelo de Y) */
                    if (!fix_noise) {
                        print_arma_factors(&TmY, "N", 1, x, cov, dev, &pi, outputv);
                        print_arma_factors(&TmY, "N", 0, x, cov, dev, &pi, outputv);
                    } else {
                        for (j = 1; j <= p_N; j++)
                            fprintf(outputv, "phi_N[B^%-2d]     (fixed %12.6f)\n", j, phi_N[j]);
                        for (j = 1; j <= q_N; j++)
                            fprintf(outputv, "theta_N[B^%-2d]   (fixed %12.6f)\n", j, theta_N[j]);
                    }

                    /* ARMA de la entrada (modelo de X) */
                    if (!fix_X) {
                        print_arma_factors(&TmX, "X", 1, x, cov, dev, &pi, outputv);
                        print_arma_factors(&TmX, "X", 0, x, cov, dev, &pi, outputv);
                    } else {
                        for (j = 1; j <= p_X; j++)
                            fprintf(outputv, "phi_X[B^%-2d]     (fixed %12.6f)\n", j, phi_X[j]);
                        for (j = 1; j <= q_X; j++)
                            fprintf(outputv, "theta_X[B^%-2d]   (fixed %12.6f)\n", j, theta_X[j]);
                    }

                    /* Deterministic coefficients */
                    /* Deterministas: ω_i(j) y δ_i(j) de cada variable, solo los
                       que el .pre marca como estimables.                     */
                    {
                        int s;
                        struct Tusmodel *Tms[2]; const char *tag[2]; int skip[2];
                        Tms[0] = &TmY; tag[0] = "Y"; skip[0] = fix_det_Y;
                        Tms[1] = &TmX; tag[1] = "X"; skip[1] = fix_det_X;

                        for (s = 0; s < 2; s++) {
                            int i2, j2;
                            if (skip[s]) continue;
                            for (i2 = 1; i2 <= Tms[s]->NdetVar; i2++) {
                                for (j2 = 0; j2 <= Tms[s]->Nomega[i2]; j2++) {
                                    if (Tms[s]->Imega[i2][j2] != 1) continue;
                                    dev[pi] = (cov[pi][pi] > 0) ? sqrt(cov[pi][pi]) : 0.0;
                                    tstat = (dev[pi] > 1e-15) ? x[pi] / dev[pi] : 0.0;
                                    pval  = 2.0 * (1.0 - normal_cdf(fabs(tstat)));
                                    fprintf(outputv, "omega_%s[%d,%d]       %12.6f %12.6f %8.3f %6.4f %s\n",
                                            tag[s], i2, j2, x[pi], dev[pi], tstat, pval,
                                            (pval < 0.001) ? "***" : (pval < 0.01) ? "**" :
                                            (pval < 0.05) ? "*" : (pval < 0.1) ? "." : "");
                                    pi++;
                                }
                                for (j2 = 1; j2 <= Tms[s]->Ndelta[i2]; j2++) {
                                    if (Tms[s]->Ielta[i2][j2] != 1) continue;
                                    dev[pi] = (cov[pi][pi] > 0) ? sqrt(cov[pi][pi]) : 0.0;
                                    tstat = (dev[pi] > 1e-15) ? x[pi] / dev[pi] : 0.0;
                                    pval  = 2.0 * (1.0 - normal_cdf(fabs(tstat)));
                                    fprintf(outputv, "delta_%s[%d,%d]       %12.6f %12.6f %8.3f %6.4f %s\n",
                                            tag[s], i2, j2, x[pi], dev[pi], tstat, pval,
                                            (pval < 0.001) ? "***" : (pval < 0.01) ? "**" :
                                            (pval < 0.05) ? "*" : (pval < 0.1) ? "." : "");
                                    pi++;
                                }
                            }
                        }
                    }

                    /* Means (cada serie por separado) */
                    {
                        int k;
                        int fixed_mu[2]; real mu_val[2];
                        fixed_mu[0] = fix_mu_Y;  mu_val[0] = mu_Y;
                        fixed_mu[1] = fix_mu_X;  mu_val[1] = mu_X;

                        for (k = 0; k < 2; k++) {
                            if (fixed_mu[k]) {
                                fprintf(outputv, "mu[%d]           (fixed %12.6f)\n",
                                        k+1, mu_val[k]);
                                continue;
                            }
                            dev[pi] = (cov[pi][pi] > 0) ? sqrt(cov[pi][pi]) : 0.0;
                            tstat = (dev[pi] > 1e-15) ? x[pi] / dev[pi] : 0.0;
                            pval  = 2.0 * (1.0 - normal_cdf(fabs(tstat)));
                            fprintf(outputv, "mu[%d]                %12.6f %12.6f %8.3f %6.4f %s\n",
                                    k+1, x[pi], dev[pi], tstat, pval,
                                    (pval < 0.001) ? "***" : (pval < 0.01) ? "**" :
                                    (pval < 0.05) ? "*" : (pval < 0.1) ? "." : "");
                            pi++;
                        }
                    }

                    /* Razón de varianzas: el único parámetro de escala estimable
                       (la escala global la concentra sigma2). */
                    {
                        dev[pi] = (cov[pi][pi] > 0) ? sqrt(cov[pi][pi]) : 0.0;
                        tstat = (dev[pi] > 1e-15) ? x[pi] / dev[pi] : 0.0;
                        pval  = 2.0 * (1.0 - normal_cdf(fabs(tstat)));
                        fprintf(outputv, "log(varX/varY)       %12.6f %12.6f %8.3f %6.4f %s\n",
                                x[pi], dev[pi], tstat, pval,
                                (pval < 0.001) ? "***" : (pval < 0.01) ? "**" :
                                (pval < 0.05) ? "*" : (pval < 0.1) ? "." : "");
                    }

                    fprintf(outputv, "--------------------------------------------------------------------\n");
                    fprintf(outputv, "Signif. codes:  0 '***' 0.001 '**' 0.01 '*' 0.05 '.' 0.1 ' ' 1\n\n");

                    /* Q es la covarianza NORMALIZADA (Q[1,1] = 1); la covarianza
                       real es Sigma = sigma2 * Q, con sigma2 el factor de escala
                       que est() concentra. drvarma imprime ambas: igual aquí.   */
                    fprintf(outputv, "sigma2 (concentrada) = %.6f\n\n", varma1.sigma2);
                    fprintf(outputv, "Sigma = sigma2 * Q  (covarianza de las innovaciones):\n");
                    fprintf(outputv, "  Sigma[1,1] (noise) = %12.6f\n",
                            varma1.sigma2 * 1.0);
                    fprintf(outputv, "  Sigma[2,2] (input) = %12.6f\n\n",
                            varma1.sigma2 * exp(x[npar]));
                }

                /* ─────────── Diagnóstico con funciones de drvarma ─────────── */
                {
                    struct Tvarma vdiag;
                    int ifault_diag = 0;
                    int t;

                    /* Reconstruir el VARMA con los parámetros finales */
                    shootx(x, &vdiag, &ifault_diag, 1, 0);

                    if (ifault_diag == 0)
                    {
                        /* Copiar residuos estimados al VARMA de diagnóstico */
                        for (t = 1; t <= n_stat; t++) {
                            vdiag.a[t][1] = a_est[t][1];
                            vdiag.a[t][2] = a_est[t][2];
                        }

                        fprintf(outputv, "\n");
                        diagnose(&vdiag);

                        fprintf(outputv, "\n");
                        fprintf(outputv, "--- Multivariate diagnostics (Hosking + JB) ---\n");
                        multivariate_diagnostics(a_est, n_stat, 2, outputv);

                        /* Adecuación de la transferencia: el ruido no debe
                           conservar huella de la entrada preblanqueada. Solo
                           tiene sentido si hay transferencia (s >= 0).       */
                        if (s_ord >= 0)
                            transfer_adequacy(a_est, n_stat, b_delay, r_ord,
                                              s_ord, outputv,
                                              &sum_p_transfer, &sum_p_exog);

                        if (fc_horizon > 0)
                            transfer_forecast(x, npar, fc_horizon,
                                              varma1.sigma2, outputv);

                        /* Liberar el VARMA de diagnóstico */
                        shootx(x, &vdiag, &ifault_diag, 0, 1);
                    }
                }

                /* ── Resumen de consola ────────────────────────────────────
                   Escueto y en inglés, como fue/drvarma. El detalle esta en el .out. */
                {
                    int j2;
                    printf("Observations           : %d\n", n_stat);
                    printf("Parameters             : %d\n", sum_npar);
                    printf("Transfer (b, r, s)     : (%d, %d, %d)%s\n",
                           b_delay, r_ord, s_ord,
                           auto_id ? "  [identified by prewhitening]" : "  [imposed]");
                    printf("\n**** %s AFTER %d ITERATIONS\n", sum_conv, opt_iters);
                    printf("**** %s\n", sum_why);
                    if (sum_fault)
                        printf("**** ifault = %d (estimates not reliable)\n", sum_fault);
                    printf("\nLog-likelihood         : %.6f\n\n", sum_logl);

                    printf("Estimated model\n");
                    if (s_ord >= 0) {
                        printf("  Transfer  : nu(B) = omega(B)/delta(B) * B^%d\n", b_delay);
                        for (j2 = 0; j2 <= s_ord; j2++)
                                printf("                omega_%d = %10.6f  (t = %6.2f)\n", j2,
                                       x[j2 + 1], dev[j2+1] > 1e-15 ? x[j2+1]/dev[j2+1] : 0.0);
                        for (j2 = 1; j2 <= r_ord; j2++)
                                printf("                delta_%d = %10.6f  (t = %6.2f)\n", j2,
                                       x[s_ord + 1 + j2],
                                       dev[s_ord+1+j2] > 1e-15 ? x[s_ord+1+j2]/dev[s_ord+1+j2] : 0.0);
                    } else {
                        printf("  Transfer  : none (two univariate models, joint diagonal fit)\n");
                    }
                    printf("  Noise (Y) : AR order %d, MA order %d, %d deterministic(s)%s\n",
                           p_N, q_N, TmY.NdetVar, fix_mu_Y ? ", mean fixed" : ", mean free");
                    printf("  Input (X) : AR order %d, MA order %d, %d deterministic(s)%s\n",
                           p_X, q_X, TmX.NdetVar, fix_mu_X ? ", mean fixed" : ", mean free");

                    if (sum_p_transfer >= 0.0) {
                        printf("\nDiagnostics\n");
                        printf("  Transfer adequacy   : %-9s (p = %.4f)\n",
                                   sum_p_transfer < 0.05 ? "INADEQUATE" : "adequate",
                                   sum_p_transfer);
                        printf("  Input exogeneity    : %-9s (p = %.4f)\n",
                                   sum_p_exog < 0.05 ? "FEEDBACK!" : "ok",
                                   sum_p_exog);
                    }
                    printf("\nFull results written to %s\n\n", outfile_path);
                }

                /* Liberar memoria del optimizador */
                free_matrix(a_est, 1, n_stat, 1, 2);
                free_matrix(cov, 1, npar, 1, npar);
                free_vector(dev, 1, npar);
            }

            free_vector(x, 1, npar);
        }
    }

    /* --- Liberar memoria --- */
    /* (simplificado: en producción haría falta liberar todas las estructuras) */

    if (outfile != NULL && outputv != stdout)
        fclose(outputv);

    return 0;
}
