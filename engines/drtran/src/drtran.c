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
#include "dates.h"
#include "prewhiten.h"
#include "netfile.h"
#include "drtran.h"
#include "fue_pre_reader.h"
#include "forecast.h"
#include <unistd.h>   /* getopt */
#include <strings.h>  /* strcasecmp */
#include <string.h>   /* strcmp */
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

/* BUG-8. La entrada de cada enlace, DIFERENCIADA POR EL OPERADOR DE SU SALIDA.
   El modelo dice que la transferencia relaciona los NIVELES y que la
   diferenciacion la lleva el ruido; como nabla conmuta con nu(B),

       op_y N  =  op_y y  -  nu(B) (op_y x)

   asi que el vector que alimenta la transferencia es la entrada diferenciada
   por el operador de la SALIDA, no por el suyo. NULL cuando los dos operadores
   coinciden, que es todo el legacy, m6 entero y la red.                      */
real  *w_alt[MAX_LINK + 1];

/* La CABECERA de cada serie: las observaciones que el recorte a la ventana
   comun deja fuera por delante. Son datos REALES, no estimaciones, y son
   justo lo que la convolucion de la transferencia necesita antes de t=1. Se
   guardan porque trim_to_common las machaca.                                */
real  *w_head[MAX_SER + 1];
int    n_head[MAX_SER + 1];

/* Delta(B) = op_salida / op_entrada de cada enlace desajustado y ANIDADO.
   Sirve para retropronosticar la muestra previa de la entrada re-diferenciada:
   esa serie no tiene modelo propio con el que hacerlo --su MA seria
   theta(B)Delta(B), y Delta tiene raices EN el circulo unidad, asi que la
   recursion no es invertible--, de modo que se retropronostica la serie de la
   entrada, que si tiene un modelo sano, y se pasa por Delta.                 */
real  *alt_delta[MAX_LINK + 1];
int    n_alt_delta[MAX_LINK + 1];
/* Cuantas observaciones le sobran POR DELANTE a la entrada re-diferenciada
   tras el recorte. Es el desplazamiento de w_alt, NO el de la serie propia de
   la entrada: confundirlos desplaza la muestra previa entera.               */
int    n_alt_head[MAX_LINK + 1];
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
/* Origen de prevision del informe -f/-L, INDEPENDIENTE de la ventana de estimacion
   (-estwin). >0: obs explicita; <0: el final ACTUAL de los datos (SPS en tiempo
   real); 0 (por defecto): el final de la ventana de estimacion, o el final de los
   datos si no hay ventana. */
static int  fc_origin = 0;

/* EL CAST. Por defecto la transferencia va DENTRO del VARMA (empotrada), que es
   lo correcto: elf recibe las series tal cual y hace la inicializacion
   pre-muestral EXACTA, asi que la verosimilitud que se informa es la de LOS
   DATOS -- y por tanto AIC, BIC y los contrastes LR significan lo que dicen.
   Con el cast por resta, elf calculaba una verosimilitud exacta... de la serie
   equivocada.  -S vuelve al cast por resta.                                     */
int embed_varma = 1;

/* -i : identificacion de RED. Tras estimar el modelo diagonal, lee las ccf
   residuales y propone la red de transferencias + covarianzas (identify_network). */
int net_ident = 0;

/* -g NAME : modo GUIADO (driver de la escalera). Como -i, pero ademas ESCRIBE
   los artefactos NAME.dag / NAME.cns listos para -n/-c y emite el plan con el
   siguiente comando. Deja al usuario confirmar/podar y estimar (la doctrina de
   la escuela: la red identificada es una guia, no la final). */
char *guide_name = NULL;

/* -L : ademas del reporte ASCII, escribe un informe de prevision LaTeX/PDF "a la
   fuf" (<base>_forecast.tex): por serie, la tabla NIVEL/VARIACION (convencion de
   filas de fuf: L/2 de historia + L/2 meses + solo fines de año) con su grafico
   gnuplot (forecast_graphic de fuf) AL LADO, dos informes por cara.  Lo compila
   con pdflatex si esta disponible.  forecast_base es el nombre base. */
int   latex_forecast = 0;
char *forecast_base   = NULL;

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

/* total_ar_order / total_ma_order viven ahora en lib/prewhiten (prewhiten.h):
   el orden expandido de un factor anual --p*sper-- es justo lo que el GUI
   tiene que saber para preblanquear igual que el motor. */

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

/* operators_differ: comparan los DOS operadores no estacionarios completos.

   Compara el POLINOMIO, no el par (nrdiff, nadiff), y la diferencia importa:
   nabla nabla_4 escrito a la manera de la escuela --nrdiff=2, nadiff=0,
   ifadf=[0,1,1], como lo lleva EA de m6-- es EL MISMO OPERADOR que nrdiff=1,
   nadiff=1, y comparando los enteros esas dos codificaciones se leerian como
   desajuste. rnsop ya trae el polinomio armado, ifadf incluido.             */
int operators_differ(int out, int inp)
{
    return operators_differ_tm(&Tm[out], &Tm[inp]);
}

/* links_need_subtracting: 1 si algun enlace cruza operadores distintos.

   Es EL DESPACHO. El cast empotrado convierte la transferencia en coeficientes
   off-diagonal que actuan sobre las columnas de w, asi que la entrada es lo que
   w tenga para ella: una columna, una diferenciacion. El cast por resta calcula
   el termino aparte, y ahi es donde cabe el segundo vector.                  */
int links_need_subtracting(void)
{
    int j;
    for (j = 1; j <= n_link; j++)
        if (operators_differ(lnk[j].out, lnk[j].inp)) return 1;
    return 0;
}

/* poly_div: q = a / b sobre polinomios en B dados de menor a mayor grado.
   Devuelve el grado del cociente, o -1 si la division no es exacta.          */
static int poly_div(const real *a, int da, const real *b, int db, real *q)
{
    real *r;
    int i, k, dq = da - db;
    if (dq < 0) return -1;
    r = vector(0, da);
    for (i = 0; i <= da; i++) r[i] = a[i];
    for (k = dq; k >= 0; k--) {
        real c = r[k + db] / b[db];
        q[k] = c;
        for (i = 0; i <= db; i++) r[k + i] -= c * b[i];
    }
    for (i = 0; i <= da; i++)
        if (fabs(r[i]) > 1e-9) { free_vector(r, 0, da); return -1; }
    free_vector(r, 0, da);
    return dq;
}

/* backcast_arma: los L valores de una serie ARMA estacionaria ANTERIORES a su
   primera observacion. out[1] es el inmediatamente anterior, out[2] el de antes.

   Retroprevision de Box-Jenkins, y descansa en un hecho: la funcion generatriz
   de autocovarianzas de un ARMA estacionario,

       gamma(z) = sigma^2 Theta(z)Theta(1/z) / (Phi(z)Phi(1/z))

   es simetrica bajo z -> 1/z, asi que EL PROCESO INVERTIDO EN EL TIEMPO TIENE
   EL MISMO MODELO. Retropronosticar es por tanto prever la serie invertida con
   los mismos phi y theta, que es lo que hace BackForeCast en TASTE.

   Convenio de signos como en elf: a[t] = w[t] - SUM phi_i w[t-i] + SUM th_j a[t-j].
   phi[1..p] y theta[1..q] son 1-indexados; w[1..n] tambien.                  */
static void backcast_arma(const real *w, int n, const real *ph, int p,
                          const real *th, int q, real mu, int L, real *out)
{
    real *u, *e;
    int t, i, j, k;
    if (L <= 0 || n <= 0) return;
    u = vector(1, n);
    e = vector(1, n);
    for (t = 1; t <= n; t++) u[t] = w[n - t + 1] - mu;   /* invertida y centrada */
    for (t = 1; t <= n; t++) {
        real acc = u[t];
        for (i = 1; i <= p && i < t; i++) acc -= ph[i] * u[t - i];
        for (j = 1; j <= q && j < t; j++) acc += th[j] * e[t - j];
        e[t] = acc;
    }
    for (k = 1; k <= L; k++) {                            /* prevision hacia delante */
        real acc = 0.0;
        for (i = 1; i <= p; i++) {
            int idx = k - i;
            acc += ph[i] * (idx >= 1 ? out[idx] - mu : u[n + idx]);
        }
        for (j = 1; j <= q; j++) {
            int idx = n + k - j;
            if (idx <= n) acc -= th[j] * e[idx];
        }
        out[k] = acc + mu;
    }
    free_vector(u, 1, n);
    free_vector(e, 1, n);
}

/* build_pre_sample: los valores de la ENTRADA de un enlace anteriores a la
   ventana de estimacion. (*pre)[m] es el de m periodos antes del primero.
   Devuelve cuantos, y aloja (*pre) si son mas de cero.

   Dos fuentes, en este orden:
     1. Observaciones REALES que el recorte a la ventana comun dejo fuera
        (w_head). Cuando la entrada va menos diferenciada que la salida le
        sobran valores por delante: son datos, no estimaciones, y salen gratis.
     2. RETROPRONOSTICOS para lo que siga faltando, como hace TASTE.

   Solo tantos como nu pueda alcanzar. Con r=0 el filtro tiene b+s+1 pesos y
   todo lo demas es exactamente cero, asi que una transferencia contemporanea
   NO necesita muestra previa y una (0,0,1) necesita un solo valor. Son las
   transferencias RACIONALES, de cola infinita, las que el truncamiento
   estaba estropeando.                                                       */
