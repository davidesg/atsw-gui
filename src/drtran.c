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
#include <strings.h>  /* strcasecmp */
#include <ctype.h>
#include <stdarg.h>

/* -------------------------------------------------------------------------- */
/* Definición de variables globales del modelo                                */
/* -------------------------------------------------------------------------- */
/* Indice 1 = SALIDA (Y); 2..n_ser = ENTRADAS. La entrada j es la serie j+1. */
int n_ser = 2;
int n_inp = 1;

struct Tusmodel Tm[MAX_SER + 1];
struct Tseries  Ts[MAX_SER + 1];
real **DataMat[MAX_SER + 1];
real  *w[MAX_SER + 1];
int    n_stat = 0;

int   p_ord[MAX_SER + 1], q_ord[MAX_SER + 1];
real *phi[MAX_SER + 1], *theta[MAX_SER + 1];

real mu[MAX_SER + 1];
int  fix_mu[MAX_SER + 1];
int  fix_arma[MAX_SER + 1];
int  fix_det[MAX_SER + 1];

/* Transferencia de la entrada j (j = 1..n_inp) */
int b_del[MAX_SER + 1], r_ord[MAX_SER + 1], s_ord[MAX_SER + 1];

/* --------------------------------------------------------------------------
   AGREGADOS: combinaciones lineales de las series, con su banda.

   Las identidades contables (OCUPADOS = suma de sectores; PARADOS = ACTIVOS -
   OCUPADOS) NO entran en el modelo: se calculan DESPUES de prever, como hacia
   el legacy. Lo que no es trivial es la banda: los errores de prevision de las
   series estan CORRELACIONADOS -- comparten innovaciones a traves de la red --
   asi que la varianza del agregado no es la suma de las varianzas. Es c'Vc.
   -------------------------------------------------------------------------- */
#define MAX_AGGR 8
static char aggr_name[MAX_AGGR + 1][40];
static real aggr_c[MAX_AGGR + 1][MAX_SER + 1];
static int  n_aggr = 0;

/* LA RED: cada enlace es una transferencia de lnk[k].inp a lnk[k].out.      */
struct Tlink lnk[MAX_LINK + 1];
int n_link = 0;
int topo[MAX_SER + 1];

/* Prevision recursiva: ventana de estimacion y fichero de errores por origen. */
static int  rec_start = 0;
static int  nobs_full[MAX_SER + 1];
static char rec_csv[600] = "";

/* El cast: 0 = restar la transferencia; 1 = EMPOTRARLA en el VARMA (-V). */
int embed_varma = 0;

/* ¿La red es la ESTRELLA por defecto (todo entra a la serie 1)? Solo entonces
   tiene sentido hablar de "la entrada j" y "la salida Y".                    */
static int net_is_star(void)
{
    int k;
    if (n_link != n_inp) return 0;
    for (k = 1; k <= n_link; k++)
        if (lnk[k].out != 1 || lnk[k].inp != k + 1) return 0;
    return 1;
}

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
static real sum_gain[MAX_LINK + 1];
static real link_p_transfer[MAX_LINK + 1];
static real sum_mlag[MAX_LINK + 1];
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
static void trim_to_common(real *v, int nstat, int ncommon)
{
    int t, off = nstat - ncommon;
    if (off <= 0) return;
    for (t = 1; t <= ncommon; t++) v[t] = v[t + off];
}

/* Construye la serie estacionaria de TODAS las series y las recorta a la
   VENTANA COMUN: cada modelo pierde tantas observaciones iniciales como el
   orden de su propio operador no estacionario. Todas arrancan en la misma
   fecha, asi que alinear por el final las alinea en el calendario.          */