int build_pre_sample(int j, const real *nu, int nlen, real **pre)
{
    int in = lnk[j].inp;
    int K = 0, k, m, c, need, nh, nfull;
    real *ext, *bc;

    for (k = 1; k <= nlen; k++) if (fabs(nu[k]) > 1e-12) K = k - 1;
    if (K <= 0) { *pre = NULL; return 0; }

    nh = n_head[in];
    if (w_alt[j] == NULL) {
        /* La serie propia de la entrada: cabecera + retropronostico. */
        need = K - nh; if (need < 0) need = 0;
        nfull = nh + n_stat;
        ext = vector(1, need + nfull);
        bc  = need > 0 ? vector(1, need) : NULL;
        for (k = 1; k <= nh; k++)      ext[need + k] = w_head[in][k];
        for (k = 1; k <= n_stat; k++)  ext[need + nh + k] = w[in][k];
        if (need > 0) {
            backcast_arma(ext + need, nfull, phi[in], p_ord[in],
                          theta[in], q_ord[in], mu[in], need, bc);
            for (k = 1; k <= need; k++) ext[need - k + 1] = bc[k];
            free_vector(bc, 1, need);
        }
        *pre = vector(1, K);
        for (m = 1; m <= K; m++) (*pre)[m] = ext[need + nh - m + 1];
        free_vector(ext, 1, need + nfull);
        return K;
    }

    /* Entrada re-diferenciada: se retropronostica la serie PROPIA --que tiene
       un modelo sano-- y se pasa por Delta, usando la identidad
       alt[t] = SUM_c delta[c] w_x[t+g-c].  Sin Delta no hay como hacerlo.   */
    if (alt_delta[j] == NULL) { *pre = NULL; return 0; }
    {
        int g = n_alt_delta[j];
        int off = n_alt_head[j];      /* el de w_alt, no el de la serie propia */
        need = K - off; if (need < 0) need = 0;
        nfull = nh + n_stat;
        ext = vector(1, need + nfull);
        bc  = need > 0 ? vector(1, need) : NULL;
        for (k = 1; k <= nh; k++)      ext[need + k] = w_head[in][k];
        for (k = 1; k <= n_stat; k++)  ext[need + nh + k] = w[in][k];
        if (need > 0) {
            backcast_arma(ext + need, nfull, phi[in], p_ord[in],
                          theta[in], q_ord[in], mu[in], need, bc);
            for (k = 1; k <= need; k++) ext[need - k + 1] = bc[k];
            free_vector(bc, 1, need);
        }
        *pre = vector(1, K);
        for (m = 1; m <= K; m++) {
            real acc = 0.0;
            for (c = 0; c <= g; c++) {
                int idx = need + off - m + g - c + 1;
                if (idx >= 1 && idx <= need + nfull) acc += alt_delta[j][c] * ext[idx];
            }
            (*pre)[m] = acc;
        }
        free_vector(ext, 1, need + nfull);
        return K;
    }
}

/* Construye la serie estacionaria de TODAS las series y las recorta a la
   VENTANA COMUN: cada modelo pierde tantas observaciones iniciales como el
   orden de su propio operador no estacionario. Todas arrancan en la misma
   fecha, asi que alinear por el final las alinea en el calendario.          */
void build_stationary_series(void)
{
    int nst[MAX_SER + 1], nal[MAX_LINK + 1];
    int i, j, nmin = 0;

    for (i = 1; i <= n_ser; i++) {
        if (w[i]) free_vector(w[i], 1, n_stat);
        w[i] = NULL;
    }
    for (i = 1; i <= n_ser; i++) {
        if (w_head[i]) free_vector(w_head[i], 1, n_head[i]);
        w_head[i] = NULL;  n_head[i] = 0;
    }
    for (j = 1; j <= n_link; j++) {
        if (w_alt[j]) free_vector(w_alt[j], 1, n_stat);
        w_alt[j] = NULL;
        nal[j] = 0;
        if (alt_delta[j]) free_vector(alt_delta[j], 0, n_alt_delta[j]);
        alt_delta[j] = NULL;  n_alt_delta[j] = 0;  n_alt_head[j] = 0;
    }
    for (i = 1; i <= n_ser; i++) {
        nst[i] = 0;
        apply_univariate_model(&Tm[i], &Ts[i], DataMat[i], &w[i], &nst[i]);
        if (w[i] == NULL || nst[i] <= 0) { n_stat = 0; return; }
        if (nmin == 0 || nst[i] < nmin) nmin = nst[i];
    }

    /* La entrada re-diferenciada por el operador de su salida (BUG-8). Se
       obtiene intercambiando SOLO ornsop/rnsop --que es lo unico que mira el
       diferenciador-- y llamando al MISMO apply_univariate_model: mismo
       Box-Cox, mismas deterministas, misma serie; solo cambia el operador.
       Sin segunda fuente de verdad.                                          */
    for (j = 1; j <= n_link; j++) {
        int o = lnk[j].out, in = lnk[j].inp;
        int  sav_o;
        real *sav_r;
        if (!operators_differ(o, in)) continue;
        sav_o = Tm[in].ornsop;  sav_r = Tm[in].rnsop;
        Tm[in].ornsop = Tm[o].ornsop;  Tm[in].rnsop = Tm[o].rnsop;
        apply_univariate_model(&Tm[in], &Ts[in], DataMat[in], &w_alt[j], &nal[j]);
        Tm[in].ornsop = sav_o;  Tm[in].rnsop = sav_r;
        if (w_alt[j] == NULL || nal[j] <= 0) { n_stat = 0; return; }
        if (nal[j] < nmin) nmin = nal[j];
    }

    /* Delta(B) = op_salida / op_entrada, para la muestra previa de la entrada
       re-diferenciada. Solo cuando la division es EXACTA (operadores
       anidados); si no, no hay Delta y la muestra previa se queda a cero.   */
    for (j = 1; j <= n_link; j++) {
        int o = lnk[j].out, in = lnk[j].inp, dq;
        real *cy, *cx, *qq;
        int dy, dx, t;
        if (!w_alt[j]) continue;
        dy = Tm[o].ornsop;  dx = Tm[in].ornsop;
        if (dy < dx) continue;
        cy = vector(0, dy);  cx = vector(0, dx);  qq = vector(0, dy - dx);
        for (t = 0; t <= dy; t++) cy[t] = -Tm[o].rnsop[t];   /* rnsop[0] = -1 */
        for (t = 0; t <= dx; t++) cx[t] = -Tm[in].rnsop[t];
        dq = poly_div(cy, dy, cx, dx, qq);
        if (dq >= 0) { alt_delta[j] = qq; n_alt_delta[j] = dq; }
        else free_vector(qq, 0, dy - dx);
        free_vector(cy, 0, dy);  free_vector(cx, 0, dx);
    }

    n_stat = nmin;

    /* La CABECERA: lo que el recorte deja fuera. Datos reales, y son lo
       primero que alimenta la muestra previa de la transferencia.           */
    for (i = 1; i <= n_ser; i++) {
        int t, nh = nst[i] - n_stat;
        if (nh <= 0) continue;
        w_head[i] = vector(1, nh);
        for (t = 1; t <= nh; t++) w_head[i][t] = w[i][t];
        n_head[i] = nh;
    }

    for (i = 1; i <= n_ser; i++) trim_to_common(w[i], nst[i], n_stat);
    for (j = 1; j <= n_link; j++)
        if (w_alt[j]) {
            n_alt_head[j] = nal[j] - n_stat;
            trim_to_common(w_alt[j], nal[j], n_stat);
        }
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

/* expand_ar_factors / expand_ma_factors viven ahora en lib/prewhiten, con
   apply_univariate_model: son puras y el GUI las necesita para preblanquear
   con EL MISMO codigo que el motor. */

/* -------------------------------------------------------------------------- */
/* DateToObs: convierte (año, periodo) a número de observación               */
/* -------------------------------------------------------------------------- */
/* DateToObs vive ahora en lib/dates: el GUI la necesita para poder
   enlazar fue_pre_reader.c, y aqui estaba encerrada con el main(). */

/* -------------------------------------------------------------------------- */
/* apply_univariate_model vive ahora en lib/prewhiten: es una funcion PURA    */
/* --no toca un solo global-- y el GUI la necesita para poder preblanquear    */
/* con el mismo codigo que el motor. Misma razon por la que salio DateToObs.  */
/* -------------------------------------------------------------------------- */

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

/* Varianza del error de prevision de (1-B^lag) aplicado al NIVEL de la serie i, a
   l pasos.  lag=0 -> el nivel; lag=1 -> variacion de periodo (1-B); lag=s ->
   variacion anual (1-B^s).  Pesos LP(t)-LP(t-lag), como el msfo.c del legacy
   (secciones [5]-[8]): las previsiones de nivel a l y a l-lag comparten las
   innovaciones, y su diferencia hereda los pesos restados.                       */
static real vcov_diff_at(real ***LP, real **sigma, int m, int i, int l, int lag)
{
    int t, j, k;
    real v = 0.0;
    for (t = 0; t <= l - 1; t++)
        for (j = 1; j <= m; j++)
            for (k = 1; k <= m; k++) {
                real wj = LP[i][j][t] - (lag > 0 && t - lag >= 0 ? LP[i][j][t-lag] : 0.0);
                real wk = LP[i][k][t] - (lag > 0 && t - lag >= 0 ? LP[i][k][t-lag] : 0.0);
                v += sigma[j][k] * wj * wk;
            }
    return v;
}

/* Fecha (periodo/anno) de la observacion n, dada la fecha de arranque.  Como el
   ObsToDate de fuf/forsil.  Para series anuales (freq<=1) *per queda a 0.         */
static void obs_to_date(int begyear, int begtime, int freq, int n, int *per, int *yr)
{
    if (freq <= 1) { *per = 0; *yr = begyear + (n - 1); return; }
    { int idx = (begtime - 1) + (n - 1);
      *per = idx % freq + 1;
      *yr  = begyear + idx / freq; }
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
            /* Convencion de Box-Jenkins, la MISMA que compute_irf y el cast
               empotrado: omega(B) = w0 - w1 B - w2 B^2 - ...  El termino lider
               SUMA y los demas RESTAN.  Antes se sumaban todos, lo que invertia
               el signo de nu_k para lag > 0 y, con el, la GANANCIA de la columna
               acumulada (que es nu(1) = omega(1)/delta(1)).  Con s = 0 no se
               notaba; con s > 0 el error es grande: en el caso canonico publicaba
               0.005610 = w0 + w1 donde la ganancia es w0 - w1 = 0.027194.        */
            if (lag >= 0 && lag <= sn)
                acc = (lag == 0) ? xf[slot] : -xf[slot + lag];
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

            /* d nu_k / d omega_lag.  Con la convencion BJR el lider suma y los
               demas restan, asi que la derivada respecto de omega_lag es -1 para
               lag > 0.  Tiene que ser COHERENTE con la recursion de nu de arriba
               o los errores estandar de nu_k y de la ganancia salen mal.        */
            if (lag >= 0 && lag <= sn)
                add_grad(gg, slot + lag, (lag == 0) ? 1.0 : -1.0);
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

    /* las series extendidas, en orden topologico. Con el cast EMPOTRADO la
       transferencia ya esta dentro del VARMA: no se vuelve a sumar.            */
    we = matrix(1, m, 1, n_stat + L);
    for (u = 1; u <= m; u++) {
        i = topo[u];
        for (t = 1; t <= n_stat; t++) we[i][t] = w[i][t];
        for (l = 1; l <= L; l++) {
            real acc = f1[i][l];
            int  tt  = n_stat + l;
            if (!embed_varma)
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

/* Escapa los caracteres especiales de LaTeX de un nombre (los _ de las series
   romperian el .tex).  Devuelve dst.                                             */
static const char *latex_escape(char *dst, size_t n, const char *src)
{
    size_t j = 0;
    if (src == NULL) { dst[0] = '\0'; return dst; }
    for (; *src && j + 2 < n; src++) {
        if (*src == '_' || *src == '%' || *src == '&' || *src == '#' || *src == '$')
            dst[j++] = '\\';
        dst[j++] = *src;
    }
    dst[j] = '\0';
    return dst;
}

/* Modulo grafico IMPORTADO de fuf (src/fuf_graphic.c): dibuja la variacion anual
   (historia + prevision +/- 1 DT) y los residuos (ERR) con los formatos de fuf. */
void forecast_graphic(double *data, double **res, double **f3, double ***v3,
                      int ornsop, double sigma2, int begyear, int begtime,
                      int nobs, int L, int freq, char *x11out, double refactor);
void forecast_graphic_BC(double *data, double **res, double **f1, double ***v1,
                         int ornsop, double sigma, int begyear, int begtime,
                         int nobs, int L, int freq, double boxlam, char *x11out,
                         double refactor);

/* Informe de prevision LaTeX/PDF "a la fuf": una pagina por serie con la tabla
   NIVEL/VARIACION/ERR y un grafico (historia + prevision + banda del 95%) en
   pgfplots (autocontenido, sin gnuplot).  Compila con pdflatex si esta.
   Recibe los tensores ya calculados por transfer_forecast.                       */
static void forecast_latex_doc(int m, int L, real ***LP, real **sigma,
                               real **BC, real **DET, real **LVL,
                               real **aresid, int *topo)
{
    char fname[600];
    FILE *tex;
    int  u, i, l, t, per, yr;

    if (forecast_base == NULL) return;
    snprintf(fname, sizeof fname, "%s_forecast.tex", forecast_base);
    tex = fopen(fname, "w");
    if (tex == NULL) return;

    fprintf(tex,
        "\\documentclass[11pt,a4paper]{article}\n"
        "\\usepackage[T1]{fontenc}\n\\usepackage[utf8]{inputenc}\n"
        "\\usepackage{geometry}\n\\usepackage{graphicx}\n"
        "\\usepackage[table]{xcolor}\n"
        "\\geometry{margin=1.0cm}\n\\pagestyle{empty}\n"
        "\\setlength{\\parindent}{0pt}\n"
        "\\renewcommand{\\arraystretch}{0.75}\\setlength{\\tabcolsep}{3.5pt}\n"
        "\\begin{document}\n");

    for (u = 1; u <= m; u++) {
        int  nb, ord, freq;
        real refc, lam, vscale;
        real *ystar;
        int  hasfig = 0;
        char pdf[720] = {0};
        i    = topo[u];
        nb   = Ts[i].nobs;
        ord  = Tm[i].ornsop;
        freq = Ts[i].freq;
        refc = Ts[i].refactor;
        lam  = Tm[i].boxlam;
        vscale = (fabs(lam) < 1e-8) ? 100.0 / refc : 1.0 / refc;

        ystar = vector(1, nb + L);
        for (t = 1; t <= nb + L; t++) ystar[t] = BC[i][t] + DET[i][t];

        /* --- Grafico PRIMERO (modulo de fuf, forecast_graphic): asi sabemos si
           hay figura y maquetamos tabla|grafico lado a lado. Dibuja la variacion
           anual (historia + prevision +/- 1 DT) y los residuos (ERR) via gnuplot_i,
           tal cual fuf; solo con estacionalidad (freq>1) y si hay gnuplot.        */
        if (freq > 1 && system("command -v gnuplot >/dev/null 2>&1") == 0) {
            double *res1[2];               /* res[1][...] = residuos de la serie i */
            double **f3w = matrix(1, 1, 1, L);
            double ***v3w = tensor(1, L, 1, 1, 1, 1);
            char x11[560], eps[720], cmd[1600];
            int  aper, asub, ll;
            real s2 = 0.0; int cc = 0;
            res1[1] = aresid[i];
            for (ll = 1; ll <= L; ll++) {
                f3w[1][ll]    = (nb + ll - freq >= 1) ? ystar[nb+ll] - ystar[nb+ll-freq] : 0.0;
                v3w[ll][1][1] = vcov_diff_at(LP, sigma, m, i, ll, freq);
            }
            /* El panel ERR fija sus tics en 2*sqrt(sigma2), con los residuos pintados
               como vscale*a.  sigma2 = varianza MUESTRAL de esos residuos pintados. */
            for (t = 1; t <= n_stat; t++) { real z = vscale * aresid[i][t]; s2 += z * z; cc++; }
            if (cc > 0) s2 /= cc;
            snprintf(x11, sizeof x11, "%s_s%d", forecast_base, i);
            forecast_graphic(ystar, res1, f3w, v3w, ord, s2,
                             Ts[i].begyear, Ts[i].begtime, nb, L, freq, x11, refc);
            free_tensor(v3w, 1, L, 1, 1, 1, 1);
            free_matrix(f3w, 1, 1, 1, L);
            ObsToDate(Ts[i].begyear, Ts[i].begtime, nb + 1, freq, &aper, &asub);
            snprintf(eps, sizeof eps, "prev%s.%d%d.eps", x11, asub, aper);
            snprintf(pdf, sizeof pdf, "prev%s.%d%d.pdf", x11, asub, aper);
            snprintf(cmd, sizeof cmd, "epstopdf '%s' >/dev/null 2>&1", eps);
            if (system(cmd) == 0) {
                /* recorta el margen en blanco del canvas de gnuplot: contenido
                   pegado arriba, alinea con la cabecera de la tabla. */
                char crop[740];
                snprintf(crop, sizeof crop, "prev%s.%d%d-crop.pdf", x11, asub, aper);
                snprintf(cmd, sizeof cmd,
                         "command -v pdfcrop >/dev/null 2>&1 && "
                         "pdfcrop --margins 2 '%s' '%s' >/dev/null 2>&1", pdf, crop);
                if (system(cmd) == 0) snprintf(pdf, sizeof pdf, "%s", crop);
                hasfig = 1;
            }
            snprintf(cmd, sizeof cmd, "rm -f '%s'", eps);
            if (system(cmd) != 0) { /* ignore */ }
        }

        /* --- Titulo del informe: formato EXACTO de fuf (usfo.c:863-873):
           nombre, "Series brief description", Base/Unit / Data Source / Forecast
           Origin, centrado.  Igual que fuf para que convivan en la publicacion. */
        obs_to_date(Ts[i].begyear, Ts[i].begtime, freq, nb, &per, &yr);
        { char enm[128];
          fprintf(tex, "\\begin{center}\n\\textbf{%s} \\\\ \n"
                       "\\textbf{Series brief description} \\\\ \n"
                       "Base/Unit: \\space \\space \\space Data Source: \\space \\space \\space ",
                  latex_escape(enm, sizeof enm, Ts[i].name)); }
        if (freq > 1) fprintf(tex, "Forecast Origin: %d/%d \\\\ \n\\end{center}\n\n", per, yr);
        else          fprintf(tex, "Forecast Origin: %d \\\\ \n\\end{center}\n\n", yr);

        /* --- Maqueta a la fuf: tabla (izq.) | grafico (der.), lado a lado ------- */
        /* Maqueta tabla | grafico (proporciones de fuf: ~equilibrados).
           \vspace*{0pt} en ambas minipages -> alineadas por el borde SUPERIOR.   */
        if (hasfig)
            fprintf(tex, "\\noindent\\begin{minipage}[t]{0.54\\textwidth}\\vspace*{0pt}\\small\n");
        else
            fprintf(tex, "\\begin{center}\\small\n");

        /* ---- Tabla: maqueta EXACTA de fuf (usfo.c forecast_table_latex): ------
           cabecera en cajones (|c| + \hline + \cline), celdas $\mathsf{}$, filas
           de prevision sombreadas, y la fila en blanco de fuf entre los L/2 meses
           y los fines de año.                                                     */
        { const char *pcol = (freq == 12) ? "MONT" : (freq == 4) ? "QUART" : "PER";
        fprintf(tex, "\\begin{tabular}{rccrcrcr}\n\\hline\n");
        fprintf(tex, "\\multicolumn{1}{|c|}{} & \\multicolumn{2}{c|}{} & \\multicolumn{4}{c}{} & \\multicolumn{1}{|c|}{}\\vspace{-.10in}\\\\\n");
        fprintf(tex, "\\multicolumn{1}{|c|}{} & \\multicolumn{2}{c|}{LEVEL} & \\multicolumn{4}{c}{LOG RATE OF CHANGE} & \\multicolumn{1}{|c|}{}\\\\\n");
        fprintf(tex, "\\multicolumn{1}{|c|}{} & \\multicolumn{2}{c|}{} & \\multicolumn{4}{c}{} & \\multicolumn{1}{|c|}{}\\vspace{-.10in}\\\\ \\cline{2-7}\n");
        fprintf(tex, "\\multicolumn{1}{|c|}{} & \\multicolumn{1}{c|}{} & \\multicolumn{1}{c|}{} & \\multicolumn{1}{c|}{} & \\multicolumn{1}{c|}{} & \\multicolumn{1}{c|}{} & \\multicolumn{1}{c|}{} & \\multicolumn{1}{c|}{}\\vspace{-.05in}\\\\\n");
        fprintf(tex, "\\multicolumn{1}{|c|}{DATE} & \\multicolumn{1}{c|}{VALUE} & \\multicolumn{1}{c|}{Std} & \\multicolumn{1}{c|}{%s} & \\multicolumn{1}{c|}{Std} & \\multicolumn{1}{c|}{ANUAL} & \\multicolumn{1}{c|}{Std} & \\multicolumn{1}{c|}{ERR}\\\\\n", pcol);
        fprintf(tex, "\\multicolumn{1}{|c|}{} & \\multicolumn{1}{c|}{} & \\multicolumn{1}{c|}{($\\%%$)} & \\multicolumn{1}{c|}{($\\%%$)} & \\multicolumn{1}{c|}{($\\%%$)} & \\multicolumn{1}{c|}{($\\%%$)} & \\multicolumn{1}{c|}{($\\%%$)} & \\multicolumn{1}{c|}{($\\%%$)}\\\\\n");
        fprintf(tex, "\\hline \\\\\n");
        }
        /* Historia: L/2 antes del origen + el origen (convencion fuf). */
        for (t = nb - L/2; t <= nb; t++) {
            if (t < 1) continue;
            obs_to_date(Ts[i].begyear, Ts[i].begtime, freq, t, &per, &yr);
            fprintf(tex, "$\\mathsf{%d/%d}$ & $\\mathsf{%.2f}$ & - & ", per, yr, Ts[i].data[t]);
            if (t - 1 >= 1)    fprintf(tex, "$\\mathsf{%.2f}$ & - & ", vscale * (ystar[t] - ystar[t-1]));
            else               fprintf(tex, "- & - & ");
            if (t - freq >= 1) fprintf(tex, "$\\mathsf{%.2f}$ & - & ", vscale * (ystar[t] - ystar[t-freq]));
            else               fprintf(tex, "- & - & ");
            if (t - ord >= 1 && t - ord <= n_stat)
                 fprintf(tex, "$\\mathsf{%.2f}$ \\\\\n", vscale * aresid[i][t - ord]);
            else fprintf(tex, "- \\\\\n");
        }
        /* Prevision (convencion fuf): los primeros L/2 meses todos, la fila en
           blanco de fuf, y del resto SOLO los fines de año (per==freq).           */
        for (l = 1; l <= L; l++) {
            real sd1, sd2, sd3, f2, f3;
            obs_to_date(Ts[i].begyear, Ts[i].begtime, freq, nb + l, &per, &yr);
            if (l == L/2 + 1) fprintf(tex, " \\\\\n");          /* separador de fuf */
            if (l > L/2 && per != freq) continue;
            sd1 = sqrt(vcov_diff_at(LP, sigma, m, i, l, 0));
            sd2 = sqrt(vcov_diff_at(LP, sigma, m, i, l, 1));
            sd3 = (freq > 1) ? sqrt(vcov_diff_at(LP, sigma, m, i, l, freq)) : 0.0;
            f2  = ystar[nb + l] - ystar[nb + l - 1];
            f3  = (nb + l - freq >= 1) ? ystar[nb + l] - ystar[nb + l - freq] : 0.0;
            fprintf(tex, "\\rowcolor{black!7}");
            fprintf(tex, "$\\mathsf{%d/%d}$ & $\\mathsf{%.2f}$ & $\\mathsf{%.2f}$ & $\\mathsf{%.2f}$ & $\\mathsf{%.2f}$ & ",
                    per, yr, LVL[i][l], vscale * sd1, vscale * f2, vscale * sd2);
            if (freq > 1) fprintf(tex, "$\\mathsf{%.2f}$ & $\\mathsf{%.2f}$ & - \\\\\n", vscale * f3, vscale * sd3);
            else          fprintf(tex, "- & - & - \\\\\n");
        }
        fprintf(tex, "\\end{tabular}\n");

        if (hasfig) {
            /* grafico pegado a la tabla, altura ~igual a la tabla (equilibrado
               como fuf); keepaspectratio mantiene la forma del grafico. */
            fprintf(tex, "\\end{minipage}\\hfill\n"
                         "\\begin{minipage}[t]{0.44\\textwidth}\\vspace*{0pt}\\centering\n");
            fprintf(tex, "\\includegraphics[width=\\linewidth,height=10cm,"
                         "keepaspectratio]{%s}\n", pdf);
            fprintf(tex, "\\end{minipage}\n\n");
        } else {
            fprintf(tex, "\\end{center}\n\n");
        }

        /* Dos informes por cara: separador tras el impar, salto de pagina tras el par. */
        if (u % 2 == 0 && u < m)
            fprintf(tex, "\\clearpage\n\n");
        else if (u < m)
            fprintf(tex, "\\vspace{0.5em}\\hrule\\vspace{0.5em}\n\n");

        free_vector(ystar, 1, nb + L);
    }
    fprintf(tex, "\\end{document}\n");
    fclose(tex);

    /* compilar con pdflatex si esta disponible (dos pasadas por fill between) */
    if (system("command -v pdflatex >/dev/null 2>&1") == 0) {
        char cmd[1400];
        snprintf(cmd, sizeof cmd,
                 "pdflatex -interaction=batchmode -halt-on-error %s >/dev/null 2>&1 && "
                 "pdflatex -interaction=batchmode -halt-on-error %s >/dev/null 2>&1",
                 fname, fname);
        if (system(cmd) == 0) {
            /* limpiar auxiliares y los PDF de los graficos (ya embebidos) */
            char aux[1400];
            snprintf(aux, sizeof aux,
                     "rm -f %s_forecast.aux %s_forecast.log prev%s_s*.pdf",
                     forecast_base, forecast_base, forecast_base);
            if (system(aux) != 0) { /* ignore */ }
        }
    }
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

    /* La covarianza de las innovaciones. Con el cast EMPOTRADO se usa la
       ESTRUCTURAL, no la reducida: las varianzas de prevision salen IGUALES
       (Psi_red Sigma_red Psi_red' = Psi_str Q Psi_str'), pero solo con la
       estructural las innovaciones son ORTOGONALES -- y solo entonces la
       descomposicion de la varianza es UNICA.                                  */
    sigma = matrix(1, m, 1, m);
    for (i = 1; i <= m; i++)
        for (j = 1; j <= m; j++)
            sigma[i][j] = sigma2 * ((embed_varma && !phi0_is_identity)
                                    ? qq_struct[i][j] : vf.qq[i][j]);
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
    /* Con el cast EMPOTRADO la transferencia YA ESTA DENTRO del VARMA: f1[i][l] es
       directamente la prevision de la serie OBSERVADA w_i. Volver a sumarle la
       transferencia la contaria DOS VECES -- y lo hacia: inflaba la sd un 40%.    */
    we = matrix(1, m, 1, n_stat + L);
    for (u = 1; u <= m; u++) {
        i = topo[u];
        for (t = 1; t <= n_stat; t++) we[i][t] = w[i][t];
        for (l = 1; l <= L; l++) {
            real acc = f1[i][l];              /* f1 ya lleva mu_i */
            int  tt  = n_stat + l;
            if (!embed_varma)
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
    if (embed_varma) {
        /* El VARMA ya ES el sistema: sus psi son los del sistema. Pero son los de la
           forma REDUCIDA: w = Psi_red(B) u, con u = Phi(0)^-1 a. Luego los psi
           ESTRUCTURALES son Psi_str = Psi_red . Phi(0)^-1, y con ellos la
           descomposicion de la varianza vuelve a ser unica.                     */
        for (i = 1; i <= m; i++)
            for (j = 1; j <= m; j++)
                for (t = 0; t <= L; t++) {
                    if (phi0_is_identity) { pt[i][j][t] = psi[t][i][j]; continue; }
                    {
                        real acc = 0.0;
                        for (k = 1; k <= m; k++)
                            acc += psi[t][i][k] * phi0_inv[k][j];
                        pt[i][j][t] = acc;
                    }
                }
    } else
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

    /* --- REPORTE DE PREVISION "a la fuf/forsil" ---------------------------
       Una tabla por serie con NIVEL (valor, DT), VARIACION de periodo (1-B) y
       ANUAL (1-B^s) con sus DT, y el residuo (ERR).  Homologable con el
       forecast_table_acii de fuf (univariante) y con forsil (multivariante):
       misma tabla.  Las filas observadas llevan la variacion real y el residuo;
       las de prevision, el valor previsto y su desviacion tipica.
       Metrica: en modelos LOG la variacion va en % (x100/refactor, como fuf); en
       niveles, como diferencia directa (/refactor, como forsil).                */
    for (u = 1; u <= m; u++) {
        int    nb, ord, freq, per, yr, obs;
        real   refc, lam, vscale;
        real  *ystar;                 /* nivel TRANSFORMADO y* = BC + DET */
        i = topo[u];
        nb   = Ts[i].nobs;
        ord  = Tm[i].ornsop;
        freq = Ts[i].freq;
        refc = Ts[i].refactor;
        lam  = Tm[i].boxlam;
        vscale = (fabs(lam) < 1e-8) ? 100.0 / refc : 1.0 / refc;

        ystar = vector(1, nb + L);
        for (t = 1; t <= nb + L; t++) ystar[t] = BC[i][t] + DET[i][t];

        obs_to_date(Ts[i].begyear, Ts[i].begtime, freq, nb, &per, &yr);
        fprintf(out, "\n  FORECAST REPORT\n");
        fprintf(out, "    VARIABLE NAME  : %s\n", Ts[i].name ? Ts[i].name : "");
        if (freq > 1) fprintf(out, "    FORECAST ORIGIN: %2d/%-4d", per, yr);
        else          fprintf(out, "    FORECAST ORIGIN: %-4d", yr);
        fprintf(out, "    LEAD TIME: %d\n\n", L);
        fprintf(out, "   +--------------------------------------------------------------------+\n");
        fprintf(out, "   |         |      LEVEL       |           VARIATION            |       |\n");
        fprintf(out, "   |  DATE   +-----------------+-------------------------------+  ERR  |\n");
        fprintf(out, "   |         |   VALUE  |  STD |  PERIOD |  STD  | ANNUAL |  STD |       |\n");
        fprintf(out, "   +--------------------------------------------------------------------+\n");

        /* Filas OBSERVADAS: las ultimas L+1 (origen incluido). */
        for (obs = nb - L; obs <= nb; obs++) {
            if (obs < 1) continue;
            obs_to_date(Ts[i].begyear, Ts[i].begtime, freq, obs, &per, &yr);
            if (freq > 1) fprintf(out, "   %2d/%-4d ", per, yr);
            else          fprintf(out, "   %-6d ", yr);
            fprintf(out, "%9.2f    -   ", Ts[i].data[obs]);
            if (obs - 1 >= 1) fprintf(out, "%8.2f    -   ", vscale * (ystar[obs] - ystar[obs-1]));
            else              fprintf(out, "    -        -   ");
            if (obs - freq >= 1) fprintf(out, "%7.2f    -  ", vscale * (ystar[obs] - ystar[obs-freq]));
            else                 fprintf(out, "   -        -  ");
            if (obs - ord >= 1 && obs - ord <= n_stat)
                /* a esta indexada [tiempo][serie], como en todo el resto del
                   fichero.  Aqui estaba al reves -- a[i][obs-ord] --, asi que
                   la columna ERR leia de OTRA posicion del bloque contiguo de
                   residuos: valores que parecen residuos porque LO SON, pero de
                   la observacion equivocada.  Por eso nunca canto.             */
                fprintf(out, "%7.2f\n", vscale * vf.a[obs - ord][i]);
            else
                fprintf(out, "    -  \n");
        }
        /* Filas de PREVISION. */
        for (l = 1; l <= L; l++) {
            real sd1 = sqrt(vcov_diff_at(LP, sigma, m, i, l, 0));
            real sd2 = sqrt(vcov_diff_at(LP, sigma, m, i, l, 1));
            real sd3 = (freq > 1) ? sqrt(vcov_diff_at(LP, sigma, m, i, l, freq)) : 0.0;
            real f2  = ystar[nb + l] - ystar[nb + l - 1];
            real f3  = (nb + l - freq >= 1) ? ystar[nb + l] - ystar[nb + l - freq] : 0.0;
            obs_to_date(Ts[i].begyear, Ts[i].begtime, freq, nb + l, &per, &yr);
            if (freq > 1) fprintf(out, "   %2d/%-4d ", per, yr);
            else          fprintf(out, "   %-6d ", yr);
            fprintf(out, "%9.2f %6.2f ", LVL[i][l], vscale * sd1);
            fprintf(out, "%8.2f %6.2f ", vscale * f2, vscale * sd2);
            if (freq > 1) fprintf(out, "%7.2f %6.2f", vscale * f3, vscale * sd3);
            else          fprintf(out, "    -       - ");
            fprintf(out, "    -  \n");
        }
        fprintf(out, "   +--------------------------------------------------------------------+\n");
        free_vector(ystar, 1, nb + L);
    }

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

    /* --- Informe LaTeX/PDF (-L), con los mismos tensores ------------------- */
    if (latex_forecast)
        forecast_latex_doc(m, L, LP, sigma, BC, DET, LVL, vf.a, topo);

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

/* series_index, read_network y topo_sort viven en lib/netfile: el GUI tiene que
   leer y validar EXACTAMENTE el mismo .dag, y la unica forma de garantizarlo es
   que sea el mismo codigo. Lo que queda aqui es el trasvase de los globales.  */

static void net_names(const char **nombre)
{
    int i;
    for (i = 1; i <= n_ser; i++) nombre[i] = Ts[i].name;
}

/* Lo usa tambien el lector de agregados. */
static int series_index(const char *tok)
{
    const char *nombre[MAX_SER + 1];

    net_names(nombre);
    return net_series_index(nombre, n_ser, tok);
}

static int read_network(const char *path)
{
    const char *nombre[MAX_SER + 1];
    NetLink     tmp[MAX_LINK];
    NetError    e;
    char        why[512];
    int         n, k;

    net_names(nombre);
    n = net_read(path, nombre, n_ser, tmp,
                 MAX_LINK < NET_MAX_LINK ? MAX_LINK : NET_MAX_LINK, &e);
    if (n < 0) {
        fprintf(stderr, "Error: %s\n", net_error_en(&e, why, sizeof why));
        return -1;
    }

    n_link = n;
    for (k = 0; k < n; k++) {
        lnk[k + 1].out = tmp[k].out;
        lnk[k + 1].inp = tmp[k].inp;
        lnk[k + 1].b   = tmp[k].b;
        lnk[k + 1].r   = tmp[k].r;
        lnk[k + 1].s   = tmp[k].s;
    }
    return n;
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
   puede resolver restando transferencias: hay que decirlo, no estimar basura.
   La cuenta esta en lib/netfile (net_topo); aqui solo el trasvase.         */
static int topo_sort(void)
{
    NetLink tmp[MAX_LINK];
    int     k;

    for (k = 1; k <= n_link; k++) {
        tmp[k - 1].out = lnk[k].out;
        tmp[k - 1].inp = lnk[k].inp;
        tmp[k - 1].b = lnk[k].b; tmp[k - 1].r = lnk[k].r; tmp[k - 1].s = lnk[k].s;
    }

    if (net_topo(tmp, n_link, n_ser, topo)) return 1;

    fprintf(stderr, "Error: the transfer network has a CYCLE: the system is\n"
                    "       simultaneous and cannot be cast as a triangular\n"
                    "       VARMA by subtracting transfers.\n");
    return 0;
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
"             omega1[1] = omega1[0] * theta_2[B^1]   # PRODUCT: a factorized\n"
"                                      # numerator w0*(1 - x*B) with x shared with\n"
"                                      # the input's own MA (Mauricio's m6 form)\n"
"             omega3[0] = omega3[1] + omega3[2] + omega3[3]  # LINEAR COMB: a\n"
"                                      # fixed (1-B) factor forces nu_num(1)=0;\n"
"                                      # terms may be slots or products (y*z - w)\n"
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
"FIXED-WINDOW ESTIMATION  (-estwin: real-time SPS + out-of-sample evaluation)\n"
"  -estwin E   estimate ONCE on observations 1..E and hold the parameters FIXED.\n"
"           (-R E is a hidden alias, kept for compatibility.)  Needs -f H.\n"
"           The ESTIMATION WINDOW (E) and the FORECAST ORIGIN (-O, below) are two\n"
"           independent choices: params come from 1..E; the forecast starts wherever\n"
"           -O says. Re-estimating the SAME fixed window is deterministic and cheap.\n"
"  -O g     FORECAST ORIGIN for the -f/-L report (default: the window end E, or the\n"
"           data end when there is no -estwin). Decoupled from the window:\n"
"             -O g   forecast from observation g -- e.g. -estwin 216 -O 216 forecasts\n"
"                    the out-of-sample hold-out from the end of the training window\n"
"                    (compare two models at a chosen origin by running each with the\n"
"                    same -O);\n"
"             -O -1  the CURRENT end of the data -- the real-time SPS: append a new\n"
"                    datum to the input .pre and it becomes the new origin, a fresh\n"
"                    report with NO drifting parameters (fuf split estimate/forecast\n"
"                    into two programs because compute was costly; not needed here).\n"
"           Add -L for the LaTeX/PDF report.\n"
"\n"
"           OUT-OF-SAMPLE EVALUATION.  With -C the origin ALSO rolls forward one\n"
"           datum at a time over E..n-H (balanced horizons), comparing each forecast\n"
"           with what actually happened: MAE, RMSE and MAPE by horizon. The\n"
"           variances the model reports are THEORETICAL. This is the only way to\n"
"           decide EMPIRICALLY whether one model forecasts better than another --\n"
"           run it on two specifications and compare.\n"
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
"  -V       EMBED the transfer in the VARMA.  THIS IS THE DEFAULT.\n"
"  -S       SUBTRACT the transfer instead (the old cast).\n"
"\n"
"           The subtracting cast builds the noise OUTSIDE the likelihood engine,\n"
"           N_t = w_Y,t - SUM_k nu_k w_X,{t-k}, which at t=1 needs input values\n"
"           that DO NOT EXIST. It sets them to zero. The engine cannot fix this,\n"
"           because it never sees those inputs: it is handed the noise already\n"
"           contaminated. The likelihood it then computes is exact -- for the\n"
"           WRONG series.\n"
"\n"
"           Embedded, nothing is subtracted. The transfer becomes OFF-DIAGONAL\n"
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
"           With a CONTEMPORANEOUS transfer (b=0) the embedded cast puts omega_0\n"
"           at lag zero, so Phi(0) != I and the innovations of the OBSERVED series\n"
"           come out correlated -- by construction, not by misspecification. The\n"
"           report gives BOTH covariances: the reduced-form one and the structural\n"
"           one, which is the one the model assumes orthogonal.\n"
"\n"
"IDENTIFICATION\n"
"  -p       PREWHITEN ONLY: filter the input with its own ARMA, apply the same\n"
"           filter to the output, plot the CCF and suggest (b, r, s).\n"
"           Does NOT estimate and does NOT iterate.\n"
"  -i       IDENTIFY THE NETWORK from the diagonal model. After estimating the\n"
"           diagonal (-0), read the residual CCFs and propose the transfer DAG\n"
"           (directed links + b/r/s) and the contemporaneous covariances. It is\n"
"           a GUIDE: prune by exogeneity, acyclicity and lag plausibility. This\n"
"           is the multivariate counterpart of -p (Munoz Polo 2001, paso 3).\n"
"  -g NAME  GUIDED driver of the ladder. Like -i, but also WRITES NAME.dag and\n"
"           NAME.cns (ready for -n/-c, covariances with numeric q[i,j]) and\n"
"           prints the plan with the next command. Review/prune the draft, then\n"
"           estimate:  drtran <.pre ...> -n NAME.dag -c NAME.cns\n"
"\n"
"TRANSFER FUNCTION  (one per input)\n"
"           nu(B) = omega(B)/delta(B) * B^b, in the Box-Jenkins convention:\n"
"             omega(B) = omega_0 - omega_1 B - ... - omega_s B^s\n"
"             delta(B) = 1     - delta_1 B - ... - delta_r B^r\n"
"           (the leading omega adds, the rest SUBTRACT, as in fue's calcnu; the\n"
"           impulse response nu is the same physical object either way).\n"
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
"           input innovation, the latter propagated through nu(B).  Emits, per\n"
"           series, a report with the LEVEL and its period/annual VARIATION,\n"
"           each with a standard deviation (as fuf/forsil present them).\n"
"  -L       also write a LaTeX/PDF forecast report (<name>_forecast.tex, one\n"
"           page per series: the table plus fuf's forecast chart (annual rate of\n"
"           change with +/-1 SD band, and the residuals). Needs gnuplot+epstopdf\n"
"           for the chart and pdflatex for the PDF; degrades gracefully without.\n"
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
#define MAX_SLOT    400
#define SLOT_FREE    0
#define SLOT_FIXED   1
#define SLOT_ALIAS   2
#define SLOT_PRODUCT 3   /* x = y * z : el coeficiente ES un producto de otros dos */
#define SLOT_LINCOMB 4   /* x = [±]t1 [±]t2 ... : suma de terminos (ti = slot o slot*slot) */
#define MAX_LC_TERMS 6   /* terminos por combinacion lineal (basta para m6)              */

static char slot_name[MAX_SLOT + 1][40];
static int  slot_kind[MAX_SLOT + 1];
static int  slot_alias[MAX_SLOT + 1];
static real slot_value[MAX_SLOT + 1];
static int  slot_pa[MAX_SLOT + 1], slot_pb[MAX_SLOT + 1];  /* operandos del PRODUCTO */
static int  slot_nlc[MAX_SLOT + 1];                        /* nº de terminos (LINCOMB) */
static real slot_lc_sign[MAX_SLOT + 1][MAX_LC_TERMS];      /* +1/-1 por termino        */
static int  slot_lc_a[MAX_SLOT + 1][MAX_LC_TERMS];         /* factor 1 de cada termino */
static int  slot_lc_b[MAX_SLOT + 1][MAX_LC_TERMS];         /* factor 2 (0 = sin producto) */
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
    slot_pa[n_slot]    = 0;
    slot_pb[n_slot]    = 0;
    slot_nlc[n_slot]   = 0;
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
        char lhs[64], rhs[64], rhsfull[128];
        char *hash = strchr(line, '#');
        char *star;
        int a, b;
        double v;

        if (hash) *hash = '\0';
        if (sscanf(line, " %63[^= \t] = %127[^\n]", lhs, rhsfull) != 2) continue;

        a = find_slot(lhs);
        if (a == 0) {
            fprintf(stderr, "Error: unknown parameter '%s' in %s\n", lhs, path);
            fclose(f);
            return -1;
        }

        /* COMBINACION LINEAL:  x = [±]t1 [±]t2 ...  con ti = slot o slot*slot.
           Generaliza el PRODUCTO a sumas/diferencias de terminos.  Cubre el factor
           FIJO (1−B) de una FLT — que impone nu_num(1)=0, i.e. omega[0]=omega[1]+
           omega[2]+... — y los coeficientes producto±termino de un numerador
           factorizado (p.ej. x12*x14 − x13 del legacy).  Se detecta por un
           separador +/- interno (los nombres de slot no llevan +/-).  El gradiente
           lo capta cdgrad por diferencias finitas, como el producto.              */
        {
            char *q = rhsfull;
            int   is_lc = 0;
            while (*q == ' ' || *q == '\t') q++;
            if (*q == '+' || *q == '-') q++;          /* signo inicial: no separa */
            for (; *q; q++) if (*q == '+' || *q == '-') { is_lc = 1; break; }
            if (is_lc) {
                char *p = rhsfull;
                real  sg = 1.0;
                int   nt = 0, ok = 1;
                while (*p) {
                    char tok[80], f1[64], f2[64], *st, *ast;
                    while (*p == ' ' || *p == '\t') p++;
                    if (*p == '+') { sg =  1.0; p++; continue; }
                    if (*p == '-') { sg = -1.0; p++; continue; }
                    if (!*p) break;
                    st = tok;                          /* leer termino hasta +/- o fin */
                    while (*p && *p != '+' && *p != '-' &&
                           (size_t)(st - tok) < sizeof tok - 1) *st++ = *p++;
                    *st = '\0';
                    if (nt >= MAX_LC_TERMS) { ok = 0; break; }
                    ast = strchr(tok, '*');
                    if (ast) {
                        int s1, s2;
                        *ast = '\0';
                        if (sscanf(tok, " %63s", f1) != 1 ||
                            sscanf(ast + 1, " %63s", f2) != 1) { ok = 0; break; }
                        s1 = find_slot(f1); s2 = find_slot(f2);
                        if (!s1 || !s2 || s1 == a || s2 == a) { ok = 0; break; }
                        slot_lc_sign[a][nt] = sg;
                        slot_lc_a[a][nt] = s1; slot_lc_b[a][nt] = s2; nt++;
                    } else {
                        int s1;
                        if (sscanf(tok, " %63s", f1) != 1) { ok = 0; break; }
                        s1 = find_slot(f1);
                        if (!s1 || s1 == a) { ok = 0; break; }
                        slot_lc_sign[a][nt] = sg;
                        slot_lc_a[a][nt] = s1; slot_lc_b[a][nt] = 0; nt++;
                    }
                }
                if (!ok || nt < 1) {
                    fprintf(stderr,
                        "Error: cannot parse linear combination '%s = %s' in %s\n",
                        lhs, rhsfull, path);
                    fclose(f); return -1;
                }
                slot_kind[a] = SLOT_LINCOMB;
                slot_nlc[a]  = nt;
                nc++;
                continue;
            }
        }

        /* PRODUCTO:  x = [-] y * z.  El coeficiente ES el producto de otros dos
           slots, con un signo opcional (numerador factorizado del legacy: p.ej.
           omega1[1] = -omega1[0] * theta_4 reproduce -x5*(1-x6B) con x6 compartido
           con la MA del input).  El gradiente lo maneja cdgrad por diferencias
           finitas: no hace falta regla de la cadena analitica.                    */
        star = strchr(rhsfull, '*');
        if (star) {
            char pa[64], pb[64];
            char *p = rhsfull;
            real sign = 1.0;
            *star = '\0';
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '-') { sign = -1.0; p++; while (*p == ' ' || *p == '\t') p++; }
            if (sscanf(p, "%63s", pa) == 1 && sscanf(star + 1, " %63s", pb) == 1) {
                int sa = find_slot(pa), sb = find_slot(pb);
                if (sa == 0 || sb == 0) {
                    fprintf(stderr, "Error: unknown operand in product '%s = %s * %s' in %s\n",
                            lhs, pa, pb, path);
                    fclose(f); return -1;
                }
                if (sa == a || sb == a) {
                    fprintf(stderr, "Error: '%s' cannot be a factor of itself\n", lhs);
                    fclose(f); return -1;
                }
                slot_kind[a]  = SLOT_PRODUCT;
                slot_pa[a] = sa;  slot_pb[a] = sb;
                slot_value[a] = sign;         /* +1 o -1: el signo del producto */
                nc++;
                continue;
            }
        }

        if (sscanf(rhsfull, " %63s", rhs) != 1) continue;

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
            /* omega(B) = w0 - w1 B - ... (BJR): el lider suma, los demas restan */
            real sk = (k == 0) ? 1.0 : -1.0;
            w1 += sk * xf[slot + k];
            wk += k * sk * xf[slot + k];
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
            add_grad(gg, slot + k, ((k == 0) ? 1.0 : -1.0) / d1);  /* d g / d omega_k (BJR) */
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
                add_grad(gg, slot + k, ((k == 0) ? 1.0 : -1.0) * (k - A) / w1);
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
        else                                 xfull[i] = 0.0;   /* alias/producto: abajo */
    }
    for (i = 1; i <= n_slot; i++)
        if (slot_kind[i] == SLOT_ALIAS) xfull[i] = xfull[slot_alias[i]];
    /* PRODUCTOS:  x = xfull[pa] * xfull[pb].  Se itera para resolver cadenas
       (producto de producto); n_slot pasadas bastan.  El gradiente lo capta
       cdgrad por diferencias finitas sobre los libres — sin regla de cadena. */
    {
        int pass, changed = 1;
        for (pass = 0; changed && pass < n_slot; pass++) {
            changed = 0;
            for (i = 1; i <= n_slot; i++) {
                if (slot_kind[i] == SLOT_PRODUCT) {
                    real nv = slot_value[i] * xfull[slot_pa[i]] * xfull[slot_pb[i]];
                    if (nv != xfull[i]) { xfull[i] = nv; changed = 1; }
                } else if (slot_kind[i] == SLOT_LINCOMB) {
                    real nv = 0.0; int t;
                    for (t = 0; t < slot_nlc[i]; t++) {
                        real term = slot_lc_sign[i][t] * xfull[slot_lc_a[i][t]];
                        if (slot_lc_b[i][t]) term *= xfull[slot_lc_b[i][t]];
                        nv += term;
                    }
                    if (nv != xfull[i]) { xfull[i] = nv; changed = 1; }
                }
            }
        }
    }
    return xfull;
}

/* -------------------------------------------------------------------------- */
/* identify_network — el "-p del sistema entero".  Lee las CCF de los residuos  */
/* del modelo DIAGONAL (cada serie preblanqueada por su PROPIO modelo) y         */
/* PROPONE la red de transferencias + las covarianzas contemporaneas, tal como   */
/* manda la escuela (Munoz Polo 2001 §2.6: leer las ccf residuales del modelo    */
/* diagonal para descubrir las relaciones dinamicas del sistema).                */
/*                                                                              */
/* Convencion:  ccf_ij(k) = corr(a_i(t), a_j(t+k)),  con i<j.                    */
/*   |r| > 2/sqrt(n) es significativo.                                          */
/*     k>0  -> a_i antecede a a_j -> enlace  i -> j  (a_j RECIBE de a_i)         */
/*     k<0  -> a_j antecede a a_i -> enlace  j -> i                             */
/*     k=0  -> contemporaneo      -> liberar la covarianza  q[i,j]              */
/*     ambos lados significativos -> FEEDBACK: no cabe en un DAG de una via;    */
/*                                    se avisa y se toma la direccion dominante. */
/* Cada enlace dinamico propone (b, s): b = primer retardo significativo, s =    */
/* extension del bloque contiguo (omegas libres, r=0), igual que la M2 bivariante.*/
/* -------------------------------------------------------------------------- */
static int net_bs_from_side(real *c, int nlags, real thr, int *b, int *s,
                            real *peak)
{
    /* c[k+1] = ccf en el retardo k = 1..nlags de un lado.  Devuelve 1 si hay
       un bloque significativo, y fija b (primer k signif.) y s (fin - b).      */
    int k, b_hat = -1, last = -1;
    real pk = 0.0;
    for (k = 1; k <= nlags; k++) {
        if (fabs(c[k + 1]) > thr) {
            if (b_hat < 0) b_hat = k;
            last = k;
        } else if (b_hat >= 0 && k > last + 1) {
            break;   /* el bloque es el CONTIGUO desde b; un pico aislado lejano es ruido */
        }
        if (fabs(c[k + 1]) > fabs(pk)) pk = c[k + 1];
    }
    if (b_hat < 0) return 0;
    /* recortar last al bloque contiguo desde b_hat */
    last = b_hat;
    while (last + 1 <= nlags && fabs(c[last + 2]) > thr) last++;
    *b = b_hat; *s = last - b_hat; *peak = pk;
    return 1;
}

static void identify_network(real **a, int n, int m, FILE *out)
{
    int   i, j, t, k, nlags;
    real  thr = 2.0 / sqrt((real) n);
    int   lo[MAX_SER * MAX_SER + 1], li[MAX_SER * MAX_SER + 1];   /* out, in */
    int   lb[MAX_SER * MAX_SER + 1], lsv[MAX_SER * MAX_SER + 1];  /* b, s    */
    real  lpk[MAX_SER * MAX_SER + 1];
    int   nl = 0;
    int   qi[MAX_SER * MAX_SER + 1], qj[MAX_SER * MAX_SER + 1];   /* covarianzas */
    real  qr[MAX_SER * MAX_SER + 1];
    int   nq = 0, nfb = 0;
    real *ai, *aj, *cpos, *cneg;

    /* Ventana de busqueda: transferencias con retardo > ~2 ciclos estacionales
       son inverosimiles; ademas, cuantos mas retardos, mas falsos positivos por
       contraste multiple.  Se acota a 2*s (o 8 si no hay estacionalidad).       */
    nlags = n / 4;  if (nlags > 12) nlags = 12;  if (nlags < 6) nlags = 6;
    { int fq = Ts[1].freq; int cap = (fq > 1) ? 2 * fq : 8;
      if (nlags > cap) nlags = cap; }

    fprintf(out, "\n=============================================================\n");
    fprintf(out, "  NETWORK IDENTIFICATION  (residual CCF of the diagonal model)\n");
    fprintf(out, "=============================================================\n");
    fprintf(out, "  Munoz Polo (2001) §2.6: las relaciones dinamicas del sistema se\n");
    fprintf(out, "  leen en las ccf de los residuos del modelo DIAGONAL.  Esto es una\n");
    fprintf(out, "  GUIA de candidatos, no la red final: podar por exogeneidad (nada\n");
    fprintf(out, "  entra en una serie exogena), aciclicidad (el DAG no admite ciclos)\n");
    fprintf(out, "  y verosimilitud del retardo.  Busqueda hasta k=%d.\n\n", nlags);
    fprintf(out, "  residuals a_i.  ccf(k)=corr(a_i(t),a_j(t+k)); |r|>%.3f is\n", thr);
    fprintf(out, "  significant.  k=0 -> contemporaneous (free q[i,j]);\n");
    fprintf(out, "  k>0 -> i->j;  k<0 -> j->i;  both sides -> feedback.\n\n");

    ai = vector(1, n);  aj = vector(1, n);
    cpos = vector(1, nlags + 1);  cneg = vector(1, nlags + 1);

    for (i = 1; i < m; i++) {
        for (j = i + 1; j <= m; j++) {
            real mi, mj, si, sj, r0, pkp = 0.0, pkn = 0.0;
            int  bp, sp, bn, sn, haspos, hasneg;
            for (t = 1; t <= n; t++) { ai[t] = a[t][i]; aj[t] = a[t][j]; }
            mi = Mean(ai, n);  si = Stdev(ai, n);
            mj = Mean(aj, n);  sj = Stdev(aj, n);
            if (si < 1e-12 || sj < 1e-12) continue;

            /* cpos[k+1]=corr(a_i(t),a_j(t+k))  (k>=0, lado i->j)               */
            /* cneg[k+1]=corr(a_j(t),a_i(t+k)) = ccf_ij(-k)  (lado j->i)         */
            Ccf(ai, aj, n, nlags, cpos, mi, mj, si, sj);
            Ccf(aj, ai, n, nlags, cneg, mj, mi, sj, si);
            r0 = cpos[1];                         /* k = 0 */

            if (fabs(r0) > thr) { qi[++nq] = i; qj[nq] = j; qr[nq] = r0; }

            haspos = net_bs_from_side(cpos, nlags, thr, &bp, &sp, &pkp);
            hasneg = net_bs_from_side(cneg, nlags, thr, &bn, &sn, &pkn);

            if (haspos && hasneg) {           /* FEEDBACK: se toma el mas fuerte */
                nfb++;
                fprintf(out, "  [feedback]  %s <-> %s : i->j k=%d(%.2f), j->i k=%d(%.2f)"
                             "  -> se toma el dominante\n",
                        Ts[i].name ? Ts[i].name : "?", Ts[j].name ? Ts[j].name : "?",
                        bp, pkp, bn, pkn);
                if (fabs(pkp) >= fabs(pkn)) { hasneg = 0; } else { haspos = 0; }
            }
            if (haspos) { lo[++nl] = j; li[nl] = i; lb[nl] = bp; lsv[nl] = sp; lpk[nl] = pkp; }
            if (hasneg) { lo[++nl] = i; li[nl] = j; lb[nl] = bn; lsv[nl] = sn; lpk[nl] = pkn; }
        }
    }

    /* --- Covarianzas contemporaneas --- */
    fprintf(out, "  CONTEMPORANEOUS  (k=0; free the innovation covariance):\n");
    if (nq == 0) fprintf(out, "    (none above the band)\n");
    for (k = 1; k <= nq; k++)
        fprintf(out, "    %-4s - %-4s   r(0) = %+.3f\n",
                Ts[qi[k]].name ? Ts[qi[k]].name : "?",
                Ts[qj[k]].name ? Ts[qj[k]].name : "?", qr[k]);

    /* Ordenar los enlaces por |peak| descendente (los reales suelen ser los mas
       fuertes; los picos lejanos espurios caen al fondo). Insercion sobre los
       arrays paralelos.                                                         */
    for (i = 2; i <= nl; i++) {
        int oo = lo[i], ii = li[i], bb = lb[i], ss = lsv[i]; real pp = lpk[i];
        int q = i - 1;
        while (q >= 1 && fabs(lpk[q]) < fabs(pp)) {
            lo[q+1]=lo[q]; li[q+1]=li[q]; lb[q+1]=lb[q]; lsv[q+1]=lsv[q]; lpk[q+1]=lpk[q];
            q--;
        }
        lo[q+1]=oo; li[q+1]=ii; lb[q+1]=bb; lsv[q+1]=ss; lpk[q+1]=pp;
    }

    /* --- Enlaces dinamicos --- */
    fprintf(out, "\n  DIRECTED LINKS  (candidate transfers, strongest first):\n");
    if (nl == 0) fprintf(out, "    (none above the band)\n");
    for (k = 1; k <= nl; k++)
        fprintf(out, "    %-4s -> %-4s   peak %+.3f   proposal  b=%d r=0 s=%d\n",
                Ts[li[k]].name ? Ts[li[k]].name : "?",
                Ts[lo[k]].name ? Ts[lo[k]].name : "?", lpk[k], lb[k], lsv[k]);

    /* --- Bloques listos para copiar --- */
    if (nl > 0) {
        fprintf(out, "\n  SUGGESTED NETWORK  (paste into a -n file):\n");
        for (k = 1; k <= nl; k++)
            fprintf(out, "    %s <- %s   %d 0 %d\n",
                    Ts[lo[k]].name ? Ts[lo[k]].name : "?",
                    Ts[li[k]].name ? Ts[li[k]].name : "?", lb[k], lsv[k]);
    }
    if (nq > 0) {
        fprintf(out, "\n  SUGGESTED COVARIANCES  (paste into a -c file; positions by\n");
        fprintf(out, "  command-line order):\n");
        for (k = 1; k <= nq; k++)
            fprintf(out, "    q[%s,%s] = free\n",
                    Ts[qi[k]].name ? Ts[qi[k]].name : "?",
                    Ts[qj[k]].name ? Ts[qj[k]].name : "?");
    }
    if (nfb > 0)
        fprintf(out, "\n  NOTE: %d pair(s) show feedback (both directions). A one-way\n"
                     "  transfer DAG took the dominant side; inspect them by hand.\n", nfb);
    fprintf(out, "=============================================================\n");

    /* --- MODO GUIADO (-g): escribir los artefactos y emitir el plan --- */
    if (guide_name != NULL) {
        char fdag[600], fcns[600];
        FILE *fd, *fc;
        snprintf(fdag, sizeof fdag, "%s.dag", guide_name);
        snprintf(fcns, sizeof fcns, "%s.cns", guide_name);

        fd = fopen(fdag, "w");
        if (fd) {
            fprintf(fd, "# Red PROPUESTA por drtran -g (guia, no la red final).\n");
            fprintf(fd, "# Revisa y PODA: exogeneidad, aciclicidad, retardo verosimil.\n");
            fprintf(fd, "# Formato:  SALIDA <- ENTRADA  b r s      (# pico de la ccf)\n");
            for (k = 1; k <= nl; k++)
                fprintf(fd, "%-4s <- %-4s   %d 0 %d      # pico %+.3f\n",
                        Ts[lo[k]].name ? Ts[lo[k]].name : "?",
                        Ts[li[k]].name ? Ts[li[k]].name : "?",
                        lb[k], lsv[k], lpk[k]);
            fclose(fd);
        }
        fc = fopen(fcns, "w");
        if (fc) {
            fprintf(fc, "# Covarianzas contemporaneas PROPUESTAS por drtran -g.\n");
            fprintf(fc, "# Indices q[i,j] por orden en la linea de comandos:\n#  ");
            for (i = 1; i <= m; i++)
                fprintf(fc, " %d=%s", i, Ts[i].name ? Ts[i].name : "?");
            fprintf(fc, "\n");
            /* los slots q[i,j] son el triangulo INFERIOR (i>j): mayor indice
               primero.  El bucle guarda qi<qj, asi que se escribe q[qj,qi].    */
            for (k = 1; k <= nq; k++)
                fprintf(fc, "q[%d,%d] = free      # %s - %s : r(0) = %+.3f\n",
                        qj[k], qi[k],
                        Ts[qi[k]].name ? Ts[qi[k]].name : "?",
                        Ts[qj[k]].name ? Ts[qj[k]].name : "?", qr[k]);
            fclose(fc);
        }

        fprintf(out, "\n=============================================================\n");
        fprintf(out, "  GUIDED MODE (-g): la escalera, paso a paso\n");
        fprintf(out, "=============================================================\n");
        fprintf(out, "  [hecho] paso 2  modelo DIAGONAL estimado.\n");
        fprintf(out, "  [hecho] paso 3  red IDENTIFICADA de las ccf residuales:\n");
        fprintf(out, "            -> %-16s (%d enlace(s) candidato(s))\n", fdag, nl);
        fprintf(out, "            -> %-16s (%d covarianza(s))\n", fcns, nq);
        fprintf(out, "\n  paso 4  CONFIRMA y ESTIMA.  La red escrita es una GUIA:\n");
        fprintf(out, "          revisa %s y PODA (exogeneidad, aciclicidad,\n", fdag);
        fprintf(out, "          retardo verosimil) antes de estimar.  Luego:\n\n");
        fprintf(out, "            drtran <tus .pre, mismo orden> -n %s -c %s\n", fdag, fcns);
        fprintf(out, "\n  Los numeradores factorizados/compartidos del sistema se\n");
        fprintf(out, "  expresan en el -c (PRODUCTO x=y*z, COMBINACION x=y+z; ver -h).\n");
        fprintf(out, "=============================================================\n");
    }

    free_vector(cneg, 1, nlags + 1);  free_vector(cpos, 1, nlags + 1);
    free_vector(aj, 1, n);            free_vector(ai, 1, n);
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
            } else if (slot_kind[i] == SLOT_PRODUCT) {
                fprintf(outputv, "%-20s %12.6f       (= %s%s * %s)\n",
                        slot_name[i], xf[i], (slot_value[i] < 0 ? "-" : ""),
                        slot_name[slot_pa[i]], slot_name[slot_pb[i]]);
            } else if (slot_kind[i] == SLOT_LINCOMB) {
                char expr[256]; int t; size_t off = 0;
                for (t = 0; t < slot_nlc[i]; t++) {
                    const char *op = (slot_lc_sign[i][t] < 0) ? "- "
                                   : (t == 0 ? "" : "+ ");
                    off += snprintf(expr + off, sizeof expr - off, "%s%s", op,
                                    slot_name[slot_lc_a[i][t]]);
                    if (slot_lc_b[i][t])
                        off += snprintf(expr + off, sizeof expr - off, " * %s",
                                        slot_name[slot_lc_b[i][t]]);
                    if (t + 1 < slot_nlc[i])
                        off += snprintf(expr + off, sizeof expr - off, " ");
                }
                fprintf(outputv, "%-20s %12.6f       (= %s)\n",
                        slot_name[i], xf[i], expr);
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
            if (embed_varma && !phi0_is_identity) {
                fprintf(outputv,
"\n  NOTE: this is the REDUCED-FORM covariance. With a contemporaneous transfer\n"
"  (b=0) the VARMA representation puts omega_0 at lag zero, so Phi(0) != I and the\n"
"  innovations of the OBSERVED series are correlated -- by construction, not by\n"
"  misspecification: Sigma_12 = omega_0 * Sigma_22. The STRUCTURAL innovations are\n"
"  orthogonal (Q diagonal): that is the model's assumption, and it is what the\n"
"  diagnostics below are run on, after undoing the normalisation with\n"
"  a_structural = Phi(0) a_reduced.\n"
"\n"
"  Note also what that undoing IS: a Cholesky factorisation of Sigma with the\n"
"  INPUT ordered FIRST. So this does not escape orthogonalisation -- it escapes\n"
"  the ARBITRARINESS of it. The ordering is not chosen by the analyst; it is the\n"
"  exogeneity of the input, which is an assumption, and one that is TESTED (the\n"
"  cross-correlation at negative lags). With b >= 1 the question does not even\n"
"  arise: Phi(0) = I and the innovations are already orthogonal. The whole\n"
"  identification problem lives at lag zero.\n");
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

            /* Y la ESTRUCTURAL, que es la que el modelo SUPONE ortogonal y la unica
               comparable con la del cast por resta. Sin esto, con -V solo se veia la
               reducida -- y su correlacion es 0.56 POR CONSTRUCCION, no por nada que
               se haya estimado. */
            if (embed_varma && !phi0_is_identity) {
                fprintf(outputv, "Sigma STRUCTURAL = sigma2 * Q  "
                                 "(what the model assumes orthogonal):\n");
                for (i = 1; i <= n_ser; i++) {
                    fprintf(outputv, "  ");
                    for (jj = 1; jj <= n_ser; jj++)
                        fprintf(outputv, "%12.6f", varma1.sigma2 * qq_struct[i][jj]);
                    fprintf(outputv, "\n");
                }
                fprintf(outputv, "\nStructural innovation correlations:\n");
                for (i = 1; i <= n_ser; i++) {
                    fprintf(outputv, "  ");
                    for (jj = 1; jj <= n_ser; jj++)
                        fprintf(outputv, "%12.4f", qq_struct[i][jj] /
                                sqrt(qq_struct[i][i] * qq_struct[jj][jj]));
                    fprintf(outputv, "\n");
                }
                fprintf(outputv, "\n");
            }
        }
        shootx(x, &vq, &ifq, 0, 1);
    }

    {
        struct Tvarma vdiag;
        int ifault_diag = 0, t;

        shootx(x, &vdiag, &ifault_diag, 1, 0);

        /* Con el cast EMPOTRADO, los residuos que devuelve elf son los de la FORMA
           REDUCIDA. Los diagnosticos (adecuacion, exogeneidad, ACF del ruido) hablan
           del RUIDO ESTRUCTURAL, asi que hay que deshacer la normalizacion:
                a_estructural = Phi(0) * a_reducido
           Sin esto, la prueba de adecuacion mide la correlacion contemporanea que la
           PROPIA transferencia genera (Sigma_12 = omega_0 * sigma_X^2) y la declara
           mala especificacion: los dos modelos del IPC salian "NO adecuados" con
           p = 0.0000 solo por eso. */
        if (embed_varma && !phi0_is_identity && ifault_diag == 0) {
            real *tmp = vector(1, n_ser);
            int   ii, jj, tt;
            for (tt = 1; tt <= n_stat; tt++) {
                for (ii = 1; ii <= n_ser; ii++) {
                    tmp[ii] = 0.0;
                    for (jj = 1; jj <= n_ser; jj++)
                        tmp[ii] += phi0_last[ii][jj] * a_est[tt][jj];
                }
                for (ii = 1; ii <= n_ser; ii++) a_est[tt][ii] = tmp[ii];
            }
            free_vector(tmp, 1, n_ser);
        }

        if (ifault_diag == 0) {
            for (t = 1; t <= n_stat; t++)
                for (i = 1; i <= n_ser; i++) vdiag.a[t][i] = a_est[t][i];

            fprintf(outputv, "\n");
            diagnose(&vdiag, Ts[1].freq);
            fprintf(outputv, "\n--- Multivariate diagnostics (Hosking + JB) ---\n");
            multivariate_diagnostics(a_est, n_stat, n_ser, outputv);

            /* -i : identificar la RED a partir de las ccf residuales del diagonal.
               Tiene sentido sobre el modelo DIAGONAL (sin transferencias todavia). */
            if (net_ident && n_ser >= 2)
                identify_network(a_est, n_stat, n_ser, outputv);

            for (j = 1; j <= n_link; j++) link_p_transfer[j] = -1.0;
            for (j = 1; j <= n_link; j++)
                if (lnk[j].s >= 0) {
                    real pt = -1.0, pe = -1.0;
                    transfer_adequacy(a_est, n_stat, j, outputv, &pt, &pe);
                    link_p_transfer[j] = pt;
                    if (j == 1) { sum_p_transfer = pt; sum_p_exog = pe; }
                }

            reformulation_advice(a_est, n_stat, outputv);

            if (fc_horizon > 0) {
                /* ORIGEN de prevision, INDEPENDIENTE de la ventana de estimacion.
                   La estimacion se ancla en la ventana (-estwin E, params FIJOS);
                   el informe -f/-L sale del origen que elija el investigador:
                     -O g   origen explicito en la obs g  (evaluar modelos a un
                            origen concreto: p.ej. -estwin 216 -O 216 preve el
                            hold-out desde el final de la ventana);
                     -O -1  el final ACTUAL de los datos (SPS en tiempo real: un
                            dato nuevo => nuevo origen);
                     (omitido)  el final de la VENTANA de estimacion si -estwin
                            esta activo, o el final de los datos si no.
                   Se fija nobs al origen y se reconstruye la serie estacionaria
                   para que transfer_forecast preva desde ahi. */
                int forigin;
                if      (fc_origin > 0) forigin = fc_origin;
                else if (fc_origin < 0) forigin = nobs_full[1];
                else if (rec_start > 0) forigin = rec_start;
                else                    forigin = nobs_full[1];
                if (forigin > nobs_full[1]) forigin = nobs_full[1];
                for (i = 1; i <= n_ser; i++) Ts[i].nobs = forigin;
                build_stationary_series();
                transfer_forecast(x, npar, fc_horizon, varma1.sigma2, outputv);
            }

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

    /* -estwin es el nombre UNIFICADO con drvarma del modo de ventana fija; -R
       queda como alias oculto (compatibilidad). getopt no entiende opciones largas
       de un solo guion, asi que traducimos el token -estwin -> -R antes del bucle. */
    {
        int ai;
        for (ai = 1; ai < argc; ai++)
            if (strcmp(argv[ai], "-estwin") == 0) argv[ai] = (char *)"-R";
    }

    while ((opt = getopt(argc, argv, "r:s:b:f:m:c:n:a:R:C:g:O:Lp0iXNDEMVSvho:")) != -1) {
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
        case 'O': fc_origin  = atoi(optarg); break;
        case 'm': model_name = optarg;       break;
        case 'p': prewhiten_only = 1;        break;
        case 'i': net_ident = 1;             break;
        case 'g': guide_name = optarg; net_ident = 1; no_transfer = 1; auto_id = 0; break;
        case 'L': latex_forecast = 1;        break;
        case 'c': cons_file = optarg;        break;
        case 'n': net_file  = optarg;        break;
        case 'a': aggr_file = optarg;        break;
        case 'R': rec_start = atoi(optarg);  break;
        case 'C': snprintf(rec_csv, sizeof rec_csv, "%s", optarg); break;
        case '0': no_transfer = 1; auto_id = 0; break;
        case 'V': embed_varma = 1;           break;
        case 'S': embed_varma = 0;           break;
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
    forecast_base = model_name;
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

    /* EL DESPACHO (BUG-8). Si algun enlace cruza operadores de diferenciacion
       distintos, la transferencia necesita la entrada re-diferenciada por el
       operador de la SALIDA -- un segundo vector para la misma serie, y el cast
       empotrado no tiene donde ponerlo. Se cambia al cast por resta, que
       construye el termino aparte.

       Se ANUNCIA, no se hace callando: los dos casts calculan verosimilitudes
       de forma distinta, y elegir entre entradas candidatas para una misma
       salida podria acabar comparando un numero de cada uno.

       Todo el legacy, m6 entero, la red y los canonicos tienen los operadores
       iguales, asi que no pasan por aqui.                                     */
    if (links_need_subtracting() && embed_varma) {
        int j;
        embed_varma = 0;
        fprintf(stderr,
            "Note: switching to the SUBTRACTING cast (-S).\n");
        for (j = 1; j <= n_link; j++) {
            int o = lnk[j].out, in = lnk[j].inp;
            if (!operators_differ(o, in)) continue;
            fprintf(stderr,
                "      %s <- %s: differenced by DIFFERENT operators (orders %d "
                "and %d).\n", Ts[o].name, Ts[in].name,
                Tm[o].ornsop, Tm[in].ornsop);
        }
        fprintf(stderr,
            "      The transfer relates the LEVELS and the noise carries the\n"
            "      differencing, so the input must enter differenced by the\n"
            "      OUTPUT's operator. The embedded cast has one column per\n"
            "      series and cannot hold a second vector; the subtracting one\n"
            "      can. Without this the fitted transfer would be nu(B)*Delta(B)\n"
            "      and the reported GAIN wrong by Delta(1). See BUG-8.\n");
    }

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

    /*  BUG-12: lo que leyeron los .pre, soltado.  Hasta el 2026-08-21 no habia
     *  desasignador y cada lectura dejaba una veintena de bloques.  No hacia
     *  dano aqui -- se leen dos ficheros y se termina -- pero tapaba: una
     *  salida de valgrind con veinte fugas conocidas es una en la que la
     *  veintiuna no se ve.                                                    */
    for (i = 1; i <= n_ser; i++)
        free_fue_pre(&Tm[i], &Ts[i], DataMat[i]);

    fclose(outputv);
    return 0;
}