void build_stationary_series(void)
{
    int nst[MAX_SER + 1];
    int i, nmin = 0;

    for (i = 1; i <= n_ser; i++) {
        if (w[i]) free_vector(w[i], 1, n_stat);
        w[i] = NULL;
    }
    for (i = 1; i <= n_ser; i++) {
        nst[i] = 0;
        apply_univariate_model(&Tm[i], &Ts[i], DataMat[i], &w[i], &nst[i]);
        if (w[i] == NULL || nst[i] <= 0) { n_stat = 0; return; }
        if (nmin == 0 || nst[i] < nmin) nmin = nst[i];
    }
    n_stat = nmin;
    for (i = 1; i <= n_ser; i++) trim_to_common(w[i], nst[i], n_stat);
}

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
void prewhiten_and_identify(int j, FILE *outputv)
{
    int    si  = lnk[j].inp;            /* serie que EMITE  el enlace j */
    int    so  = lnk[j].out;            /* serie que RECIBE el enlace j */
    real  *w_X = w[si];
    real  *w_Y = w[so];
    real  *phi_X_in   = phi[si];
    real  *theta_X_in = theta[si];
    int    p_X_in = p_ord[si];
    int    q_X_in = q_ord[si];
    int    n = n_stat;
    int   *r = &lnk[j].r;
    int   *s = &lnk[j].s;
    int   *b = &lnk[j].b;

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
    fprintf(outputv, "  Link %d:  %s  ->  %s\n", j,
            Ts[si].name ? Ts[si].name : "", Ts[so].name ? Ts[so].name : "");
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
        ser.freq = Ts[si].freq;
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
static void transfer_adequacy(real **a, int n, int j, FILE *out,
                              real *p_transfer, real *p_exog)
{
    int si = lnk[j].inp;                /* serie que EMITE  el enlace j */
    int so = lnk[j].out;                /* serie que RECIBE el enlace j */
    int r  = lnk[j].r;
    int s  = lnk[j].s;
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

    for (t = 1; t <= n; t++) { aN[t] = a[t][so]; aX[t] = a[t][si]; }

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
    fprintf(out, "  TRANSFER FUNCTION ADEQUACY - input %d (%s)\n",
            j, Ts[si].name ? Ts[si].name : "");
    fprintf(out, "  CCF between the estimated noise and the prewhitened input    \n");
    fprintf(out, "=============================================================\n");
    fprintf(out, "  If (b, r, s) is correct, this CCF must be white noise.\n");
    fprintf(out, "    significant at k >= 0 -> structure missing in the TRANSFER\n");
    fprintf(out, "    significant at k <  0 -> FEEDBACK (X is not exogenous)\n\n");

    {
        struct Tseries ser;
        ser.nobs = n;
        ser.freq = Ts[si].freq;
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
                        nsig_pos, expected);
                fprintf(out, "     by chance, so they do not contradict the joint test:\n");
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


/* --------------------------------------------------------------------------
   EL ORDEN DE REFORMULACION.

   Munoz Polo (2001, sec. 2.6) establece una asimetria que no es obvia y que
   decide por donde empezar a arreglar un modelo:

     "La especificacion inadecuada de la relacion v(B) puede generar la
      apariencia (en acf/pacf residuales) de especificacion inadecuada del
      ruido [...]. Sin embargo, la especificacion inadecuada del ruido NO PUEDE
      dar la impresion en ccf de especificacion inadecuada de la relacion. Por
      estas razones, se reformula v(B) hasta que parezca adecuada ANTES de
      reformular theta(B)."

   Es decir: la contaminacion va en un solo sentido. Un ruido mal especificado
   no ensucia la CCF, pero una relacion mal especificada SI ensucia la ACF del
   ruido -- porque lo que sobra del input se queda dentro del ruido. Luego un
   ACF residual feo NO es evidencia contra el ruido mientras la CCF siga
   hablando. Arreglar el ruido primero es perseguir un sintoma.

   El programa imprimia los dos diagnosticos y callaba sobre cual mirar. Aqui
   los combina y lo dice.
   -------------------------------------------------------------------------- */
static void reformulation_advice(real **a, int n, FILE *out)
{
    int i, j, u, nlags, df;
    real *res, *corr, mean, var, Q, p_noise, p_rel;

    nlags = (n / 4 < 24) ? n / 4 : 24;
    if (nlags < 4) return;

    fprintf(out, "\n");
    fprintf(out, "=============================================================\n");
    fprintf(out, "  WHAT TO REFORMULATE, AND IN WHAT ORDER\n");
    fprintf(out, "=============================================================\n");
    fprintf(out, "  The contamination runs ONE WAY. A badly specified relation\n");
    fprintf(out, "  leaves part of the input inside the noise, so it DOES dirty the\n");
    fprintf(out, "  residual ACF. A badly specified noise CANNOT dirty the CCF.\n");
    fprintf(out, "  Therefore: fix the RELATION first; only then the noise. A poor\n");
    fprintf(out, "  residual ACF is not evidence against the noise while the CCF is\n");
    fprintf(out, "  still speaking.  (Munoz Polo 2001, sec. 2.6.)\n\n");

    res  = vector(1, n);
    corr = vector(1, nlags);

    for (u = 1; u <= n_ser; u++) {
        int is_out = 0;
        i = topo[u];
        for (j = 1; j <= n_link; j++)
            if (lnk[j].out == i && lnk[j].s >= 0) is_out = 1;
        if (!is_out) continue;

        /* --- el ruido de esta serie: portmanteau de sus residuos --- */
        mean = 0.0;
        for (j = 1; j <= n; j++) { res[j] = a[j][i]; mean += res[j]; }
        mean /= n;
        var = 0.0;
        for (j = 1; j <= n; j++) var += (res[j] - mean) * (res[j] - mean);
        var /= n;

        Acf(res, n, nlags, corr, mean, var);
        Q = 0.0;
        for (j = 1; j <= nlags; j++)
            Q += corr[j] * corr[j] / (n - j);
        Q *= n * (n + 2.0);

        /* Los grados de libertad se pierden por cada PARAMETRO estimado, no por
           cada grado del polinomio. Un SAR(1)_12 tiene grado 12 y UN parametro:
           restar 12 infla el estadistico y produce una falsa alarma justo en los
           modelos estacionales, que son los que mas importan aqui.            */
        df = nlags - (n_ar_free_params(&Tm[i]) + n_ma_free_params(&Tm[i]));
        if (df < 1) df = 1;
        p_noise = 1.0 - chisq(Q, df);

        /* --- la relacion: el peor p-valor de adecuacion de sus enlaces --- */
        p_rel = 1.0;
        for (j = 1; j <= n_link; j++)
            if (lnk[j].out == i && lnk[j].s >= 0 && link_p_transfer[j] >= 0.0
                && link_p_transfer[j] < p_rel)
                p_rel = link_p_transfer[j];

        fprintf(out, "  %s:\n", Ts[i].name ? Ts[i].name : "");
        fprintf(out, "    relation (CCF, noise vs input)   p = %.4f\n", p_rel);
        fprintf(out, "    noise    (ACF of the residuals)  p = %.4f   [Q(%d) = %.2f]\n",
                p_noise, df, Q);

        if (p_rel < 0.05) {
            fprintf(out, "    -> REFORMULATE THE RELATION.\n");
            if (p_noise < 0.05)
                fprintf(out, "       The noise looks bad too, but do NOT touch it yet:\n"
                             "       that may be the relation's leftovers showing up in\n"
                             "       the ACF. Re-estimate and look again.\n");
        } else if (p_noise < 0.05) {
            fprintf(out, "    -> The relation is adequate. NOW reformulate the NOISE\n"
                         "       (its ARMA structure), not the transfer.\n");
        } else {
            fprintf(out, "    -> Nothing to reformulate: relation and noise both pass.\n");
        }
        fprintf(out, "\n");
    }

    free_vector(corr, 1, nlags);
    free_vector(res, 1, n);
    fprintf(out, "=============================================================\n");
}


/* V(l)_{i1,i2} = SUM_{t<l} [ LP(t) Sigma LP(t)' ]_{i1,i2}, con Sigma GENERAL. */
static real vcov_at(real ***LP, real **sigma, int m, int i1, int i2, int l)
{
    int t, j, k;
    real v = 0.0;
    for (t = 0; t <= l - 1; t++)
        for (j = 1; j <= m; j++)
            for (k = 1; k <= m; k++)
                v += sigma[j][k] * LP[i1][j][t] * LP[i2][k][t];
    return v;
}


/* --------------------------------------------------------------------------
   PREVISION RECURSIVA (fuera de muestra, parametros FIJOS).

   Las varianzas de prevision que da el modelo son TEORICAS: dicen lo que el
   modelo implica, no lo que pasa fuera de muestra, donde entran la
   incertidumbre de los parametros y los cambios de estructura. La unica forma
   de decidir empiricamente si un modelo prevé mejor que otro es esta: estimar
   UNA vez sobre una ventana, congelar los parametros, y hacer rodar el origen
   un dato cada vez, previendo H pasos y comparando con lo que de verdad paso.

   Es lo que hace drvarma (-estwin) y lo que el ejercicio de pass-through pedia.
   -------------------------------------------------------------------------- */

/* Previsiones de NIVEL, sin bandas, de todas las series. Devuelve 0 si va bien. */

static void add_grad(real *g, int slot, real v);
static real delta_se(real *g, real **cov, int npar);

/* --------------------------------------------------------------------------
   LA FUNCION DE RESPUESTA AL IMPULSO, cuantificada.

   Los pesos nu_k YA son la respuesta al impulso: es lo que hace comodo a un
   modelo de transferencia. Pero sueltos no son comparables con nada. Lo que se
   necesita es (a) su error estandar y (b) la respuesta ACUMULADA, que es la que
   converge a la ganancia y la que un economista lee.

   nu_t = omega_{t-1-b} + SUM_j delta_j nu_{t-j}

   El gradiente sale de la MISMA recursion:
     d nu_t / d omega_i = [t-1-b == i] + SUM_j delta_j * d nu_{t-j} / d omega_i
     d nu_t / d delta_i = nu_{t-i}     + SUM_j delta_j * d nu_{t-j} / d delta_i
   y de ahi el error estandar por el metodo delta. La acumulada es la suma de
   los gradientes.

   NOTA sobre los VAR. En un VAR la respuesta al impulso NO esta identificada sin
   una ordenacion (Cholesky), porque Sigma no es diagonal: hay que decidir quien
   choca primero, y la respuesta CAMBIA con esa decision. Aqui la identificacion
   ES el modelo: nu(B) se estima directamente y Q es diagonal. Pero eso no es
   magia -- es que las restricciones (input exogeno, Sigma diagonal) estan
   DECLARADAS, se CONTRASTAN (exogeneidad, adecuacion) y se pueden RELAJAR. Un
   VAR con orden de Cholesky impone restricciones del mismo tipo, solo que
   escondidas en el orden. Ver el aviso de casi-colinealidad: si se libera la
   covarianza junto a una transferencia contemporanea, drtran vuelve a caer en el
   MISMO problema, y lo dice.
   -------------------------------------------------------------------------- */
static void impulse_response_report(real *x, real **cov, int npar, FILE *out)
{
    real *xf = expand_params(x);
    int   j, k, i, slot0 = 0, slot;
    int   K;
    real *nu, *cum, **gnu, *gcum, *gg;

    for (j = 1; j <= n_link; j++) if (lnk[j].s >= 0) break;
    if (j > n_link) return;

    fprintf(out, "\n");
    fprintf(out, "=============================================================\n");
    fprintf(out, "  IMPULSE RESPONSE  nu(B) = omega(B)/delta(B) * B^b\n");
    fprintf(out, "=============================================================\n");
    fprintf(out, "  nu_k is the response of the output, k periods later, to a ONE-OFF\n");
    fprintf(out, "  unit shock in the input. The CUMULATIVE column is the response to a\n");
    fprintf(out, "  PERMANENT unit change; it converges to the gain.\n\n");
    fprintf(out, "  In a VAR the impulse response is not identified without an ordering\n");
    fprintf(out, "  (Cholesky), because Sigma is not diagonal. Here the identification IS\n");
    fprintf(out, "  the model. That is not magic: the restrictions (exogenous input,\n");
    fprintf(out, "  diagonal Sigma) are DECLARED, they are TESTED, and they can be\n");
    fprintf(out, "  relaxed -- and if you relax them next to a contemporaneous transfer,\n");
    fprintf(out, "  you land back in the VAR's problem, and the program says so.\n");

    slot0 = 0;
    for (j = 1; j <= n_link; j++) {
        int b = lnk[j].b, r = lnk[j].r, sn = lnk[j].s;

        if (sn < 0) continue;
        slot = slot0 + 1;
        slot0 += (sn + 1) + r;

        K = b + sn + 12;                       /* suficiente para ver la cola */
        if (K > 36) K = 36;

        nu   = vector(0, K);
        cum  = vector(0, K);
        gnu  = matrix(0, K, 1, npar);
        gcum = vector(1, npar);
        gg   = vector(1, npar);

        for (k = 0; k <= K; k++) {
            real acc = 0.0;
            int  lag = k - b;
            if (lag >= 0 && lag <= sn) acc = xf[slot + lag];
            for (i = 1; i <= r; i++)
                if (k >= i) acc += xf[slot + sn + i] * nu[k - i];
            nu[k] = acc;
            cum[k] = (k == 0) ? nu[0] : cum[k - 1] + nu[k];
        }

        /* gradientes, por la misma recursion */
        for (k = 0; k <= K; k++)
            for (i = 1; i <= npar; i++) gnu[k][i] = 0.0;
        for (i = 1; i <= npar; i++) gcum[i] = 0.0;

        fprintf(out, "\n  ");
        if (net_is_star()) fprintf(out, "Input %d", j);
        else fprintf(out, "%s <- %s", Ts[lnk[j].out].name, Ts[lnk[j].inp].name);
        fprintf(out, "   (b=%d, r=%d, s=%d)\n\n", b, r, sn);
        fprintf(out, "    k      nu_k     std.err       t   |   cumulative   std.err\n");
        fprintf(out, "  ---------------------------------------------------------------\n");

        for (k = 0; k <= K; k++) {
            real se, sec;
            int  lag = k - b;

            for (i = 1; i <= npar; i++) gg[i] = 0.0;

            /* d nu_k / d omega_lag */
            if (lag >= 0 && lag <= sn) add_grad(gg, slot + lag, 1.0);
            /* d nu_k / d delta_i  y la parte recursiva */
            for (i = 1; i <= r; i++) {
                int p2;
                if (k >= i) add_grad(gg, slot + sn + i, nu[k - i]);
                for (p2 = 1; p2 <= npar; p2++)
                    if (k >= i) gg[p2] += xf[slot + sn + i] * gnu[k - i][p2];
            }
            for (i = 1; i <= npar; i++) {
                gnu[k][i] = gg[i];
                gcum[i]  += gg[i];
            }

            se  = delta_se(gg, cov, npar);
            sec = delta_se(gcum, cov, npar);

            fprintf(out, "  %3d  %9.6f  %9.6f  %6.2f  |  %9.6f  %9.6f\n",
                    k, nu[k], se, (se > 1e-15) ? nu[k] / se : 0.0, cum[k], sec);
        }

        free_vector(gg, 1, npar);
        free_vector(gcum, 1, npar);
        free_matrix(gnu, 0, K, 1, npar);
        free_vector(cum, 0, K);
        free_vector(nu, 0, K);
    }
    fprintf(out, "=============================================================\n");
}

static int forecast_levels(real *x, int L, real **LVL)
{
    struct Tvarma vf;
    int  ifault = 0, m = n_ser, i, j, k, l, t, u;
    int  K = n_stat + L + 1;
    real **sigma, **f1, ***v1, ***v2, ***v3, **nu, **we, *det, *bc;
    int  rc = 0;

    shootx(x, &vf, &ifault, 1, 0);
    if (ifault != 0) return 1;
    x = expand_params(x);

    {   /* los residuos: sin ellos, la parte MA no preve nada */
        real pi1, pi2, pi3;
        int  ifa = 0;
        vf.xitol = -1e-3;
        elf(vf.m, vf.n, vf.p, vf.q, vf.mu, vf.phi, vf.theta, vf.qq, vf.w,
            1.0, vf.xitol, FALSE, vf.a, &pi1, &pi2, &pi3, &ifa);
        if (ifa != 0) { shootx(x, &vf, &ifault, 0, 1); return 2; }
    }

    sigma = matrix(1, m, 1, m);
    for (i = 1; i <= m; i++)
        for (j = 1; j <= m; j++) sigma[i][j] = vf.qq[i][j];

    f1 = matrix(1, m, 1, L);
    v1 = tensor(1, L, 1, m, 1, m);
    v2 = tensor(1, L, 1, m, 1, m);
    v3 = tensor(1, L, 1, m, 1, m);
    forecast_model(m, n_stat, vf.p, vf.q, vf.mu, vf.phi, vf.theta, sigma,
                   vf.w, vf.a, f1, v1, v2, v3, 0, L, Ts[1].freq, NULL);

    nu = matrix(1, (n_link > 0) ? n_link : 1, 1, K);
    for (k = 1; k <= ((n_link > 0) ? n_link : 1); k++)
        for (t = 1; t <= K; t++) nu[k][t] = 0.0;
    {
        real omega[MAX_S + 1], delta[MAX_R + 1];
        int idx = 1;
        for (k = 1; k <= n_link; k++) {
            for (j = 0; j <= lnk[k].s; j++) omega[j] = x[idx++];
            for (j = 1; j <= lnk[k].r; j++) delta[j] = x[idx++];
            compute_irf(omega, lnk[k].s, delta, lnk[k].r, lnk[k].b, nu[k], K);
        }
    }

    /* las series extendidas, en orden topologico */
    we = matrix(1, m, 1, n_stat + L);
    for (u = 1; u <= m; u++) {
        i = topo[u];
        for (t = 1; t <= n_stat; t++) we[i][t] = w[i][t];
        for (l = 1; l <= L; l++) {
            real acc = f1[i][l];
            int  tt  = n_stat + l;
            for (k = 1; k <= n_link; k++) {
                if (lnk[k].out != i) continue;
                for (j = 1; j <= tt && j <= K; j++)
                    acc += nu[k][j] * we[lnk[k].inp][tt - j + 1];
            }
            we[i][tt] = acc;
        }
    }

    /* integrar al nivel */
    for (i = 1; i <= m; i++) {
        int nb = Ts[i].nobs, ord = Tm[i].ornsop;
        det = vector(1, nb + L);
        bc  = vector(1, nb + L);
        build_det_component(&Tm[i], &Ts[i], nb + L, det);

        for (t = 1; t <= nb; t++) {
            real y  = Ts[i].data[t];
            real b0 = (fabs(Tm[i].boxlam) < 1e-8)
                    ? log(y) * Ts[i].refactor
                    : ((pow(y, Tm[i].boxlam) - 1.0) / Tm[i].boxlam) * Ts[i].refactor;
            bc[t] = b0 - det[t];
        }
        for (l = 1; l <= L; l++) {
            real acc = we[i][n_stat + l];
            int  tt  = nb + l;
            for (k = 1; k <= ord; k++) acc -= (-Tm[i].rnsop[k]) * bc[tt - k];
            bc[tt] = acc;

            {
                real c = bc[tt] + det[tt], lam = Tm[i].boxlam;
                LVL[i][l] = (fabs(lam) < 1e-8)
                          ? exp(c / Ts[i].refactor)
                          : pow(lam * (c / Ts[i].refactor) + 1.0, 1.0 / lam);
            }
        }
        free_vector(bc, 1, nb + L);
        free_vector(det, 1, nb + L);
    }

    free_matrix(we, 1, m, 1, n_stat + L);
    free_matrix(nu, 1, (n_link > 0) ? n_link : 1, 1, K);
    free_tensor(v3, 1, L, 1, m, 1, m);
    free_tensor(v2, 1, L, 1, m, 1, m);
    free_tensor(v1, 1, L, 1, m, 1, m);
    free_matrix(f1, 1, m, 1, L);
    free_matrix(sigma, 1, m, 1, m);
    shootx(x, &vf, &ifault, 0, 1);
    return rc;
}

static void recursive_eval(real *x, int L, FILE *out)
{
    int   i, l, e, no;
    int   nfull = nobs_full[1];
    int   e0    = rec_start;
    int   elast = nfull - L;
    real **LVL  = matrix(1, n_ser, 1, L);
    real  *sae  = vector(1, L);      /* suma de |error|        */
    real  *sse  = vector(1, L);      /* suma de error^2        */
    real  *sape = vector(1, L);      /* suma de |error|/actual */
    int   *cnt  = ivector(1, L);
    FILE  *csv  = NULL;

    if (elast < e0) {
        fprintf(out, "\nRecursive evaluation: not enough data "
                     "(origin %d, horizon %d, %d observations).\n", e0, L, nfull);
        goto done;
    }

    for (l = 1; l <= L; l++) { sae[l] = sse[l] = sape[l] = 0.0; cnt[l] = 0; }

    if (rec_csv[0] != '\0') {
        csv = fopen(rec_csv, "w");
        if (csv) fprintf(csv, "origin,horizon,actual,forecast,error\n");
    }

    no = 0;
    for (e = e0; e <= elast; e++) {
        for (i = 1; i <= n_ser; i++) Ts[i].nobs = e;
        build_stationary_series();
        if (n_stat <= 0) continue;

        if (forecast_levels(x, L, LVL) != 0) continue;
        no++;

        for (l = 1; l <= L; l++) {
            real act = Ts[1].data[e + l];      /* el dato NO truncado */
            real err = LVL[1][l] - act;
            sae[l]  += fabs(err);
            sse[l]  += err * err;
            sape[l] += (fabs(act) > 1e-12) ? fabs(err / act) : 0.0;
            cnt[l]++;
            if (csv)
                fprintf(csv, "%d,%d,%.6f,%.6f,%.6f\n", e, l, act, LVL[1][l], err);
        }
    }

    if (csv) fclose(csv);

    /* Devolver la muestra a la VENTANA DE ESTIMACION, no al tamano completo: las
       estructuras que el llamante todavia tiene vivas (la Tvarma de diagnosis)
       estan dimensionadas para esa ventana, y escribir con un n_stat mayor las
       desborda. Costo una segfault. */
    for (i = 1; i <= n_ser; i++) Ts[i].nobs = rec_start;
    build_stationary_series();

    fprintf(out, "\n");
    fprintf(out, "=============================================================\n");
    fprintf(out, "  RECURSIVE FORECAST EVALUATION (out of sample)\n");
    fprintf(out, "=============================================================\n");
    fprintf(out, "  The variances the model reports are THEORETICAL: they say what\n");
    fprintf(out, "  the model implies, not what happens out of sample, where\n");
    fprintf(out, "  parameter uncertainty and structural change have their say.\n");
    fprintf(out, "  Here the parameters are estimated ONCE on the first %d\n", e0);
    fprintf(out, "  observations, then held FIXED while the origin rolls forward\n");
    fprintf(out, "  one datum at a time.\n\n");
    fprintf(out, "  Output   : %s\n", Ts[1].name ? Ts[1].name : "");
    fprintf(out, "  Origins  : %d  (from obs %d to %d)\n", no, e0, elast);
    fprintf(out, "  Horizon  : %d\n\n", L);
    fprintf(out, "    h      n        MAE         RMSE        MAPE(%%)\n");
    fprintf(out, "  ---------------------------------------------------\n");
    for (l = 1; l <= L; l++) {
        if (cnt[l] == 0) continue;
        fprintf(out, "  %3d  %5d  %11.6f  %11.6f  %10.4f\n",
                l, cnt[l], sae[l] / cnt[l], sqrt(sse[l] / cnt[l]),
                100.0 * sape[l] / cnt[l]);
    }
    fprintf(out, "=============================================================\n");
    if (rec_csv[0] != '\0')
        fprintf(out, "  Per-origin errors written to %s\n", rec_csv);

done:
    free_ivector(cnt, 1, L);
    free_vector(sape, 1, L);
    free_vector(sse, 1, L);
    free_vector(sae, 1, L);
    free_matrix(LVL, 1, n_ser, 1, L);
}

static void transfer_forecast(real *x, int npar, int L, real sigma2, FILE *out)
{
    struct Tvarma vf;
    int    ifault = 0;
    int    m = n_ser, i, j, k, l, t, u, p, q;
    int    K  = n_stat + L + 1;
    int    NK = (n_link > 0) ? n_link : 1;

    real **sigma, **f1, ***v1, ***v2, ***v3, ***psi;
    real **nu, **we, ***pt;
    int    nobsmax = 0;

    shootx(x, &vf, &ifault, 1, 0);
    if (ifault != 0) {
        fprintf(out, "\nCould not build the model for forecasting.\n");
        return;
    }
    x = expand_params(x);        /* los omegas se leen de la estructura completa */
    p = vf.p; q = vf.q;

    /* Los RESIDUOS pasados. forecast_model los necesita para la parte MA: la
       prevision de un MA(q) es una combinacion de las ultimas q innovaciones. Y
       shootx solo ALOJA a[] -- a ceros --, no lo calcula: quien lo calcula es elf.
       Sin esta llamada, todo modelo con q > 0 se preveia con residuos NULOS. No
       se notaba porque un AR puro no entra en ese bucle... y ningun modelo con MA
       se preveia en las pruebas. */
    {
        real pi1, pi2, pi3;
        int  ifa = 0;
        vf.xitol = -1e-3;                       /* verosimilitud EXACTA */
        elf(vf.m, vf.n, vf.p, vf.q, vf.mu, vf.phi, vf.theta, vf.qq, vf.w,
            1.0, vf.xitol, FALSE, vf.a, &pi1, &pi2, &pi3, &ifa);
        if (ifa != 0) {
            fprintf(out, "\nCould not compute the residuals for forecasting "
                         "(elf ifault = %d).\n", ifa);
            shootx(x, &vf, &ifault, 0, 1);
            return;
        }
    }

    sigma = matrix(1, m, 1, m);
    for (i = 1; i <= m; i++)
        for (j = 1; j <= m; j++) sigma[i][j] = sigma2 * vf.qq[i][j];
    for (i = 1; i <= m; i++)
        if (Ts[i].nobs > nobsmax) nobsmax = Ts[i].nobs;

    /* El VARMA preve sus m series, que son los RUIDOS: cada w_i menos lo que
       recibe por la red.  Reconstruir las series OBSERVADAS exige recorrer la
       red en orden topologico, sumando las transferencias.                    */
    f1 = matrix(1, m, 1, L);
    v1 = tensor(1, L, 1, m, 1, m);
    v2 = tensor(1, L, 1, m, 1, m);
    v3 = tensor(1, L, 1, m, 1, m);
    forecast_model(m, n_stat, p, q, vf.mu, vf.phi, vf.theta, sigma,
                   vf.w, vf.a, f1, v1, v2, v3, 0, L, Ts[1].freq, NULL);

    /* Pesos nu de cada ENLACE */
    nu = matrix(1, NK, 1, K);
    for (k = 1; k <= NK; k++)
        for (t = 1; t <= K; t++) nu[k][t] = 0.0;
    {
        real omega[MAX_S + 1], delta[MAX_R + 1];
        int idx = 1;
        for (k = 1; k <= n_link; k++) {
            for (j = 0; j <= lnk[k].s; j++) omega[j] = x[idx++];
            for (j = 1; j <= lnk[k].r; j++) delta[j] = x[idx++];
            compute_irf(omega, lnk[k].s, delta, lnk[k].r, lnk[k].b, nu[k], K);
        }
    }

    /* --- Series estacionarias EXTENDIDAS, en orden topologico --------------
       Pasado: lo observado.  Futuro: el ruido previsto MAS la transferencia,
       que a su vez necesita el futuro de las entradas -> por eso el orden.   */
    we = matrix(1, m, 1, n_stat + L);
    for (u = 1; u <= m; u++) {
        i = topo[u];
        for (t = 1; t <= n_stat; t++) we[i][t] = w[i][t];
        for (l = 1; l <= L; l++) {
            real acc = f1[i][l];              /* f1 ya lleva mu_i */
            int  tt  = n_stat + l;
            for (k = 1; k <= n_link; k++) {
                if (lnk[k].out != i) continue;
                for (j = 1; j <= tt && j <= K; j++)
                    acc += nu[k][j] * we[lnk[k].inp][tt - j + 1];
            }
            we[i][tt] = acc;
        }
    }

    /* --- Pesos psi TOTALES del sistema -------------------------------------
       La serie i responde a la innovacion j por dos caminos: su propio ruido
       (si i == j) y todo lo que le llega por la red:

           Psi_ij(B) = d_ij * psi_i(B) + SUM_{k: out=i} nu_k(B) * Psi_{inp(k),j}(B)

       Es la misma recursion topologica.  El error de prevision de una serie
       hereda, propagadas por su nu(B), las innovaciones de TODO lo que tiene
       aguas arriba: eso es lo que hace que prever Y exija prever las X.      */
    psi = tensor(0, L, 1, m, 1, m);
    compute_psi_weights(m, p, q, vf.phi, vf.theta, L, psi);

    pt = tensor(1, m, 1, m, 0, L);
    for (u = 1; u <= m; u++) {
        i = topo[u];
        for (j = 1; j <= m; j++)
            for (t = 0; t <= L; t++) {
                real acc = (i == j) ? psi[t][i][i] : 0.0;   /* VARMA diagonal */
                for (k = 1; k <= n_link; k++) {
                    int v;
                    if (lnk[k].out != i) continue;
                    for (v = 0; v <= t; v++)
                        if (v + 1 <= K)
                            acc += nu[k][v + 1] * pt[lnk[k].inp][j][t - v];
                }
                pt[i][j][t] = acc;
            }
    }

    fprintf(out, "\n");
    fprintf(out, "=============================================================\n");
    if (net_is_star())
        fprintf(out, "  FORECAST OF Y GIVEN THE MODELS OF THE %d INPUT(S)\n", n_inp);
    else
        fprintf(out, "  FORECAST OF THE TRANSFER NETWORK (%d LINKS)\n", n_link);
    fprintf(out, "=============================================================\n");
    fprintf(out, "  Forecasting an output requires forecasting its inputs: the\n");
    fprintf(out, "  transfer needs their future. The forecast error therefore adds\n");
    fprintf(out, "  the series' own innovation and EVERY innovation upstream of it\n");
    fprintf(out, "  in the network, each propagated through the nu(B) it crosses.\n");

    /* --- Niveles y pesos psi del NIVEL, para TODAS las series --------------
       Se necesitan de todas, no solo de las salidas, porque un AGREGADO es una
       combinacion lineal de cualquier subconjunto de ellas.                   */
    {
    real ***LP  = tensor(1, m, 1, m, 0, L);   /* LP[i][j][t]: psi del nivel de i */
    real  **BC  = matrix(1, m, 1, nobsmax + L);
    real  **DET = matrix(1, m, 1, nobsmax + L);
    real  **LVL = matrix(1, m, 1, L);         /* nivel previsto */
    real  **JAC = matrix(1, m, 1, L);         /* dz/db en el punto previsto */
    real   *uu_i = vector(0, L);

    for (i = 1; i <= m; i++) {
        int ordi = Tm[i].ornsop;
        int nb   = Ts[i].nobs;

        build_det_component(&Tm[i], &Ts[i], nb + L, DET[i]);

        for (t = 1; t <= nb; t++) {
            real y  = Ts[i].data[t];
            real b0 = (fabs(Tm[i].boxlam) < 1e-8)
                    ? log(y) * Ts[i].refactor
                    : ((pow(y, Tm[i].boxlam) - 1.0) / Tm[i].boxlam) * Ts[i].refactor;
            BC[i][t] = b0 - DET[i][t];
        }
        for (l = 1; l <= L; l++) {
            real acc = we[i][n_stat + l];
            int  tt  = nb + l;
            for (k = 1; k <= ordi; k++) acc -= (-Tm[i].rnsop[k]) * BC[i][tt - k];
            BC[i][tt] = acc;
        }

        /* 1/rnsop(B): el operador que integra al nivel */
        uu_i[0] = 1.0;
        for (t = 1; t <= L; t++) {
            real sum = 0.0;
            for (k = 1; k <= ordi && k <= t; k++)
                sum += (-Tm[i].rnsop[k]) * uu_i[t - k];
            uu_i[t] = -sum;
        }
        for (j = 1; j <= m; j++)
            for (t = 0; t <= L; t++) {
                real acc = 0.0;
                for (k = 0; k <= t; k++) acc += uu_i[k] * pt[i][j][t - k];
                LP[i][j][t] = acc;
            }

        /* nivel y jacobiano dz/db (para la delta de los agregados) */
        for (l = 1; l <= L; l++) {
            real center = BC[i][Ts[i].nobs + l] + DET[i][Ts[i].nobs + l];
            real lam    = Tm[i].boxlam;
            if (fabs(lam) < 1e-8) {
                LVL[i][l] = exp(center / Ts[i].refactor);
                JAC[i][l] = LVL[i][l] / Ts[i].refactor;
            } else {
                real base = lam * (center / Ts[i].refactor) + 1.0;
                LVL[i][l] = pow(base, 1.0 / lam);
                JAC[i][l] = pow(base, 1.0 / lam - 1.0) / Ts[i].refactor;
            }
        }
    }

    /* --- Covarianza del error de prevision, en el espacio TRANSFORMADO -----
           V(l)_{i1,i2} = SUM_{t<l} [ LP(t) Sigma LP(t)' ]_{i1,i2}
       Sigma GENERAL: desde que se pueden liberar covarianzas, suponerla
       diagonal aqui seria un error -- y lo era.                              */
    #define VCOV(I1, I2, LL)  vcov_at(LP, sigma, m, (I1), (I2), (LL))

    /* --- Una tabla por serie que RECIBE alguna transferencia --------------- */
    for (u = 1; u <= m; u++) {
        int is_out = 0;
        i = topo[u];
        for (k = 1; k <= n_link; k++) if (lnk[k].out == i) is_out = 1;
        if (!is_out) continue;

        fprintf(out, "\n  Output: %s\n", Ts[i].name ? Ts[i].name : "");
        fprintf(out, "  Stationary series (w) and LEVEL, with 95%% bands:\n\n");
        fprintf(out, "   l     w_Y fcst    sd(w)   |     LEVEL         lower         upper\n");
        fprintf(out, "  ---------------------------------------------------------------------\n");

        for (l = 1; l <= L; l++) {
            real vw = 0.0, vl, sd, lo, hi, center, lvl, lam;
            int  j2;

            for (t = 0; t <= l - 1; t++)
                for (j = 1; j <= m; j++)
                    for (j2 = 1; j2 <= m; j2++)
                        vw += sigma[j][j2] * pt[i][j][t] * pt[i][j2][t];

            vl = VCOV(i, i, l);
            sd = sqrt(vl);

            center = BC[i][Ts[i].nobs + l] + DET[i][Ts[i].nobs + l];
            lo = center - 1.96 * sd;
            hi = center + 1.96 * sd;
            lam = Tm[i].boxlam;
            lvl = LVL[i][l];

            if (fabs(lam) < 1e-8) {
                lo = exp(lo / Ts[i].refactor);
                hi = exp(hi / Ts[i].refactor);
            } else {
                lo = pow(lam * (lo / Ts[i].refactor) + 1.0, 1.0 / lam);
                hi = pow(lam * (hi / Ts[i].refactor) + 1.0, 1.0 / lam);
            }
            fprintf(out, "  %3d  %10.4f  %8.4f  |  %10.4f  %10.4f  %10.4f\n",
                    l, we[i][n_stat + l], sqrt(vw), lvl, lo, hi);
        }
    }

    /* --- DESCOMPOSICION DE LA VARIANZA DEL ERROR DE PREVISION --------------
       Cuanto del error de prever la salida i a l pasos viene de CADA fuente de
       innovacion. Con Sigma DIAGONAL la respuesta es limpia y unica:

           share_ij(l) = sigma_jj * SUM_{t<l} LP[i][j][t]^2  /  Var_i(l)

       porque las innovaciones son ortogonales y no hay nada que ordenar.

       Si Sigma NO es diagonal, la descomposicion NO ES UNICA: hay que decidir a
       quien se le atribuye la parte comun, y eso exige una ORDENACION (Cholesky).
       Es EXACTAMENTE el problema del VAR. drtran no lo resuelve por arte de
       magia: lo evita mientras Sigma sea diagonal, y cuando no lo es, lo dice en
       vez de fabricar una descomposicion que depende de un orden arbitrario.     */
    {
        int i2, diagQ = 1;
        for (i = 1; i <= m && diagQ; i++)
            for (j = 1; j <= m; j++)
                if (i != j && fabs(sigma[i][j]) > 1e-14) { diagQ = 0; break; }

        fprintf(out, "\n");
        fprintf(out, "  FORECAST ERROR VARIANCE DECOMPOSITION\n");
        fprintf(out, "  How much of the error of forecasting the output comes from EACH\n");
        fprintf(out, "  source of innovation.\n");

        if (!diagQ) {
            fprintf(out, "\n  NOT REPORTED: Sigma is not diagonal. With correlated\n");
            fprintf(out, "  innovations the decomposition is NOT UNIQUE -- someone has to be\n");
            fprintf(out, "  given the common part, and that requires an ORDERING (Cholesky).\n");
            fprintf(out, "  That is exactly the VAR's problem. It is not solved here; it is\n");
            fprintf(out, "  avoided while Sigma stays diagonal, and declared when it does not.\n\n");
        } else {
            for (u = 1; u <= m; u++) {
                int is_out = 0;
                i = topo[u];
                for (k = 1; k <= n_link; k++) if (lnk[k].out == i) is_out = 1;
                if (!is_out) continue;

                fprintf(out, "\n  %s  (%% of the forecast error variance of the LEVEL)\n\n",
                        Ts[i].name ? Ts[i].name : "");
                fprintf(out, "    l  ");
                for (j = 1; j <= m; j++)
                    fprintf(out, "%12.12s", (j == i) ? "own noise"
                                          : (Ts[j].name ? Ts[j].name : "?"));
                fprintf(out, "\n  ------");
                for (j = 1; j <= m; j++) fprintf(out, "------------");
                fprintf(out, "\n");

                for (l = 1; l <= L; l++) {
                    real tot = VCOV(i, i, l);
                    if (tot <= 0.0) continue;
                    fprintf(out, "  %3d  ", l);
                    for (j = 1; j <= m; j++) {
                        real c = 0.0;
                        for (t = 0; t <= l - 1; t++)
                            c += sigma[j][j] * LP[i][j][t] * LP[i][j][t];
                        fprintf(out, "%11.1f%%", 100.0 * c / tot);
                    }
                    fprintf(out, "\n");
                }
            }
            fprintf(out, "\n");
        }
        (void)i2;
    }

    /* --- AGREGADOS: combinaciones lineales, con varianza c'Vc --------------
       La identidad contable (OCUPADOS = suma de sectores; PARADOS = ACTIVOS -
       OCUPADOS) NO se mete en el modelo: se calcula DESPUES de prever, como
       hacia el legacy. Lo que no es trivial es su BANDA: los errores de
       prevision de las series estan correlacionados -- comparten innovaciones a
       traves de la red -- asi que la varianza del agregado NO es la suma de las
       varianzas. Es c'Vc, con V la matriz completa del error de prevision.

       Ojo: las series se modelan transformadas (log, Box-Cox), y la identidad
       vive en NIVELES. Por eso el agregado se forma con los niveles y la
       varianza se propaga por el metodo delta, con J_i = dz_i/db_i.           */
    for (k = 1; k <= n_aggr; k++) {
        fprintf(out, "\n  Aggregate: %s  =", aggr_name[k]);
        for (i = 1; i <= m; i++) {
            if (aggr_c[k][i] == 0.0) continue;
            fprintf(out, " %c %s", (aggr_c[k][i] > 0) ? '+' : '-',
                    Ts[i].name ? Ts[i].name : "?");
        }
        fprintf(out, "\n");
        fprintf(out, "  Computed AFTER forecasting; its band is c'Vc, so it accounts for\n");
        fprintf(out, "  the correlation between the series' forecast errors.\n\n");
        fprintf(out, "   l        LEVEL       sd         lower         upper\n");
        fprintf(out, "  ----------------------------------------------------------\n");

        for (l = 1; l <= L; l++) {
            real pnt = 0.0, var = 0.0, sd;
            int  i2;

            for (i = 1; i <= m; i++) pnt += aggr_c[k][i] * LVL[i][l];

            for (i = 1; i <= m; i++) {
                if (aggr_c[k][i] == 0.0) continue;
                for (i2 = 1; i2 <= m; i2++) {
                    if (aggr_c[k][i2] == 0.0) continue;
                    var += aggr_c[k][i] * aggr_c[k][i2]
                         * JAC[i][l] * JAC[i2][l] * VCOV(i, i2, l);
                }
            }
            sd = (var > 0.0) ? sqrt(var) : 0.0;
            fprintf(out, "  %3d  %11.4f  %8.4f  %11.4f  %11.4f\n",
                    l, pnt, sd, pnt - 1.96 * sd, pnt + 1.96 * sd);
        }
    }
    #undef VCOV

    free_vector(uu_i, 0, L);
    free_matrix(JAC, 1, m, 1, L);
    free_matrix(LVL, 1, m, 1, L);
    free_matrix(DET, 1, m, 1, nobsmax + L);
    free_matrix(BC, 1, m, 1, nobsmax + L);
    free_tensor(LP, 1, m, 1, m, 0, L);
    }

    fprintf(out, "=============================================================\n\n");

    free_tensor(pt, 1, m, 1, m, 0, L);
    free_tensor(psi, 0, L, 1, m, 1, m);
    free_matrix(we, 1, m, 1, n_stat + L);
    free_matrix(nu, 1, NK, 1, K);
    free_tensor(v3, 1, L, 1, m, 1, m);
    free_tensor(v2, 1, L, 1, m, 1, m);
    free_tensor(v1, 1, L, 1, m, 1, m);
    free_matrix(f1, 1, m, 1, L);
    free_matrix(sigma, 1, m, 1, m);
    shootx(x, &vf, &ifault, 0, 1);
    (void)npar;
}

/* -------------------------------------------------------------------------- */
/* LA RED de transferencias                                                    */
/*                                                                            */
/* Por defecto la red es una ESTRELLA: todas las entradas apuntan a la serie 1.*/
/* Con -n <fichero> se declara un DAG cualquiera, que es lo que de verdad son  */
/* los sistemas de Mauricio: en m6, EU es SALIDA de EC y a la vez ENTRADA de   */
/* EI.  Cada linea del fichero es un ENLACE:                                   */
/*                                                                            */
/*     SALIDA <- ENTRADA   b r s                                              */
/*                                                                            */
/* donde SALIDA y ENTRADA son el nombre de la serie (el del .pre) o su posicion*/
/* en la linea de ordenes (1 = el primer fichero).                            */
/* -------------------------------------------------------------------------- */
static void build_default_links(void)
{
    int j;
    n_link = 0;
    for (j = 1; j <= n_inp; j++) {
        n_link++;
        lnk[n_link].out = 1;
        lnk[n_link].inp = j + 1;
        lnk[n_link].b   = b_del[j];
        lnk[n_link].r   = r_ord[j];
        lnk[n_link].s   = s_ord[j];
    }
}

static int series_index(const char *tok)
{
    int i;
    char *end;
    long v = strtol(tok, &end, 10);

    if (*end == '\0' && v >= 1 && v <= n_ser) return (int)v;

    for (i = 1; i <= n_ser; i++)
        if (Ts[i].name && strcasecmp(Ts[i].name, tok) == 0) return i;
    return 0;
}

static int read_network(const char *path)
{
    FILE *f = fopen(path, "r");
    char line[256];
    int  nl = 0;

    if (f == NULL) {
        fprintf(stderr, "Error: cannot open the network file %s\n", path);
        return -1;
    }

    n_link = 0;
    while (fgets(line, sizeof line, f)) {
        char lhs[64], arrow[8], rhs[64];
        int  b, r, sO, io, ii;
        char *h = strchr(line, '#');
        if (h) *h = '\0';
        if (sscanf(line, "%63s %7s %63s %d %d %d",
                   lhs, arrow, rhs, &b, &r, &sO) != 6) {
            int only_ws = 1; char *c;
            for (c = line; *c; c++) if (!isspace((unsigned char)*c)) only_ws = 0;
            if (only_ws) continue;
            fprintf(stderr, "Error: bad line in %s: %s", path, line);
            fclose(f); return -1;
        }
        if (strcmp(arrow, "<-") != 0) {
            fprintf(stderr, "Error: expected '<-' in %s, found '%s'\n", path, arrow);
            fclose(f); return -1;
        }
        io = series_index(lhs);
        ii = series_index(rhs);
        if (io == 0 || ii == 0) {
            fprintf(stderr, "Error: unknown series in %s: '%s <- %s'\n",
                    path, lhs, rhs);
            fclose(f); return -1;
        }
        if (io == ii) {
            fprintf(stderr, "Error: a series cannot feed itself (%s)\n", lhs);
            fclose(f); return -1;
        }
        if (n_link >= MAX_LINK) {
            fprintf(stderr, "Error: too many links (max %d)\n", MAX_LINK);
            fclose(f); return -1;
        }
        n_link++;
        lnk[n_link].out = io;
        lnk[n_link].inp = ii;
        lnk[n_link].b   = b;
        lnk[n_link].r   = r;
        lnk[n_link].s   = sO;
        nl++;
    }
    fclose(f);
    return nl;
}

/* Fichero de agregados:  NOMBRE = + SERIE - SERIE ...
   Las series por su nombre (el del .pre) o por su posicion.                  */
static int read_aggregates(const char *path)
{
    FILE *f = fopen(path, "r");
    char line[512];

    if (f == NULL) {
        fprintf(stderr, "Error: cannot open the aggregates file %s\n", path);
        return -1;
    }

    n_aggr = 0;
    while (fgets(line, sizeof line, f)) {
        char *h = strchr(line, '#');
        char *eq, *tok;
        real sign = 1.0;
        int  i;

        if (h) *h = '\0';
        eq = strchr(line, '=');
        if (eq == NULL) {
            int only_ws = 1; char *c;
            for (c = line; *c; c++) if (!isspace((unsigned char)*c)) only_ws = 0;
            if (only_ws) continue;
            fprintf(stderr, "Error: bad line in %s (expected NAME = +A -B): %s",
                    path, line);
            fclose(f); return -1;
        }
        *eq = '\0';

        if (n_aggr >= MAX_AGGR) {
            fprintf(stderr, "Error: too many aggregates (max %d)\n", MAX_AGGR);
            fclose(f); return -1;
        }
        n_aggr++;
        for (i = 1; i <= n_ser; i++) aggr_c[n_aggr][i] = 0.0;

        if (sscanf(line, " %39s", aggr_name[n_aggr]) != 1) {
            fprintf(stderr, "Error: aggregate with no name in %s\n", path);
            fclose(f); return -1;
        }

        for (tok = strtok(eq + 1, " \t\n\r"); tok; tok = strtok(NULL, " \t\n\r")) {
            if (strcmp(tok, "+") == 0) { sign =  1.0; continue; }
            if (strcmp(tok, "-") == 0) { sign = -1.0; continue; }
            if (tok[0] == '+') { sign =  1.0; tok++; }
            else if (tok[0] == '-') { sign = -1.0; tok++; }
            if (*tok == '\0') continue;

            i = series_index(tok);
            if (i == 0) {
                fprintf(stderr, "Error: unknown series '%s' in %s\n", tok, path);
                fclose(f); return -1;
            }
            aggr_c[n_aggr][i] += sign;
            sign = 1.0;
        }
    }
    fclose(f);
    return n_aggr;
}

/* Orden topologico: una serie solo se puede construir (y prever) despues de
   TODAS las que la alimentan. Si hay un ciclo el sistema es simultaneo y no se
   puede resolver restando transferencias: hay que decirlo, no estimar basura. */
static int topo_sort(void)
{
    int indeg[MAX_SER + 1], i, k, nt = 0, changed;
    char done[MAX_SER + 1];

    for (i = 1; i <= n_ser; i++) { indeg[i] = 0; done[i] = 0; }
    for (k = 1; k <= n_link; k++) indeg[lnk[k].out]++;

    do {
        changed = 0;
        for (i = 1; i <= n_ser; i++) {
            if (done[i] || indeg[i] > 0) continue;
            topo[++nt] = i;
            done[i] = 1;
            changed = 1;
            for (k = 1; k <= n_link; k++)
                if (lnk[k].inp == i) indeg[lnk[k].out]--;
        }
    } while (changed);

    if (nt < n_ser) {
        fprintf(stderr, "Error: the transfer network has a CYCLE: the system is\n"
                        "       simultaneous and cannot be cast as a triangular\n"
                        "       VARMA by subtracting transfers.\n");
        return 0;
    }
    return 1;
}

static void print_network(FILE *out)
{
    int k;
    fprintf(out, "\nTransfer network (%d link(s)):\n", n_link);
    for (k = 1; k <= n_link; k++)
        fprintf(out, "  %-14s <- %-14s  b=%d, r=%d, s=%d\n",
                Ts[lnk[k].out].name ? Ts[lnk[k].out].name : "?",
                Ts[lnk[k].inp].name ? Ts[lnk[k].inp].name : "?",
                lnk[k].b, lnk[k].r, lnk[k].s);
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
"    Y_t = SUM_j [omega_j(B)/delta_j(B)] B^b_j X_j,t + N_t\n"
"\n"
"and, with -n, a NETWORK of such equations, in which a series may be at once an\n"
"output and an input (see THE TRANSFER NETWORK below).\n"
"\n"
"Usage: %s output.pre input1.pre [input2.pre ...] [options]\n"
"       The FIRST file is the output (Y); the rest are the exogenous inputs.\n"
"       Up to 7 inputs (a VARMA cast of up to 8 series).\n"
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
"SHARED AND FIXED PARAMETERS\n"
"  -c FILE  constraints file. A parameter may appear in SEVERAL places of the\n"
"           structure with a SINGLE degree of freedom -- which is what makes a\n"
"           transfer function rational inside a system. Use the SAME names the\n"
"           program prints:\n"
"\n"
"             delta1[1] = phi_2[B^1]   # share: the transfer denominator IS the\n"
"                                      # input's own AR (a rational transfer)\n"
"             omega1[0] = omega2[0]    # share between two inputs\n"
"             omega2[1] = 0.0          # fix at a value\n"
"             q[2,1]    = free         # free an innovation covariance\n"
"\n"
"           The innovation covariances q[i,j] are the one thing that starts out\n"
"           FIXED (at zero): a diagonal covariance is the default, and freeing one\n"
"           is a modelling decision, not a switch. Mauricio's m6-1 frees three of\n"
"           its fifteen. Sigma = sigma2*Q is normalized with Q[1,1] = 1 -- that\n"
"           decomposition is not unique (Mauricio 1995, eq. 2.1), and without the\n"
"           normalization the Hessian is exactly singular.\n"
"\n"
"THE TRANSFER NETWORK\n"
"  -n FILE  declare a NETWORK of transfers (a DAG) instead of the default star.\n"
"           A series may RECEIVE transfers and be an INPUT to another one: that\n"
"           is what a real system is (in Mauricio's m6: EC -> EU -> EI -> EP).\n"
"           One link per line, output first:\n"
"\n"
"             OUTPUT <- INPUT   b r s\n"
"\n"
"           naming the series by the name in its .pre or by its position on the\n"
"           command line.  For instance, the chain X -> M -> Y:\n"
"\n"
"             Y <- M   2 0 0     # M feeds Y with a delay of 2\n"
"             M <- X   1 0 0     # and X feeds M\n"
"\n"
"           Here X affects Y only INDIRECTLY.  A cycle is rejected: the system\n"
"           would be simultaneous and cannot be cast as a triangular VARMA.\n"
"           Without -n, every input feeds the first file (the star), and -b/-r/-s\n"
"           give the orders.\n"
"\n"
"RECURSIVE FORECAST EVALUATION  (out of sample)\n"
"  -R E     estimate ONCE on observations 1..E, then hold the parameters FIXED\n"
"           and roll the forecast origin forward one datum at a time, comparing\n"
"           each forecast with what actually happened. Reports MAE, RMSE and MAPE\n"
"           by horizon. Needs -f H.\n"
"\n"
"           The variances the model reports are THEORETICAL: they say what the\n"
"           model implies, not what happens out of sample, where parameter\n"
"           uncertainty and structural change have their say. This is the only\n"
"           way to decide EMPIRICALLY whether one model forecasts better than\n"
"           another. Run it on two specifications and compare.\n"
"  -C FILE  write the per-origin errors to FILE (CSV).\n"
"\n"
"AGGREGATES  (accounting identities)\n"
"  -a FILE  linear combinations of the series, reported with the forecast:\n"
"\n"
"             OCUPADOS = + EA + EP + EI + EU + EC\n"
"             PARADOS  = + ACTIVOS - EA - EP - EI - EU - EC\n"
"\n"
"           An identity does NOT belong in the model: it is computed AFTER\n"
"           forecasting. What is not trivial is its band. The forecast errors of\n"
"           the series are CORRELATED -- through the network they share\n"
"           innovations -- so the variance of an aggregate is not the sum of the\n"
"           variances. It is c'Vc, with V the full forecast error covariance.\n"
"           Requires -f.\n"
"\n"
"THE CAST\n"
"  -V       EMBED the transfer in the VARMA instead of subtracting it.\n"
"\n"
"           By default drtran builds the noise outside the likelihood engine,\n"
"           N_t = w_Y,t - SUM_k nu_k w_X,{t-k}, which at t=1 needs input values\n"
"           that DO NOT EXIST. It sets them to zero. The engine cannot fix this,\n"
"           because it never sees those inputs: it is handed the noise already\n"
"           contaminated.\n"
"\n"
"           With -V nothing is subtracted. The transfer becomes OFF-DIAGONAL\n"
"           coefficients of the VARMA,\n"
"\n"
"             [phi_i.D_i] w_i - SUM_k [phi_i.omega_k.B^bk.(D_i/delta_k)] w_in\n"
"                                                        = [D_i.theta_i] a_i\n"
"\n"
"           and the exact likelihood does the pre-sample initialisation itself.\n"
"           The truncation is not fixed: it DISAPPEARS. This is what Mauricio's\n"
"           own m6 models do, and it makes backforecasting (Box-Jenkins, TASTE)\n"
"           unnecessary rather than better.\n"
"\n"
"           Cost: none measurable. Gain: the reported likelihood is the EXACT\n"
"           likelihood of the data, so LR tests and information criteria are\n"
"           valid; and omega is unbiased. On a 69-observation sample with a\n"
"           rational transfer the bias of omega falls from +0.0017 to -0.0002,\n"
"           though RMSE improves by well under 1%: the truncation matters less\n"
"           than one would fear.\n"
"\n"
"IDENTIFICATION\n"
"  -p       PREWHITEN ONLY: filter the input with its own ARMA, apply the same\n"
"           filter to the output, plot the CCF and suggest (b, r, s).\n"
"           Does NOT estimate and does NOT iterate.\n"
"\n"
"TRANSFER FUNCTION  (one per input)\n"
"  -b N     pure delay B^b                    (default: identified)\n"
"  -r N     order of the denominator delta(B) (default: identified)\n"
"  -s N     order of the numerator omega(B)   (default: identified)\n"
"           With several inputs, give a comma-separated list, one value per\n"
"           input:  -b 1,0  -s 0,1.  A single value applies to ALL inputs.\n"
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
"  %s CPI.pre WTI.pre EUR.pre         TWO inputs (a 3-variate VARMA cast)\n"
"  %s CPI.pre WTI.pre EUR.pre -b 1,0 -s 0,1    one (b,r,s) per input\n"
"\n"
"drtran is free software under the GNU General Public License v2 or later.\n"
"It embeds the exact VARMA likelihood engine of drvarma/ART. See COPYING.\n",
        DRTRAN_VERSION, prog, prog, prog, prog, prog, prog, prog, prog);
}

/* -------------------------------------------------------------------------- */
/* main                                                                       */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* TABLA DE SLOTS: parámetros libres, FIJOS y COMPARTIDOS                      */
/*                                                                            */
/* Rescatado del TASTE de Treadway (1991): su TFINPUT/USMODEL llevan un        */
/* "point: Apuntador a parms", un entero por parámetro que dice qué posición   */
/* del vector ocupa. Aquí se generaliza: cada parámetro estructural es un      */
/* SLOT que puede ser                                                          */
/*                                                                            */
/*     LIBRE      -> lo estima el optimizador                                  */
/*     FIJO       -> vale una constante                                        */
/*     COMPARTIDO -> es el MISMO parámetro que otro slot                       */
/*                                                                            */
/* El optimizador solo ve los libres; shootx expande ese vector corto a la     */
/* estructura completa. Compartir parámetros es lo que hace que la             */
/* transferencia de un sistema sea racional: en los modelos m6, el mismo x6    */
/* aparece en la dinámica propia de EI y en la transferencia EI->EP.           */
/*                                                                            */
/* Los nombres de los slots son los MISMOS que el programa imprime, así que    */
/* las restricciones se escriben leyendo la salida:                            */
/*                                                                            */
/*     delta1[1] = phi_2[B^1]      # el denominador de la TF = el AR de la     */
/*                                 # entrada: la firma de una TF racional      */
/*     omega1[0] = omega2[0]       # compartir entre entradas                  */
/*     omega2[1] = 0.0             # fijar                                     */
/* -------------------------------------------------------------------------- */
#define MAX_SLOT   400
#define SLOT_FREE  0
#define SLOT_FIXED 1
#define SLOT_ALIAS 2

static char slot_name[MAX_SLOT + 1][40];
static int  slot_kind[MAX_SLOT + 1];
static int  slot_alias[MAX_SLOT + 1];
static real slot_value[MAX_SLOT + 1];
static int  free_of_slot[MAX_SLOT + 1];   /* slot -> índice libre (0 si no)  */
static int  slot_of_free[MAX_SLOT + 1];   /* índice libre -> slot            */
static int  n_slot = 0, n_free = 0;
static real xfull[MAX_SLOT + 1];          /* la estructura completa          */

static void add_slot(const char *fmt, ...)
{
    va_list ap;
    if (n_slot >= MAX_SLOT) return;
    n_slot++;
    va_start(ap, fmt);
    vsnprintf(slot_name[n_slot], sizeof slot_name[0], fmt, ap);
    va_end(ap);
    slot_kind[n_slot]  = SLOT_FREE;
    slot_alias[n_slot] = 0;
    slot_value[n_slot] = 0.0;
}

/* Nombres de los factores ARMA, en el MISMO orden que pack_ar/ma_factors */
static void add_arma_slots(struct Tusmodel *Tmi, int i, int is_ar)
{
    const char *sym = is_ar ? "phi" : "theta";
    int Num1 = is_ar ? Tmi->NumAr1  : Tmi->NumMa1;
    int Num2 = is_ar ? Tmi->NumAr2  : Tmi->NumMa2;
    int Numf = is_ar ? Tmi->NumAr1f : Tmi->NumMa1f;
    int *o1 = is_ar ? Tmi->p1 : Tmi->q1;
    int *o2 = is_ar ? Tmi->p2 : Tmi->q2;
    int **f1 = is_ar ? Tmi->Ia1  : Tmi->Im1;
    int **f2 = is_ar ? Tmi->Ia2  : Tmi->Im2;
    int  *ff = is_ar ? Tmi->Ia1f : Tmi->Im1f;
    int  *fr = is_ar ? Tmi->pfre1 : Tmi->qfre1;
    int k, j;

    for (k = 1; k <= Num1; k++)
        for (j = 1; j <= o1[k]; j++)
            if (f1[k][j] == 1) add_slot("%s_%d[B^%d]", sym, i, j);
    for (k = 1; k <= Num2; k++)
        for (j = 1; j <= o2[k]; j++)
            if (f2[k][j] == 1) add_slot("%s_%d[B^%d]", sym, i, j * Tmi->sper);
    for (k = 1; k <= Numf; k++)
        if (ff[k] == 1) add_slot("%s_%d[f=%d]", sym, i, fr[k]);
}

/* Construye la tabla de slots EN EL MISMO ORDEN que el vector de parámetros */
static void build_slots(void)
{
    int i, j, k;

    n_slot = 0;

    for (j = 1; j <= n_link; j++) {
        for (k = 0; k <= lnk[j].s; k++) add_slot("omega%d[%d]", j, k);
        for (k = 1; k <= lnk[j].r; k++) add_slot("delta%d[%d]", j, k);
    }
    for (i = 1; i <= n_ser; i++) {
        if (fix_arma[i]) continue;
        add_arma_slots(&Tm[i], i, 1);
        add_arma_slots(&Tm[i], i, 0);
    }
    for (i = 1; i <= n_ser; i++) {
        int iv, kk;
        if (fix_det[i]) continue;
        for (iv = 1; iv <= Tm[i].NdetVar; iv++) {
            for (kk = 0; kk <= Tm[i].Nomega[iv]; kk++)
                if (Tm[i].Imega[iv][kk] == 1)
                    add_slot("omega_d%d[%d,%d]", i, iv, kk);
            for (kk = 1; kk <= Tm[i].Ndelta[iv]; kk++)
                if (Tm[i].Ielta[iv][kk] == 1)
                    add_slot("delta_d%d[%d,%d]", i, iv, kk);
        }
    }
    for (i = 1; i <= n_ser; i++)
        if (!fix_mu[i]) add_slot("mu[%d]", i);
    for (i = 2; i <= n_ser; i++)
        add_slot("log(var%d/var1)", i);

    /* Covarianzas de las innovaciones. Van SIEMPRE al mapa, pero FIJAS EN CERO:
       la covarianza diagonal es el caso por defecto, y liberar una covarianza es
       una decision del analista, no algo que se active en bloque. m6-1 no libera
       las 15 de su sistema: libera TRES (sigma42, sigma62, sigma54). El fichero
       de restricciones lo dice en el mismo sitio y con el mismo lenguaje que
       todo lo demas:   q[4,2] = free                                          */
    for (i = 2; i <= n_ser; i++)
        for (j = 1; j < i; j++) {
            add_slot("q[%d,%d]", i, j);
            slot_kind[n_slot]  = SLOT_FIXED;
            slot_value[n_slot] = 0.0;
        }
}

static int find_slot(const char *name);

/* --------------------------------------------------------------------------
   Aviso de casi-colinealidad: una transferencia CONTEMPORANEA (b=0) y la
   covarianza de las innovaciones de esas dos series explican LO MISMO en el
   retardo k=0. Solo se separan por como decae la covarianza cruzada en k>0:
   phi_X^k si es transferencia, phi_N^k si es covarianza. Si los dos AR se
   parecen, la identificacion es debil y el optimizador se va por una cresta:
   medido en IPC<-WTI (phi_X=0.30, phi_N=0.40) la verosimilitud no mejora
   (LR=0.03) pero la correlacion se va a -0.98, omega_0 se multiplica por 9 y
   los t-ratios llegan a 2424.

   Por eso la doctrina de la escuela es usar UNA de las dos, no las dos: m6-1
   tiene covarianzas fuera de la diagonal y NINGUNA estructura contemporanea, y
   la tesis de Munoz Polo (2001, sec. 2.4) dice que la especificacion de una
   relacion bivariante "puede comenzar con la modificacion de la matriz Sigma".
   -------------------------------------------------------------------------- */
static void warn_contemp_collinear(FILE *out)
{
    int k, s1, s2;
    char nm[40];

    for (k = 1; k <= n_link; k++) {
        if (lnk[k].b != 0 || lnk[k].s < 0) continue;

        snprintf(nm, sizeof nm, "q[%d,%d]", lnk[k].out, lnk[k].inp);
        s1 = find_slot(nm);
        snprintf(nm, sizeof nm, "q[%d,%d]", lnk[k].inp, lnk[k].out);
        s2 = find_slot(nm);

        if ((s1 && slot_kind[s1] == SLOT_FREE) ||
            (s2 && slot_kind[s2] == SLOT_FREE)) {
            fprintf(out,
"\n*** WARNING: near-collinearity.\n"
"    Link %d (%s <- %s) is CONTEMPORANEOUS (b=0) and its innovation\n"
"    covariance is FREE at the same time. At lag k=0 the two explain the very\n"
"    same thing; they part company only through the decay of the cross-\n"
"    covariance at k>0 (phi_X^k for the transfer, phi_N^k for the covariance).\n"
"    When the two AR structures are close, the likelihood has a near-flat ridge:\n"
"    the fit barely improves while omega and the correlation run off to a corner\n"
"    with enormous t-ratios. Use ONE of the two, not both.\n", k,
                Ts[lnk[k].out].name ? Ts[lnk[k].out].name : "?",
                Ts[lnk[k].inp].name ? Ts[lnk[k].inp].name : "?");
        }
    }
}

static int find_slot(const char *name)
{
    int i;
    for (i = 1; i <= n_slot; i++)
        if (strcmp(slot_name[i], name) == 0) return i;
    return 0;
}

/* Lee el fichero de restricciones:
     NOMBRE = NOMBRE   compartir (un solo grado de libertad en varios sitios)
     NOMBRE = valor    fijar
     NOMBRE = free     liberar (las covarianzas q[i,j] nacen fijas en cero)
   Comentarios con '#'.                                                       */
static int read_constraints(const char *path)
{
    FILE *f = fopen(path, "r");
    char line[256];
    int nc = 0;

    if (f == NULL) {
        fprintf(stderr, "Error opening constraints file: %s\n", path);
        return -1;
    }

    while (fgets(line, sizeof line, f)) {
        char lhs[64], rhs[64];
        char *hash = strchr(line, '#');
        int a, b;
        double v;

        if (hash) *hash = '\0';
        if (sscanf(line, " %63[^= \t] = %63s", lhs, rhs) != 2) continue;

        a = find_slot(lhs);
        if (a == 0) {
            fprintf(stderr, "Error: unknown parameter '%s' in %s\n", lhs, path);
            fclose(f);
            return -1;
        }

        if (strcmp(rhs, "free") == 0) {     /* LIBERAR (una covarianza) */
            slot_kind[a]  = SLOT_FREE;
            slot_alias[a] = 0;
            nc++;
            continue;
        }

        b = find_slot(rhs);
        if (b != 0) {                       /* COMPARTIDO */
            /* seguir la cadena hasta el representante final */
            while (slot_kind[b] == SLOT_ALIAS) b = slot_alias[b];
            if (b == a) {
                fprintf(stderr, "Error: '%s' cannot be shared with itself\n", lhs);
                fclose(f);
                return -1;
            }
            slot_kind[a]  = SLOT_ALIAS;
            slot_alias[a] = b;
        } else if (sscanf(rhs, "%lf", &v) == 1) {   /* FIJO */
            slot_kind[a]  = SLOT_FIXED;
            slot_value[a] = v;
        } else {
            fprintf(stderr, "Error: cannot parse '%s = %s' in %s\n", lhs, rhs, path);
            fclose(f);
            return -1;
        }
        nc++;
    }
    fclose(f);
    return nc;
}

/* Mapas libre <-> slot. n_free es lo que ve el optimizador. */
static void resolve_slots(void)
{
    int i;
    n_free = 0;
    for (i = 1; i <= n_slot; i++) {
        free_of_slot[i] = 0;
        if (slot_kind[i] == SLOT_FREE) {
            n_free++;
            free_of_slot[i]   = n_free;
            slot_of_free[n_free] = i;
        }
    }
}

/* --------------------------------------------------------------------------
   CARACTERIZACION DE LA FLT: ganancia y retardo medio.

   Una transferencia estimada no esta DESCRITA hasta que se dicen dos cosas:
   cuanto responde el output en total, y cuanto TARDA en hacerlo. Los omegas y
   deltas sueltos no lo dicen; son la parametrizacion, no la respuesta.

     ganancia (efecto total y permanente de un cambio unitario y permanente):
         g = nu(1) = omega(1) / delta(1),
         omega(1) = SUM_k omega_k,   delta(1) = 1 - SUM_j delta_j

     retardo medio (el centro de gravedad de la respuesta; su VELOCIDAD):
         m = nu'(1)/nu(1) = b + [SUM_k k*omega_k]/omega(1)
                              + [SUM_j j*delta_j]/delta(1)

   Los errores estandar salen por el metodo DELTA con la matriz de covarianzas
   de los parametros libres: var(f) = grad' C grad. El gradiente se arma en el
   espacio LIBRE, que es donde vive C: un slot fijo no aporta nada, y uno
   compartido aporta a su representante (por eso la ganancia de una
   transferencia con delta compartido con el AR del input tiene la SE correcta
   y no la que saldria de tratar los dos como independientes).
   -------------------------------------------------------------------------- */
static void add_grad(real *g, int slot, real v)
{
    int r = slot;
    if (r <= 0) return;
    while (slot_kind[r] == SLOT_ALIAS) r = slot_alias[r];
    if (slot_kind[r] != SLOT_FREE) return;              /* fijo: no aporta */
    g[free_of_slot[r]] += v;
}

static real delta_se(real *g, real **cov, int npar)
{
    int i, j;
    real v = 0.0;
    for (i = 1; i <= npar; i++)
        for (j = 1; j <= npar; j++) v += g[i] * cov[i][j] * g[j];
    return (v > 0.0) ? sqrt(v) : 0.0;
}

static void transfer_characteristics(real *x, real **cov, int npar, FILE *out)
{
    real *xf = expand_params(x);
    real *gg = vector(1, npar);
    int   j, k, slot0, slot;
    int   any = 0;

    for (j = 1; j <= n_link; j++) if (lnk[j].s >= 0) any = 1;
    if (!any) return;

    fprintf(out, "\n");
    fprintf(out, "The transfer functions, characterized\n");
    fprintf(out, "    g = nu(1) = omega(1)/delta(1)\n");
    fprintf(out, "        the total, permanent response of the output to a unit,\n");
    fprintf(out, "        permanent change in the input.  HOW MUCH.\n");
    fprintf(out, "    m = nu'(1)/nu(1) = b + SUM k*omega_k / omega(1)\n");
    fprintf(out, "                         + SUM j*delta_j / delta(1)\n");
    fprintf(out, "        the centre of gravity of the response in time.  HOW SOON.\n");
    fprintf(out, "    Standard errors by the delta method.\n\n");
    fprintf(out, "%-22s %12s %12s %8s\n", "", "estimate", "std.error", "t");
    fprintf(out, "--------------------------------------------------------------\n");

    slot0 = 0;
    for (j = 1; j <= n_link; j++) {
        real w1 = 0.0, wk = 0.0, d1 = 1.0, dj = 0.0;
        real g, m, seg, sem;
        char lab[64];

        if (lnk[j].s < 0) continue;

        /* primer slot de este enlace */
        slot = slot0 + 1;
        for (k = 0; k <= lnk[j].s; k++) {
            w1 += xf[slot + k];
            wk += k * xf[slot + k];
        }
        for (k = 1; k <= lnk[j].r; k++) {
            real dv = xf[slot + lnk[j].s + k];
            d1 -= dv;
            dj += k * dv;
        }
        slot0 += (lnk[j].s + 1) + lnk[j].r;

        if (net_is_star())
            snprintf(lab, sizeof lab, "input %d", j);
        else
            snprintf(lab, sizeof lab, "%s <- %s",
                     Ts[lnk[j].out].name, Ts[lnk[j].inp].name);
        fprintf(out, "%s:\n", lab);

        if (fabs(d1) < 1e-8 || fabs(w1) < 1e-12) {
            fprintf(out, "  (delta(1) or omega(1) too close to zero: "
                         "gain and mean lag are not defined)\n");
            continue;
        }

        g = w1 / d1;
        m = lnk[j].b + wk / w1 + dj / d1;

        /* --- gradiente de la GANANCIA ---------------------------------- */
        for (k = 1; k <= npar; k++) gg[k] = 0.0;
        for (k = 0; k <= lnk[j].s; k++)
            add_grad(gg, slot + k, 1.0 / d1);                 /* d g / d omega_k */
        for (k = 1; k <= lnk[j].r; k++)
            add_grad(gg, slot + lnk[j].s + k, g / d1);        /* d g / d delta_k */
        seg = delta_se(gg, cov, npar);

        fprintf(out, "  %-20s %12.6f %12.6f %8.2f\n", "gain", g, seg,
                (seg > 1e-15) ? g / seg : 0.0);
        sum_gain[j] = g;
        sum_mlag[j] = m;

        /* --- gradiente del RETARDO MEDIO -------------------------------- */
        for (k = 1; k <= npar; k++) gg[k] = 0.0;
        {
            real A = wk / w1;            /* aporte del numerador */
            real C = dj / d1;            /* aporte del denominador */
            for (k = 0; k <= lnk[j].s; k++)
                add_grad(gg, slot + k, (k - A) / w1);
            for (k = 1; k <= lnk[j].r; k++)
                add_grad(gg, slot + lnk[j].s + k, (k + C) / d1);
        }
        sem = delta_se(gg, cov, npar);

        fprintf(out, "  %-20s %12.6f %12.6f %8.2f\n", "mean lag", m, sem,
                (sem > 1e-15) ? m / sem : 0.0);
        fprintf(out, "  %-20s %12d\n", "pure delay b", lnk[j].b);
    }
    fprintf(out, "--------------------------------------------------------------\n");
    fprintf(out, "  The mean lag is in periods, measured from t. It cannot be below\n");
    fprintf(out, "  the pure delay b: the response cannot arrive before it starts.\n");

    free_vector(gg, 1, npar);
}


/* Expande el vector corto (libres) a la estructura completa. Es lo que hace
   que un parámetro compartido aparezca en DOS sitios con un solo grado de
   libertad.                                                                 */
real *expand_params(real *xfree)
{
    int i;
    for (i = 1; i <= n_slot; i++) {
        if (slot_kind[i] == SLOT_FREE)       xfull[i] = xfree[free_of_slot[i]];
        else if (slot_kind[i] == SLOT_FIXED) xfull[i] = slot_value[i];
    }
    for (i = 1; i <= n_slot; i++)
        if (slot_kind[i] == SLOT_ALIAS) xfull[i] = xfull[slot_alias[i]];
    return xfull;
}

/* -------------------------------------------------------------------------- */
/* estimate_and_report: estima por ML exacta e informa                         */
/* -------------------------------------------------------------------------- */
static void estimate_and_report(real *x, int npar, int fc_horizon,
                                const char *yname)
{
    struct Tvarma varma1;
    real  *dev   = vector(1, npar);
    real **cov   = matrix(1, npar, 1, npar);
    real **a_est = matrix(1, n_stat, 1, n_ser);
    int    maxits = 500, nrits = 10, ifault = 0;
    real   gradtol = 1e-7, steptol = 1e-7;
    int    i, j, pi;
    real   tstat, pval;

    varma1.xitol = -1e-3;              /* verosimilitud exacta */

    fprintf(outputv, "Estimating transfer function model...\n");

    est(shootx, npar, x, dev, cov, maxits, nrits,
        gradtol, steptol, varma1.xitol, a_est,
        &varma1.sigma2, &varma1.logelf, &ifault);

    {
        const char *why, *verdict;
        switch (opt_termcode) {
        case 1:  verdict = "CONVERGENCE OBTAINED";
                 why = "gradient stopping criterium satisfied";        break;
        case 2:  verdict = "CONVERGENCE OBTAINED";
                 why = "parameter stopping criterium satisfied";       break;
        case 3:  verdict = "STOPPED AT A POINT WITH NO IMPROVEMENT";
                 why = "last global step failed to locate a lower point "
                       "(usual when starting AT the optimum)";         break;
        case 4:  verdict = "*** NO CONVERGENCE ***";
                 why = "ITERATION LIMIT REACHED";                      break;
        case 5:  verdict = "*** NO CONVERGENCE ***";
                 why = "five consecutive steps of max length";         break;
        default: verdict = "*** NO CONVERGENCE ***";
                 why = "unknown";                                      break;
        }
        sum_logl = varma1.logelf;  sum_npar = npar;
        sum_conv = verdict;        sum_why = why;   sum_fault = ifault;

        fprintf(outputv, "\n**** %s AFTER %d ITERATIONS (of %d)\n",
                verdict, opt_iters, maxits);
        fprintf(outputv, "**** %s\n", why);
        if (ifault != 0)
            fprintf(outputv, "**** ifault = %d (estimates not reliable)\n", ifault);
        fprintf(outputv, "\nLog-likelihood = %.6f\n\n", varma1.logelf);
    }

    fprintf(outputv, "=============================================================\n");
    fprintf(outputv, "  Estimated Parameters and Standard Deviations               \n");
    fprintf(outputv, "=============================================================\n\n");
    fprintf(outputv, "                        Parameter     Estimate    Std.Error   t-stat  p-val\n");
    fprintf(outputv, "--------------------------------------------------------------------\n");

    /* Un recorrido por SLOTS: cada parámetro estructural es libre, fijo o
       compartido con otro. Los compartidos muestran el mismo valor y el mismo
       error estándar que su representante, y dicen con quién comparten.     */
    {
        real *xf = expand_params(x);

        for (i = 1; i <= n_slot; i++) {
            if (slot_kind[i] == SLOT_FIXED) {
                fprintf(outputv, "%-20s %12.6f       (fixed)\n",
                        slot_name[i], xf[i]);
            } else if (slot_kind[i] == SLOT_ALIAS) {
                fprintf(outputv, "%-20s %12.6f       (= %s)\n",
                        slot_name[i], xf[i], slot_name[slot_alias[i]]);
            } else {
                pi = free_of_slot[i];
                dev[pi] = (cov[pi][pi] > 0) ? sqrt(cov[pi][pi]) : 0.0;
                tstat = (dev[pi] > 1e-15) ? x[pi] / dev[pi] : 0.0;
                pval  = 2.0 * (1.0 - normal_cdf(fabs(tstat)));
                fprintf(outputv, "%-20s %12.6f %12.6f %8.3f %6.4f %s\n",
                        slot_name[i], x[pi], dev[pi], tstat, pval,
                        (pval < 0.001) ? "***" : (pval < 0.01) ? "**" :
                        (pval < 0.05) ? "*" : (pval < 0.1) ? "." : "");
            }
        }
    }

    /* Los coeficientes que el .pre deja FIJOS no son slots (no tienen grado de
       libertad), pero forman parte del modelo y hay que verlos.             */
    for (i = 1; i <= n_ser; i++) {
        int k, iv;
        char lab[40];

        for (k = 1; k <= p_ord[i]; k++)
            if (fix_arma[i])
                { char lb[40]; snprintf(lb, sizeof lb, "phi_%d[B^%d]", i, k);
                  fprintf(outputv, "%-20s (fixed by .pre %12.6f)\n", lb, phi[i][k]); }
        for (k = 1; k <= q_ord[i]; k++)
            if (fix_arma[i])
                { char lb[40]; snprintf(lb, sizeof lb, "theta_%d[B^%d]", i, k);
                  fprintf(outputv, "%-20s (fixed by .pre %12.6f)\n", lb, theta[i][k]); }

        if (!fix_arma[i]) {
            int f, j2;
            for (f = 1; f <= Tm[i].NumAr1; f++)
                for (j2 = 1; j2 <= Tm[i].p1[f]; j2++)
                    if (Tm[i].Ia1[f][j2] != 1)
                        { char lb[40]; snprintf(lb, sizeof lb, "phi_%d[B^%d]", i, j2);
                          fprintf(outputv, "%-20s (fixed by .pre %12.6f)\n", lb, Tm[i].Ar1[f][j2]); }
            for (f = 1; f <= Tm[i].NumMa1; f++)
                for (j2 = 1; j2 <= Tm[i].q1[f]; j2++)
                    if (Tm[i].Im1[f][j2] != 1)
                        { char lb[40]; snprintf(lb, sizeof lb, "theta_%d[B^%d]", i, j2);
                          fprintf(outputv, "%-20s (fixed by .pre %12.6f)\n", lb, Tm[i].Ma1[f][j2]); }
        }

        if (!fix_det[i])
            for (iv = 1; iv <= Tm[i].NdetVar; iv++)
                for (k = 0; k <= Tm[i].Nomega[iv]; k++)
                    if (Tm[i].Imega[iv][k] != 1) {
                        snprintf(lab, sizeof lab, "omega_d%d[%d,%d]", i, iv, k);
                        fprintf(outputv, "%-20s (fixed by .pre %12.6f)\n",
                                lab, Tm[i].Omega[iv][k]);
                    }

        if (fix_mu[i]) {
            snprintf(lab, sizeof lab, "mu[%d]", i);
            fprintf(outputv, "%-20s (fixed by .pre %12.6f)\n", lab, mu[i]);
        }
    }

    fprintf(outputv, "--------------------------------------------------------------------\n");
    fprintf(outputv, "Signif. codes:  0 '***' 0.001 '**' 0.01 '*' 0.05 '.' 0.1 ' ' 1\n\n");

    transfer_characteristics(x, cov, npar, outputv);
    impulse_response_report(x, cov, npar, outputv);

    /* Q es la covarianza NORMALIZADA (Q[1,1] = 1); la real es Sigma = sigma2*Q.
       La normalizacion NO es cosmetica: la verosimilitud concentrada es invariante
       ante Q -> cQ (Mauricio 1995, ec. 3.1-3.3), asi que sin fijar Q[1,1] habria
       una direccion exactamente plana y el hessiano seria singular.            */
    {
        struct Tvarma vq;
        int ifq = 0, jj;

        fprintf(outputv, "sigma2 (concentrated) = %.6f\n", varma1.sigma2);
        fprintf(outputv, "  (Q is normalized with Q[1,1] = 1; sigma2 carries the scale)\n\n");

        shootx(x, &vq, &ifq, 1, 0);
        if (ifq == 0) {
            fprintf(outputv, "Sigma = sigma2 * Q  (innovation covariance):\n");
            for (i = 1; i <= n_ser; i++) {
                fprintf(outputv, "  ");
                for (jj = 1; jj <= n_ser; jj++)
                    fprintf(outputv, "%12.6f", varma1.sigma2 * vq.qq[i][jj]);
                fprintf(outputv, "\n");
            }
            fprintf(outputv, "\nInnovation correlations:\n");
            for (i = 1; i <= n_ser; i++) {
                fprintf(outputv, "  ");
                for (jj = 1; jj <= n_ser; jj++)
                    fprintf(outputv, "%12.4f", vq.qq[i][jj] /
                            sqrt(vq.qq[i][i] * vq.qq[jj][jj]));
                fprintf(outputv, "\n");
            }
            fprintf(outputv, "\n");
        }
        shootx(x, &vq, &ifq, 0, 1);
    }

    {
        struct Tvarma vdiag;
        int ifault_diag = 0, t;

        shootx(x, &vdiag, &ifault_diag, 1, 0);
        if (ifault_diag == 0) {
            for (t = 1; t <= n_stat; t++)
                for (i = 1; i <= n_ser; i++) vdiag.a[t][i] = a_est[t][i];

            fprintf(outputv, "\n");
            diagnose(&vdiag);
            fprintf(outputv, "\n--- Multivariate diagnostics (Hosking + JB) ---\n");
            multivariate_diagnostics(a_est, n_stat, n_ser, outputv);

            for (j = 1; j <= n_link; j++) link_p_transfer[j] = -1.0;
            for (j = 1; j <= n_link; j++)
                if (lnk[j].s >= 0) {
                    real pt = -1.0, pe = -1.0;
                    transfer_adequacy(a_est, n_stat, j, outputv, &pt, &pe);
                    link_p_transfer[j] = pt;
                    if (j == 1) { sum_p_transfer = pt; sum_p_exog = pe; }
                }

            reformulation_advice(a_est, n_stat, outputv);

            if (fc_horizon > 0)
                transfer_forecast(x, npar, fc_horizon, varma1.sigma2, outputv);

            if (rec_start > 0)
                recursive_eval(x, fc_horizon, outputv);

            shootx(x, &vdiag, &ifault_diag, 0, 1);
        }
    }

    {
        int k, idx = 1;
        printf("Observations           : %d\n", n_stat);
        printf("Parameters             : %d\n", sum_npar);
        printf("\n**** %s AFTER %d ITERATIONS\n", sum_conv, opt_iters);
        printf("**** %s\n", sum_why);
        if (sum_fault)
            printf("**** ifault = %d (estimates not reliable)\n", sum_fault);
        printf("\nLog-likelihood         : %.6f\n\n", sum_logl);

        printf("Estimated model  (output: %s)\n", yname);
        for (j = 1; j <= n_link; j++) {
            if (lnk[j].s < 0) { printf("  Input %d   : no transfer\n", j); continue; }
            if (net_is_star())
                printf("  Input %d   : nu(B) = omega(B)/delta(B) * B^%d\n",
                       j, lnk[j].b);
            else
                printf("  %s <- %s : nu(B) = omega(B)/delta(B) * B^%d\n",
                       Ts[lnk[j].out].name, Ts[lnk[j].inp].name, lnk[j].b);
            for (k = 0; k <= lnk[j].s; k++, idx++)
                printf("                omega_%d = %10.6f  (t = %6.2f)\n", k,
                       x[idx], dev[idx] > 1e-15 ? x[idx] / dev[idx] : 0.0);
            for (k = 1; k <= lnk[j].r; k++, idx++)
                printf("                delta_%d = %10.6f  (t = %6.2f)\n", k,
                       x[idx], dev[idx] > 1e-15 ? x[idx] / dev[idx] : 0.0);
            if (sum_gain[j] != 0.0 || sum_mlag[j] != 0.0)
                printf("                gain    = %10.6f   mean lag = %.2f\n",
                       sum_gain[j], sum_mlag[j]);
        }
        for (i = 1; i <= n_ser; i++)
            printf("  Series %d  : AR order %d, MA order %d, %d deterministic(s)%s\n",
                   i, p_ord[i], q_ord[i], Tm[i].NdetVar,
                   fix_mu[i] ? ", mean fixed" : ", mean free");

        if (sum_p_transfer >= 0.0) {
            printf("\nDiagnostics (input 1)\n");
            printf("  Transfer adequacy   : %-9s (p = %.4f)\n",
                   sum_p_transfer < 0.05 ? "INADEQUATE" : "adequate", sum_p_transfer);
            printf("  Input exogeneity    : %-9s (p = %.4f)\n",
                   sum_p_exog < 0.05 ? "FEEDBACK!" : "ok", sum_p_exog);
        }
        printf("\nFull results written to %s\n\n", outfile_path);
    }

    free_matrix(a_est, 1, n_stat, 1, n_ser);
    free_matrix(cov, 1, npar, 1, npar);
    free_vector(dev, 1, npar);
}

/* Parsea "1,0,2" en arr[1..n]. Un solo valor se replica a todas las entradas. */
static void parse_orders(const char *str, int *arr, int n)
{
    const char *q = str;
    int j = 1, v;

    if (str == NULL) return;
    while (j <= n && sscanf(q, "%d", &v) == 1) {
        arr[j++] = v;
        q = strchr(q, ',');
        if (q == NULL) break;
        q++;
    }
    if (j == 2) for (; j <= n; j++) arr[j] = arr[1];
}

int main(int argc, char *argv[])
{
    int opt, i, j, l;
    char *outfile = NULL;
    int auto_id = 1;
    int force_fix_mu = 0;
    int fc_horizon = 0;
    int prewhiten_only = 0;
    int no_transfer = 0;
    char *model_name = NULL;
    char *opt_b = NULL, *opt_r = NULL, *opt_s = NULL;
    char *cons_file = NULL;
    char *net_file  = NULL;
    char *aggr_file = NULL;
    char outname[512];
    int fix_out_arma = 0, fix_inp_arma = 0;
    int fix_out_det  = 0, fix_inp_det  = 0;

    macheps = cmacheps();
    outputv = stdout;

    while ((opt = getopt(argc, argv, "r:s:b:f:m:c:n:a:R:C:p0XNDEMVvho:")) != -1) {
        switch (opt) {
        case 'r': opt_r = optarg; auto_id = 0; break;
        case 's': opt_s = optarg; auto_id = 0; break;
        case 'b': opt_b = optarg; auto_id = 0; break;
        case 'X': fix_inp_arma = 1;          break;
        case 'N': fix_out_arma = 1;          break;
        case 'D': fix_out_det  = 1;          break;
        case 'E': fix_inp_det  = 1;          break;
        case 'M': force_fix_mu = 1;          break;
        case 'f': fc_horizon = atoi(optarg); break;
        case 'm': model_name = optarg;       break;
        case 'p': prewhiten_only = 1;        break;
        case 'c': cons_file = optarg;        break;
        case 'n': net_file  = optarg;        break;
        case 'a': aggr_file = optarg;        break;
        case 'R': rec_start = atoi(optarg);  break;
        case 'C': snprintf(rec_csv, sizeof rec_csv, "%s", optarg); break;
        case '0': no_transfer = 1; auto_id = 0; break;
        case 'V': embed_varma = 1;           break;
        case 'v': quiet_mode = 0;            break;
        case 'o': outfile = optarg;          break;
        case 'h': usage(argv[0]); return 0;
        default:  usage(argv[0]); return 1;
        }
    }

    /* --- Series: 1 salida + N entradas --- */
    n_ser = argc - optind;
    if (n_ser < 2) {
        fprintf(stderr, "Error: at least two .pre files are needed "
                        "(one output and one input)\n\n");
        usage(argv[0]);
        return 1;
    }
    if (n_ser > MAX_SER) {
        fprintf(stderr, "Error: too many series (%d); the limit is %d\n",
                n_ser, MAX_SER);
        return 1;
    }
    n_inp = n_ser - 1;

    /* --- Nombre del modelo y fichero de resultados --- */
    if (model_name == NULL) {
        char nb[128];
        outname[0] = '\0';
        for (i = 0; i < n_ser; i++) {
            base_name(argv[optind + i], nb, sizeof nb);
            if (i) strncat(outname, "_", sizeof outname - strlen(outname) - 1);
            strncat(outname, nb, sizeof outname - strlen(outname) - 1);
        }
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

    printf("\n");
    printf("DRTRAN %s: Box-Jenkins transfer function models by exact ML\n",
           DRTRAN_VERSION);
    printf("Copyright (C) 1995-2026 A.B. Treadway, J.A. Mauricio & D.E. Guerrero\n");
    printf("Free software under the GNU GPL v2 or later; comes with NO WARRANTY.\n");
    printf("See the file COPYING for details.\n");
    printf("Non-final version. May contain errors. Please report.\n\n");
    printf("Model                  : %s\n", model_name);
    printf("Output (Y)             : %s\n", argv[optind]);
    for (j = 1; j <= n_inp; j++)
        printf("Input  (X%d)            : %s\n", j, argv[optind + j]);
    printf("Results file           : %s\n", outfile_path);
    if (prewhiten_only)
        printf("Method                 : prewhitening only (no estimation)\n");
    else
        printf("Method                 : exact maximum likelihood "
               "(%d-variate VARMA cast)\n", n_ser);

    /* --- Leer los .pre --- */
    for (i = 1; i <= n_ser; i++) {
        if (read_fue_pre(argv[optind + i - 1], &Tm[i], &Ts[i], &DataMat[i]) != 0) {
            fprintf(stderr, "Error reading %s\n", argv[optind + i - 1]);
            return 2;
        }
        mu[i]     = Tm[i].mu;
        fix_mu[i] = force_fix_mu ? 1 : !Tm[i].Imu;
        fix_arma[i] = (i == 1) ? fix_out_arma : fix_inp_arma;
        fix_det[i]  = (i == 1) ? fix_out_det  : fix_inp_det;

        if (Ts[i].nobs != Ts[1].nobs) {
            fprintf(stderr, "Error: series have different numbers of "
                            "observations (%d vs %d)\n", Ts[i].nobs, Ts[1].nobs);
            return 4;
        }
    }

    /* --- Prevision recursiva: se ESTIMA solo con la ventana de entrenamiento.
           El resto de los datos siguen en Ts[i].data; lo unico que se recorta es
           nobs. Asi la evaluacion es honestamente fuera de muestra.            */
    for (i = 1; i <= n_ser; i++) nobs_full[i] = Ts[i].nobs;
    if (rec_start > 0) {
        if (rec_start >= Ts[1].nobs) {
            fprintf(stderr, "Error: -R %d leaves no data out of sample "
                            "(%d observations)\n", rec_start, Ts[1].nobs);
            return 11;
        }
        if (fc_horizon <= 0) {
            fprintf(stderr, "Error: -R needs a horizon; give -f H\n");
            return 11;
        }
        for (i = 1; i <= n_ser; i++) Ts[i].nobs = rec_start;
        printf("Estimation window      : 1..%d  (recursive evaluation to %d)\n",
               rec_start, nobs_full[1]);
    }

    /* --- La RED: estrella por defecto, DAG si se declara con -n --- */
    for (j = 1; j <= n_inp; j++) { b_del[j] = 0; r_ord[j] = 0; s_ord[j] = 0; }
    if (no_transfer)
        for (j = 1; j <= n_inp; j++) s_ord[j] = -1;
    else {
        parse_orders(opt_b, b_del, n_inp);
        parse_orders(opt_r, r_ord, n_inp);
        parse_orders(opt_s, s_ord, n_inp);
    }
    build_default_links();

    if (net_file != NULL) {
        if (read_network(net_file) < 0) return 6;
        auto_id = 0;             /* la red trae sus propios ordenes */
    }
    if (!topo_sort()) return 7;

    if (aggr_file != NULL) {
        int na = read_aggregates(aggr_file);
        if (na < 0) return 10;
        fprintf(outputv, "\nAggregates from %s: %d\n", aggr_file, na);
        if (fc_horizon <= 0)
            fprintf(stderr, "Note: aggregates are reported with the forecast; "
                            "use -f to set a horizon.\n");
    }

    build_stationary_series();
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
    fprintf(outputv, "Series           : %d (1 output + %d input(s))\n", n_ser, n_inp);
    fprintf(outputv, "Output (Y)       : %s\n", argv[optind]);
    for (j = 1; j <= n_inp; j++)
        fprintf(outputv, "Input  (X%d)      : %s\n", j, argv[optind + j]);
    fprintf(outputv, "Frequency        : %d\n", Ts[1].freq);
    fprintf(outputv, "Start            : %d %d\n", Ts[1].begtime, Ts[1].begyear);
    fprintf(outputv, "Observations     : %d (raw %d)\n\n", n_stat, Ts[1].nobs);
    for (i = 1; i <= n_ser; i++)
        fprintf(outputv, "  series %d: Box-Cox %.2f, d=%d D=%d, %d deterministic(s)\n",
                i, Tm[i].boxlam, Tm[i].nrdiff, Tm[i].nadiff, Tm[i].NdetVar);
    fprintf(outputv, "\n");

    /* --- ARMA de cada serie --- */
    for (i = 1; i <= n_ser; i++) {
        p_ord[i] = total_ar_order(&Tm[i]);
        q_ord[i] = total_ma_order(&Tm[i]);
        if (p_ord[i] > 0) {
            phi[i] = vector(1, p_ord[i]);
            expand_ar_factors(&Tm[i], phi[i], p_ord[i]);
        }
        if (q_ord[i] > 0) {
            theta[i] = vector(1, q_ord[i]);
            expand_ma_factors(&Tm[i], theta[i], q_ord[i]);
        }
    }

    /* --- Identificacion: una por entrada --- */
    if (auto_id || prewhiten_only)
        for (j = 1; j <= n_link; j++)
            prewhiten_and_identify(j, outputv);

    if (prewhiten_only) {
        printf("Observations           : %d\n", n_stat);
        printf("\n**** PREWHITENING ONLY: no estimation performed\n\n");
        printf("Suggested transfer function orders:\n");
        for (j = 1; j <= n_link; j++)
            printf("  input %d: b = %d, r = %d, s = %d\n",
                   j, lnk[j].b, lnk[j].r, lnk[j].s);
        printf("\nCCF plots and impulse response weights written to %s\n\n",
               outfile_path);
        fclose(outputv);
        return 0;
    }

    fprintf(outputv, "\nTransfer function orders:\n");
    for (j = 1; j <= n_link; j++) {
        fprintf(outputv, "  input %d: b = %d, r = %d, s = %d\n",
                j, lnk[j].b, lnk[j].r, lnk[j].s);
        if (lnk[j].r > MAX_R) {
            fprintf(stderr, "Error: r=%d exceeds MAX_R=%d\n", lnk[j].r, MAX_R);
            return 8;
        }
        if (lnk[j].s > MAX_S) {
            fprintf(stderr, "Error: s=%d exceeds MAX_S=%d\n", lnk[j].s, MAX_S);
            return 8;
        }
    }
    if (net_file != NULL) print_network(outputv);

    /* --- Tabla de slots, restricciones y vector inicial --- */
    build_slots();

    if (cons_file != NULL) {
        int nc = read_constraints(cons_file);
        if (nc < 0) return 9;
        fprintf(outputv, "\nConstraints from %s: %d\n", cons_file, nc);
    }
    resolve_slots();
    warn_contemp_collinear(outputv);
    warn_contemp_collinear(stdout);

    fprintf(outputv, "\nStructural parameters: %d   (free: %d", n_slot, n_free);
    if (n_slot > n_free)
        fprintf(outputv, ", fixed/shared: %d", n_slot - n_free);
    fprintf(outputv, ")\n\n");

    {
        real *xs = vector(1, n_slot);      /* valores iniciales, por SLOT */
        real *x  = vector(1, n_free);      /* lo que ve el optimizador    */
        int idx = 1;

        for (j = 1; j <= n_link; j++) {
            for (l = 0; l <= lnk[j].s; l++) xs[idx++] = 0.0;
            for (l = 1; l <= lnk[j].r; l++) xs[idx++] = 0.0;
        }
        for (i = 1; i <= n_ser; i++) {
            if (fix_arma[i]) continue;
            idx += pack_ar_factors(&Tm[i], xs, idx);
            idx += pack_ma_factors(&Tm[i], xs, idx);
        }
        for (i = 1; i <= n_ser; i++)
            if (!fix_det[i]) idx += pack_det_params(&Tm[i], xs, idx);

        {
            double mm[MAX_SER + 1], vv[MAX_SER + 1];
            int t;
            for (i = 1; i <= n_ser; i++) {
                mm[i] = vv[i] = 0.0;
                for (t = 1; t <= n_stat; t++) mm[i] += w[i][t];
                mm[i] /= n_stat;
                for (t = 1; t <= n_stat; t++)
                    vv[i] += (w[i][t] - mm[i]) * (w[i][t] - mm[i]);
                vv[i] /= n_stat;
            }
            for (i = 1; i <= n_ser; i++)
                if (!fix_mu[i]) xs[idx++] = mm[i];
            for (i = 2; i <= n_ser; i++)
                xs[idx++] = log(vv[i] / vv[1]);
            for (i = 2; i <= n_ser; i++)
                for (l = 1; l < i; l++) xs[idx++] = 0.0;   /* covarianzas */
        }

        /* del vector por slots al vector LIBRE */
        for (i = 1; i <= n_slot; i++)
            if (slot_kind[i] == SLOT_FREE) x[free_of_slot[i]] = xs[i];

        estimate_and_report(x, n_free, fc_horizon, argv[optind]);

        free_vector(x, 1, n_free);
        free_vector(xs, 1, n_slot);
    }

    fclose(outputv);
    return 0;
}
