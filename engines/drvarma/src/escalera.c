/*****************************************************************************/
/*  escalera.c -- part of drvarma (multivariate VARMA modelling).
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
/*  LA ESCALERA: drvarma leyendo los .pre de fue.                            */
/*                                                                           */
/*  Cada serie trae de su .pre su modelo univariante entero -- Box-Cox,      */
/*  deterministas, operador no estacionario, media y factores ARMA -- y con  */
/*  el se lleva a su serie estacionaria w_i (lib/prewhiten, el mismo codigo  */
/*  que drtran y su GUI). Sobre w se estima                                   */
/*                                                                           */
/*      Phi(B) (w_t - mu) = Theta(B) a_t,    a_t ~ N(0, sigma2 Q)            */
/*                                                                           */
/*  con la DIAGONAL de cada fila tomada de su .pre --phi_i(B)Phi_i(B^s),     */
/*  theta_i(B)Theta_i(B^s)-- y la parte de FUERA libre hasta los ordenes p y */
/*  q de la linea de ordenes: la dinamica cruzada. La propia ya la trae cada */
/*  .pre.                                                                    */
/*                                                                           */
/*  Con p = q = 0 y Q diagonal el sistema se parte en los m univariantes, y  */
/*  eso es LA PUERTA: la verosimilitud conjunta evaluada en los optimos      */
/*  univariantes tiene que ser la suma de las univariantes. Si no, el        */
/*  programa se para. Ver docs/DESIGN-v5-ladder.md.                        */
/*****************************************************************************/

#include "main.h"
#include "fue_pre_reader.h"
#include "prewhiten.h"
#include "escalera.h"
#include "version.h"
#include "forecast.h"
#include "inpcheck.h"
#include "diagmv.h"           /* Hosking and Jarque-Bera, shared (lib/diagmv) */
#include <string.h>
#include <stdint.h>
#include <math.h>

#define ESC_MAX     10          /* series                                  */
#define ESC_MAXLAG  24          /* orden maximo de la dinamica cruzada     */
#define ESC_NAMELEN 64
#define GATE_TOL    1e-6        /* |logL_diag - SUM logL_i|, relativo      */
#define PRE_MOVE    1e-3        /* un .pre que se mueve mas no es un optimo */

/* Globales del programa (drvarma.c, diagnose.c, drvmlest.c, qnewtopt.c). */
extern real macheps;
extern FILE *outputv;
extern int  quiet_mode;
extern int  nser;
extern char **series_names;
extern int  data_freq, data_start_year, data_start_sub, trans_d, trans_D;
/* est_fdhess: main.h */
void print_matrices(struct Tvarma *varma);
void print_roots(struct Tvarma *varma);

/* ------------------------------------------------------------------------- */
/*  Estado                                                                   */
/* ------------------------------------------------------------------------- */

/* Lo leido. */
static int             n_ser;
static const char     *pre_path[ESC_MAX + 1];
static struct Tusmodel Tm[ESC_MAX + 1];
static struct Tseries  Ts[ESC_MAX + 1];
static real          **DataMat[ESC_MAX + 1];
static int             ar_deg[ESC_MAX + 1], ma_deg[ESC_MAX + 1];
static real            mu[ESC_MAX + 1];

/* Las estacionarias, recortadas a la ventana comun: w[i][1..n_stat]. */
static real *w[ESC_MAX + 1];
static int   n_stat;
static int   n_trim[ESC_MAX + 1];    /* observaciones que el recorte le quita */

/* Lo que se ajusta AHORA: un subconjunto de series (una sola para los
   univariantes de la puerta, todas para el sistema) y su estructura.     */
static int act_m, act[ESC_MAX + 1];
static int cx_p, cx_q, cx_diagcov;
static int opt_fixarma;

/* ------------------------------------------------------------------------- */
/*  LA MASCARA DE LAS DETERMINISTAS                                          */
/*                                                                           */
/*  Cada variable determinista de un fichero lleva sus coeficientes marcados  */
/*  libres o fijos. La escalera decide, variable a variable, si los libres   */
/*  entran en el vector de parametros, y lo decide dos veces:                */
/*    en la PUERTA   (cada serie sola):  det_g[i][d]                         */
/*    en el CONJUNTO (el diagonal y el modelo pedido):  det_j[i][d]          */
/*  Lo que no entra queda en el valor que tenga: el del fichero, o el que    */
/*  dejo la puerta si alli se estimo ("los valores de la puerta").           */
/*                                                                           */
/*    -det LISTA      que se estima en el conjunto: all, none, una clase     */
/*                    (seasonal: cos sin alter; calendar: easter; interv:    */
/*                    impulse compimp step ramp; trend), SERIE:clase, o el   */
/*                    nombre de un coeficiente como lo escribe la salida     */
/*                    (w0_IPC[step 8 1990]: libera esa variable).            */
/*    -redet          es -det all (lo de siempre).                           */
/*    -detbase file   lo fijo, a los valores del fichero;                    */
/*    -detbase gate   lo fijo, a los que estima la puerta en la ventana      */
/*                    comun;                                                 */
/*    sin -detbase    gate en las series que la ventana comun recorta (su    */
/*                    fichero ya no es el optimo de la muestra que se usa),  */
/*                    file en las demas.                                     */
/*  Todo lo libre en el conjunto lo es tambien en la puerta. Solo se libera  */
/*  lo que el fichero marca como libre.                                      */
/* ------------------------------------------------------------------------- */
#define DET_LISTA 1024
static char det_lista[DET_LISTA] = "";      /* -det (o "all" con -redet) */
static int  det_base = 0;                   /* 0 sin -detbase, 1 file, 2 gate */
static int  det_etapa = 1;                  /* 0 la puerta, 1 el conjunto */
static unsigned char *det_j[ESC_MAX + 1], *det_g[ESC_MAX + 1];   /* [1..NdetVar] */

/* -links "A<-B,C<-A" (2026-09-29): the cross dynamics only on the pairs the
   evidence points at. cx_link[i][j] = 1 lets series j enter the equation of
   series i (AR and MA, every lag up to p and q); without -links every pair
   is linked, as before. Indexed by SERIES, as cAR/cMA.                     */
static int opt_links;
static int cx_link[ESC_MAX + 1][ESC_MAX + 1];

static int linked(int i, int j)
{
    return i != j && (!opt_links || cx_link[i][j]);
}

/* -zero "AR1[A<-B],MA2[B<-A]" (2026-11-06, sim_gui S7): single cross
   coefficients held at zero -- Tiao and Box's simplification by coefficient
   (1981, §4), the one the Python port's zeros= does. cx_zar[k][i][j] = 1
   holds the lag-k AR coefficient of series j in the equation of series i at
   zero; cx_zma the MA one. Without -zero nothing changes: every coefficient
   of a linked pair is free, as before.                                     */
static int opt_zero;
static int cx_zar[ESC_MAXLAG + 1][ESC_MAX + 1][ESC_MAX + 1];
static int cx_zma[ESC_MAXLAG + 1][ESC_MAX + 1][ESC_MAX + 1];

static int free_ar(int k, int i, int j) { return linked(i, j) && !cx_zar[k][i][j]; }
static int free_ma(int k, int i, int j) { return linked(i, j) && !cx_zma[k][i][j]; }

/* ------------------------------------------------------------------------- */
/*  LA FORMA ECHELON (-echelon n1,n2,...), Lutkepohl y Poskitt (1996)        */
/*                                                                           */
/*  La forma echelon invertida, A(B) w_t = M(B) a_t con M_0 = A_0 triangular */
/*  inferior de diagonal unidad. Cada serie lleva su indice de Kronecker n_r */
/*  y con ellos queda fijado que coeficientes existen (su tabla, 2.3):       */
/*      n_rc = min(n_r + 1, n_c) si c < r,   min(n_r, n_c) si c >= r         */
/*    lo propio de la fila r   AR(n_r) y MA(n_r)                             */
/*    los AR cruzados r <- c   los retardos n_r - n_rc + 1 .. n_r            */
/*    los MA cruzados r <- c   los retardos 1 .. n_r                         */
/*    A_0, el termino contemporaneo r <- c   solo si c < r y n_c > n_r       */
/*  El orden de las series es parte del modelo. Con los indices iguales no   */
/*  se restringe nada y A_0 = I: es el VARMA(n,n) de la ruta V.              */
/*                                                                           */
/*  Va por la ruta V: los univariantes por la puerta, lo propio sustituido,  */
/*  el factor de fila. A_0 se guarda como C_0 = I - A_0 (estrictamente       */
/*  inferior), con el signo de Box y Jenkins: AR0[r<-c] = C_0[r][c]. elf     */
/*  quiere la forma estandar: Phi_k = A_0^-1 Phi~_k, Theta_k = A_0^-1        */
/*  Theta~_k. Es Ladder(echelon=n, own="free") del porte.                    */
/* ------------------------------------------------------------------------- */
static int  ech_on = 0;                      /* -echelon */
static int  ech_n[ESC_MAX + 1];              /* los indices, por serie */
static real cA0[ESC_MAX + 1][ESC_MAX + 1];   /* C_0 */

static int ech_nrc(int r, int c)
{
    int a = (c < r) ? ech_n[r] + 1 : ech_n[r];
    return a < ech_n[c] ? a : ech_n[c];
}
/* el termino contemporaneo i <- j: solo con estructura cruzada activa (en el
   ajuste diagonal, la V sin cruzados, no hay A_0) */
static int free_a0(int i, int j)
{
    return ech_on && (cx_p > 0 || cx_q > 0) && j < i && linked(i, j) && ech_n[j] > ech_n[i];
}

/* The free cross coefficients among the active series: (p + q) per linked
   pair, less the -zero ones. Without -zero, (cx_p + cx_q) times the linked
   ordered pairs among the active series, as before.                       */
static int n_cross_free(void)
{
    int a, b, k, n = 0;
    for (k = 1; k <= cx_p; k++)
        for (a = 1; a <= act_m; a++)
            for (b = 1; b <= act_m; b++)
                if (a != b && free_ar(k, act[a], act[b])) n++;
    for (k = 1; k <= cx_q; k++)
        for (a = 1; a <= act_m; a++)
            for (b = 1; b <= act_m; b++)
                if (a != b && free_ma(k, act[a], act[b])) n++;
    for (a = 1; a <= act_m; a++)
        for (b = 1; b <= act_m; b++)
            if (a != b && free_a0(act[a], act[b])) n++;
    return n;
}

/* La parte cruzada y Q, indexadas por SERIE y no por posicion: asi el
   estado sobrevive al pasar de un subconjunto a otro, y el diagonal es la
   semilla del completo sin traducir nada.                                 */
static real cAR[ESC_MAXLAG + 1][ESC_MAX + 1][ESC_MAX + 1];
static real cMA[ESC_MAXLAG + 1][ESC_MAX + 1][ESC_MAX + 1];
static real lvar[ESC_MAX + 1];                 /* log(Q_ii / Q_11)        */
static real qcov[ESC_MAX + 1][ESC_MAX + 1];    /* Q_ij, i > j             */

/* ------------------------------------------------------------------------- */
/*  Las series estacionarias                                                 */
/* ------------------------------------------------------------------------- */

static int det_on(int i, int d)
{
    unsigned char *m = det_etapa ? det_j[i] : det_g[i];
    return m != NULL && m[d];
}
/* los coeficientes libres de las deterministas de la serie i que la mascara
   de la etapa deja entrar */
static int det_n(int i)
{
    struct Tusmodel *M = &Tm[i];
    int n = 0, d, j;
    for (d = 1; d <= M->NdetVar; d++) {
        if (!det_on(i, d)) continue;
        for (j = 0; j <= M->Nomega[d]; j++) if (M->Imega[d][j] == 1) n++;
        for (j = 1; j <= M->Ndelta[d]; j++) if (M->Ielta[d][j] == 1) n++;
    }
    return n;
}
static int det_n_etapa(int i, int etapa)
{
    int e = det_etapa, n;
    det_etapa = etapa; n = det_n(i); det_etapa = e;
    return n;
}
static int det_pack(int i, real *x, int idx)
{
    struct Tusmodel *M = &Tm[i];
    int base = idx, d, j;
    for (d = 1; d <= M->NdetVar; d++) {
        if (!det_on(i, d)) continue;
        for (j = 0; j <= M->Nomega[d]; j++) if (M->Imega[d][j] == 1) x[idx++] = M->Omega[d][j];
        for (j = 1; j <= M->Ndelta[d]; j++) if (M->Ielta[d][j] == 1) x[idx++] = M->Delta[d][j];
    }
    return idx - base;
}
static void det_unpack(int i, real *x, int *idx)
{
    struct Tusmodel *M = &Tm[i];
    int d, j;
    for (d = 1; d <= M->NdetVar; d++) {
        if (!det_on(i, d)) continue;
        for (j = 0; j <= M->Nomega[d]; j++) if (M->Imega[d][j] == 1) M->Omega[d][j] = x[(*idx)++];
        for (j = 1; j <= M->Ndelta[d]; j++) if (M->Ielta[d][j] == 1) M->Delta[d][j] = x[(*idx)++];
    }
}
/* la clase de una variable por la primera palabra de su especificacion */
static const char *det_clase(const char *spec)
{
    char w[16];
    if (spec == NULL || sscanf(spec, "%15s", w) != 1) return "";
    if (!strcmp(w, "cos") || !strcmp(w, "sin") || !strcmp(w, "alter")) return "seasonal";
    if (!strcmp(w, "easter")) return "calendar";
    if (!strcmp(w, "impulse") || !strcmp(w, "compimp") || !strcmp(w, "step") || !strcmp(w, "ramp"))
        return "interv";
    if (!strcmp(w, "trend")) return "trend";
    return "";
}
/* Monta det_j y det_g. Llamar con las series leidas y n_trim ya calculado.
   Devuelve 0, o 1 si un selector de -det no casa con nada (lo imprime).     */
static int det_monta(void)
{
    char lista[DET_LISTA], *tok, *sig;
    int  i, d;

    for (i = 1; i <= n_ser; i++) {
        det_j[i] = (unsigned char *)calloc((size_t)Tm[i].NdetVar + 2, 1);
        det_g[i] = (unsigned char *)calloc((size_t)Tm[i].NdetVar + 2, 1);
    }
    snprintf(lista, sizeof lista, "%s", det_lista);
    for (tok = lista; tok != NULL && *tok; tok = sig) {
        int casa = 0;
        const char *sel = tok, *ser = NULL;
        char *c;
        /* la coma separa selectores, salvo dentro de los corchetes de un nombre */
        {   int hondo = 0;
            for (c = tok; *c; c++) {
                if (*c == '[') hondo++;
                else if (*c == ']') hondo--;
                else if (*c == ',' && hondo == 0) break;
            }
            if (*c == ',') { *c = 0; sig = c + 1; } else sig = NULL;
        }
        if (strcmp(sel, "none") == 0) continue;
        if (strchr(sel, '[') == NULL && (c = strchr(tok, ':')) != NULL) { *c = 0; ser = tok; sel = c + 1; }
        for (i = 1; i <= n_ser; i++) {
            if (ser != NULL && strcmp(ser, Ts[i].name) != 0) continue;
            for (d = 1; d <= Tm[i].NdetVar; d++) {
                const char *sp = (Tm[i].detspec && Tm[i].detspec[d]) ? Tm[i].detspec[d] : "det";
                int si = 0;
                if (strcmp(sel, "all") == 0 || strcmp(sel, det_clase(sp)) == 0) si = 1;
                else if (strchr(sel, '[') != NULL) {       /* un nombre: w0_SERIE[spec] */
                    char nom[ESC_NAMELEN];
                    const char *u = strchr(sel, '_');
                    snprintf(nom, sizeof nom, "%s[%s]", Ts[i].name, sp);
                    if (u != NULL && strcmp(u + 1, nom) == 0) si = 1;
                }
                if (si) { det_j[i][d] = 1; casa = 1; }
            }
        }
        if (!casa && strcmp(sel, "all") != 0) {
            printf("ERROR: -det: '%s%s%s' does not match any deterministic term.\n"
                   "       Give all, none, a class (seasonal, calendar, interv, trend),\n"
                   "       SERIES:class, or a coefficient's name as the output writes it,\n"
                   "       for instance \"w0_IPC[step 8 1990]\".\n",
                   ser ? ser : "", ser ? ":" : "", sel);
            return 1;
        }
    }
    for (i = 1; i <= n_ser; i++) {
        int todo = det_base == 2 || (det_base == 0 && n_trim[i] > 0);
        for (d = 1; d <= Tm[i].NdetVar; d++) det_g[i][d] = (unsigned char)(todo || det_j[i][d]);
    }
    return 0;
}

/* Cada modelo pierde tantas observaciones iniciales como el orden de su
   operador no estacionario. Todas acaban en la misma fecha (lo exige
   fuepre_check_alignment), asi que quedarse con las ULTIMAS n_stat de cada
   una las alinea en el calendario.                                        */
static int build_stationary(void)
{
    real *wi[ESC_MAX + 1];
    int   nst[ESC_MAX + 1], i, t, nmin = 0;

    for (i = 1; i <= n_ser; i++) {
        wi[i] = NULL; nst[i] = 0;
        apply_univariate_model(&Tm[i], &Ts[i], DataMat[i], &wi[i], &nst[i]);
        if (wi[i] == NULL || nst[i] <= 0) {
            while (--i >= 1) free_vector(wi[i], 1, nst[i]);
            return 1;
        }
        if (nmin == 0 || nst[i] < nmin) nmin = nst[i];
    }
    for (i = 1; i <= n_ser; i++) {
        if (w[i]) free_vector(w[i], 1, n_stat);
    }
    n_stat = nmin;
    for (i = 1; i <= n_ser; i++) {
        int off = nst[i] - n_stat;
        n_trim[i] = off;
        w[i] = vector(1, n_stat);
        for (t = 1; t <= n_stat; t++) w[i][t] = wi[i][t + off];
        free_vector(wi[i], 1, nst[i]);
    }
    return 0;
}

/* ------------------------------------------------------------------------- */
/*  El vector de parametros                                                  */
/*                                                                           */
/*  Por serie activa: sus factores ARMA libres (salvo -fixarma); luego sus   */
/*  deterministas libres (solo con -redet); luego las medias libres; luego   */
/*  la dinamica cruzada, retardo a retardo, fila a fila; y al final Q:       */
/*  log(Q_aa/Q_11) para a >= 2 y, salvo -diagcov, las covarianzas.           */
/*  esc_pack, esc_unpack y esc_names recorren EXACTAMENTE ese orden.         */
/* ------------------------------------------------------------------------- */
/* The free ARMA coefficients of series i's own model, as its .pre has them. */
static int esc_n_arma(int i)
{
    return n_ar_free_params(&Tm[i]) + n_ma_free_params(&Tm[i]);
}

static int esc_npar(void)
{
    int a, n = 0;
    if (!opt_fixarma)
        for (a = 1; a <= act_m; a++) n += esc_n_arma(act[a]);
    for (a = 1; a <= act_m; a++) n += det_n(act[a]);
    for (a = 1; a <= act_m; a++) if (Tm[act[a]].Imu) n++;
    n += n_cross_free();
    n += act_m - 1;
    if (!cx_diagcov) n += act_m * (act_m - 1) / 2;
    return n;
}

static void esc_pack(real *x)
{
    int a, b, k, i, idx = 1;
    for (a = 1; a <= act_m; a++) {
        i = act[a];
        if (opt_fixarma) continue;
        idx += pack_ar_factors(&Tm[i], x, idx);
        idx += pack_ma_factors(&Tm[i], x, idx);
    }
    for (a = 1; a <= act_m; a++) idx += det_pack(act[a], x, idx);
    for (a = 1; a <= act_m; a++) if (Tm[act[a]].Imu) x[idx++] = mu[act[a]];
    for (k = 1; k <= cx_p; k++)
        for (a = 1; a <= act_m; a++)
            for (b = 1; b <= act_m; b++)
                if (a != b && free_ar(k, act[a], act[b])) x[idx++] = cAR[k][act[a]][act[b]];
    for (k = 1; k <= cx_q; k++)
        for (a = 1; a <= act_m; a++)
            for (b = 1; b <= act_m; b++)
                if (a != b && free_ma(k, act[a], act[b])) x[idx++] = cMA[k][act[a]][act[b]];
    for (a = 1; a <= act_m; a++)
        for (b = 1; b <= act_m; b++)
            if (a != b && free_a0(act[a], act[b])) x[idx++] = cA0[act[a]][act[b]];
    for (a = 2; a <= act_m; a++) x[idx++] = lvar[act[a]];
    if (!cx_diagcov)
        for (a = 2; a <= act_m; a++)
            for (b = 1; b < a; b++) x[idx++] = qcov[act[a]][act[b]];
}

static void esc_unpack(real *x)
{
    int a, b, k, i, idx = 1;
    for (a = 1; a <= act_m; a++) {
        i = act[a];
        if (opt_fixarma) continue;
        unpack_ar_factors(&Tm[i], x, &idx);
        unpack_ma_factors(&Tm[i], x, &idx);
    }
    for (a = 1; a <= act_m; a++) det_unpack(act[a], x, &idx);
    for (a = 1; a <= act_m; a++) if (Tm[act[a]].Imu) mu[act[a]] = x[idx++];
    for (k = 1; k <= cx_p; k++)
        for (a = 1; a <= act_m; a++)
            for (b = 1; b <= act_m; b++)
                if (a != b && free_ar(k, act[a], act[b])) cAR[k][act[a]][act[b]] = x[idx++];
    for (k = 1; k <= cx_q; k++)
        for (a = 1; a <= act_m; a++)
            for (b = 1; b <= act_m; b++)
                if (a != b && free_ma(k, act[a], act[b])) cMA[k][act[a]][act[b]] = x[idx++];
    for (a = 1; a <= act_m; a++)
        for (b = 1; b <= act_m; b++)
            if (a != b && free_a0(act[a], act[b])) cA0[act[a]][act[b]] = x[idx++];
    for (a = 2; a <= act_m; a++) lvar[act[a]] = x[idx++];
    if (!cx_diagcov)
        for (a = 2; a <= act_m; a++)
            for (b = 1; b < a; b++) qcov[act[a]][act[b]] = x[idx++];
}

/*  esc_ndyn, esc_npar_ser -- WHAT THE RESIDUALS COST, for the degrees of
 *  freedom of their tests (BUG-42, item 3).  Hosking's Q is chi2(m^2 s - k)
 *  with k the DYNAMIC coefficients behind the residuals: each active
 *  series' own ARMA (estimated by its .pre, even when -fixarma holds it
 *  fixed here, as the Python port counts it) plus the free cross AR and MA
 *  of esc_npar.  Not the means, the deterministic terms or Q.  The Ljung-Box
 *  of residual a subtracts only its own ARMA: the cross coefficients are
 *  off the diagonal.  npar_ser[1..act_m]; free() it.                      */
static int esc_ndyn(void)
{
    int a, n = 0;
    for (a = 1; a <= act_m; a++) n += esc_n_arma(act[a]);
    return n + n_cross_free();
}

static int *esc_npar_ser(void)
{
    int a, *n = (int *) calloc((size_t) act_m + 1, sizeof(int));
    for (a = 1; a <= act_m; a++) n[a] = esc_n_arma(act[a]);
    return n;
}

/* Los nombres, en el mismo orden. names[1..npar][ESC_NAMELEN]. */
static void name_factors(char (*names)[ESC_NAMELEN], int *idx, int i, int is_ar)
{
    struct Tusmodel *M = &Tm[i];
    const char *s  = Ts[i].name;
    int  n1 = is_ar ? M->NumAr1 : M->NumMa1;
    int  n2 = is_ar ? M->NumAr2 : M->NumMa2;
    int  nf = is_ar ? M->NumAr1f : M->NumMa1f;
    int *o1 = is_ar ? M->p1 : M->q1;
    int *o2 = is_ar ? M->p2 : M->q2;
    int **f1 = is_ar ? M->Ia1 : M->Im1;
    int **f2 = is_ar ? M->Ia2 : M->Im2;
    int  *ff = is_ar ? M->Ia1f : M->Im1f;
    int  *fr = is_ar ? M->pfre1 : M->qfre1;
    const char *lo = is_ar ? "phi" : "theta", *up = is_ar ? "Phi" : "Theta";
    int f, j;

    for (f = 1; f <= n1; f++)
        for (j = 1; j <= o1[f]; j++)
            if (f1[f][j] == 1) {
                if (n1 > 1) snprintf(names[(*idx)++], ESC_NAMELEN, "%s%d_%s[B^%d]", lo, f, s, j);
                else        snprintf(names[(*idx)++], ESC_NAMELEN, "%s_%s[B^%d]", lo, s, j);
            }
    for (f = 1; f <= n2; f++)
        for (j = 1; j <= o2[f]; j++)
            if (f2[f][j] == 1)
                snprintf(names[(*idx)++], ESC_NAMELEN, "%s_%s[B^%d]", up, s, j * M->sper);
    for (f = 1; f <= nf; f++)
        if (ff[f] == 1)
            snprintf(names[(*idx)++], ESC_NAMELEN, "%sf_%s[f=%d]", lo, s, fr[f]);
}

static void esc_names(char (*names)[ESC_NAMELEN])
{
    int a, b, k, i, j, d, idx = 1;
    for (a = 1; a <= act_m; a++) {
        if (opt_fixarma) continue;
        name_factors(names, &idx, act[a], 1);
        name_factors(names, &idx, act[a], 0);
    }
        for (a = 1; a <= act_m; a++) {
            struct Tusmodel *M = &Tm[act[a]];
            for (d = 1; d <= M->NdetVar; d++) {
                const char *sp = (M->detspec && M->detspec[d]) ? M->detspec[d] : "det";
                if (!det_on(act[a], d)) continue;
                for (j = 0; j <= M->Nomega[d]; j++)
                    if (M->Imega[d][j] == 1)
                        snprintf(names[idx++], ESC_NAMELEN, "w%d_%s[%s]", j, Ts[act[a]].name, sp);
                for (j = 1; j <= M->Ndelta[d]; j++)
                    if (M->Ielta[d][j] == 1)
                        snprintf(names[idx++], ESC_NAMELEN, "d%d_%s[%s]", j, Ts[act[a]].name, sp);
            }
        }
    for (a = 1; a <= act_m; a++)
        if (Tm[act[a]].Imu) snprintf(names[idx++], ESC_NAMELEN, "mu_%s", Ts[act[a]].name);
    for (k = 1; k <= cx_p; k++)
        for (a = 1; a <= act_m; a++)
            for (b = 1; b <= act_m; b++)
                if (a != b && free_ar(k, act[a], act[b])) {
                    i = act[a]; j = act[b];
                    snprintf(names[idx++], ESC_NAMELEN, "AR%d[%s<-%s]", k, Ts[i].name, Ts[j].name);
                }
    for (k = 1; k <= cx_q; k++)
        for (a = 1; a <= act_m; a++)
            for (b = 1; b <= act_m; b++)
                if (a != b && free_ma(k, act[a], act[b])) {
                    i = act[a]; j = act[b];
                    snprintf(names[idx++], ESC_NAMELEN, "MA%d[%s<-%s]", k, Ts[i].name, Ts[j].name);
                }
    for (a = 1; a <= act_m; a++)
        for (b = 1; b <= act_m; b++)
            if (a != b && free_a0(act[a], act[b]))
                snprintf(names[idx++], ESC_NAMELEN, "AR0[%s<-%s]", Ts[act[a]].name, Ts[act[b]].name);
    for (a = 2; a <= act_m; a++)
        snprintf(names[idx++], ESC_NAMELEN, "log(Q[%s]/Q[%s])", Ts[act[a]].name, Ts[act[1]].name);
    if (!cx_diagcov)
        for (a = 2; a <= act_m; a++)
            for (b = 1; b < a; b++)
                snprintf(names[idx++], ESC_NAMELEN, "Q[%s,%s]", Ts[act[a]].name, Ts[act[b]].name);
}

/* ------------------------------------------------------------------------- */
/*  El cast: vector de parametros -> estructura VARMA                        */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/*  LA RUTA V (-route v): el vector primero, POR LA PUERTA                   */
/*                                                                           */
/*  La puerta es la de siempre: los univariantes tal cual, que dan la vara   */
/*  de medir, la media, las deterministas y la serie estacionaria. Pasada,   */
/*  el ARMA REGULAR de cada serie se sustituye por un AR(p)/MA(q) libre, con */
/*  p y q los ordenes del vector: la matriz VARMA(p,q) entera es libre. Lo   */
/*  que queda del univariante -- sus factores anuales y de frecuencia fija   */
/*  -- multiplica la FILA entera, cruzados incluidos (el factor de fila de   */
/*  Lutkepohl y Poskitt, como en la forma echelon del porte):                */
/*      F_a(B) [Phi(B) w]_a = G_a(B) [Theta(B) a]_a                          */
/*  La U y la V solo difieren en eso: que es lo propio y como se siembra.    */
/*  Con el ajuste diagonal de despues (la V sin cruzados) como base del LR.  */
/* ------------------------------------------------------------------------- */
static int opt_route_v = 0;      /* -route v */
/* -zero sobre lo PROPIO (ruta V): phi_SERIE[B^k] o theta_SERIE[B^k] a cero.
   En la V todo coeficiente es libre, los propios incluidos, y la
   simplificacion de Tiao y Box tambien es sobre ellos. En la U lo propio es
   el univariante y sus ceros son de art. Es own_zeros del porte.           */
static unsigned char v_zar[ESC_MAX + 1][ESC_MAXLAG + 1], v_zma[ESC_MAX + 1][ESC_MAXLAG + 1];
static int v_nzeros = 0;
static int v_on = 0;             /* la sustitucion ya esta hecha: el molde lleva el factor de fila */
static int v_reg_ar[ESC_MAX + 1], v_reg_ma[ESC_MAX + 1];   /* grados regulares (tras sustituir) */
static int v_fac_ar[ESC_MAX + 1], v_fac_ma[ESC_MAX + 1];   /* grados del factor de fila */
static int v_uni_ar[ESC_MAX + 1], v_uni_ma[ESC_MAX + 1];   /* grados regulares del univariante */

/* El modelo de la serie i con solo su parte regular (regular = 1) o solo el
   resto, los factores anuales y de frecuencia fija (regular = 0): una copia
   superficial con los otros factores a cero, para expandirla aparte.        */
static struct Tusmodel v_parte(int i, int regular)
{
    struct Tusmodel M = Tm[i];
    if (regular) { M.NumAr2 = M.NumMa2 = 0; M.NumAr1f = M.NumMa1f = 0; }
    else         { M.NumAr1 = M.NumMa1 = 0; }
    return M;
}

/* Un bloque regular (AR o MA) pasa a ser UN factor libre de orden k, con los
   valores de c[1..k]. La forma es la del lector (read_arma_section).        */
static void v_un_factor(int *Num, int **ord, real ***coef, int ***flag, int k, const real *c)
{
    int i, j;
    for (i = 1; i <= *Num; i++) {
        free_vector((*coef)[i], 0, (*ord)[i]);
        free_ivector((*flag)[i], 0, (*ord)[i]);
    }
    if (*Num > 0) { free_ivector(*ord, 1, *Num); free(*coef + 1); free(*flag + 1); }
    *Num = 0; *ord = NULL; *coef = NULL; *flag = NULL;
    if (k <= 0) return;
    *Num = 1;
    *ord  = ivector(1, 1);  (*ord)[1] = k;
    *coef = (real **)malloc(sizeof(real *)) - 1;
    *flag = (int **)malloc(sizeof(int *)) - 1;
    (*coef)[1] = vector(0, k);
    (*flag)[1] = ivector(0, k);
    (*coef)[1][0] = -1.0;  (*flag)[1][0] = 0;
    for (j = 1; j <= k; j++) { (*coef)[1][j] = c[j]; (*flag)[1][j] = 1; }
}

/* La sustitucion. Lo propio arranca en los polinomios regulares del
   univariante si caben en p (en q); si no, en cero.                         */
static void v_sustituye(int pc, int qc)
{
    int i, k;
    for (i = 1; i <= n_ser; i++) {
        /* los ordenes propios: los del vector, o el indice de la fila */
        int p = ech_on ? ech_n[i] : pc, q = ech_on ? ech_n[i] : qc;
        struct Tusmodel R = v_parte(i, 1), F = v_parte(i, 0);
        int   da = total_ar_order(&R), dm = total_ma_order(&R);
        real *ca = vector(0, p > da ? p : da), *cm = vector(0, q > dm ? q : dm);
        for (k = 0; k <= (p > da ? p : da); k++) ca[k] = 0.0;
        for (k = 0; k <= (q > dm ? q : dm); k++) cm[k] = 0.0;
        if (da > 0) expand_ar_factors(&R, ca, da);
        if (dm > 0) expand_ma_factors(&R, cm, dm);
        if (da > p) for (k = 0; k <= da; k++) ca[k] = 0.0;
        if (dm > q) for (k = 0; k <= dm; k++) cm[k] = 0.0;
        v_uni_ar[i] = da;  v_uni_ma[i] = dm;
        v_fac_ar[i] = total_ar_order(&F);  v_fac_ma[i] = total_ma_order(&F);
        v_un_factor(&Tm[i].NumAr1, &Tm[i].p1, &Tm[i].Ar1, &Tm[i].Ia1, p, ca);
        v_un_factor(&Tm[i].NumMa1, &Tm[i].q1, &Tm[i].Ma1, &Tm[i].Im1, q, cm);
        v_reg_ar[i] = p;  v_reg_ma[i] = q;
        for (k = 1; k <= p && k <= ESC_MAXLAG; k++)
            if (v_zar[i][k]) { Tm[i].Ia1[1][k] = 0; Tm[i].Ar1[1][k] = 0.0; }
        for (k = 1; k <= q && k <= ESC_MAXLAG; k++)
            if (v_zma[i][k]) { Tm[i].Im1[1][k] = 0; Tm[i].Ma1[1][k] = 0.0; }
        ar_deg[i] = total_ar_order(&Tm[i]);
        ma_deg[i] = total_ma_order(&Tm[i]);
        free_vector(ca, 0, p > da ? p : da);
        free_vector(cm, 0, q > dm ? q : dm);
    }
    v_on = 1;
}

/* La fila a de un polinomio matricial C(B) = I - SUM c_k B^k (k = 1..K, en
   la convencion de elf) por su factor escalar F(B) = 1 - SUM f_j B^j
   (j = 1..d). El resultado, en la misma convencion, ocupa hasta K + d.      */
static void v_fila_por_factor(real ***c, int a, int m, int K, const real *f, int d, const real *c0)
{
    int  l, j, b;
    real **n = matrix(1, K + d, 1, m);
    for (l = 1; l <= K + d; l++)
        for (b = 1; b <= m; b++) {
            real s = (l <= K) ? c[l][a][b] : 0.0;
            for (j = 1; j <= d && j <= l; j++) {
                int  r = l - j;                    /* el retardo de C que toca */
                /* el retardo 0 de la fila es el de A_0: e_a - C_0[a] */
                real cr = (r == 0) ? ((a == b ? 1.0 : 0.0) - (c0 ? c0[b] : 0.0))
                                   : (r <= K ? -c[r][a][b] : 0.0);
                s += f[j] * cr;
            }
            n[l][b] = s;
        }
    for (l = 1; l <= K + d; l++)
        for (b = 1; b <= m; b++) c[l][a][b] = n[l][b];
    free_matrix(n, 1, K + d, 1, m);
}


/* ------------------------------------------------------------------------- */
/*  -echelon search: LA BUSQUEDA DE LOS INDICES DE KRONECKER                 */
/*                                                                           */
/*  Lutkepohl y Poskitt (1996, seccion 3), etapas I y II; es echelon.search  */
/*  del porte (drvarma-python, echelon.py), operacion por operacion.         */
/*   Etapa I   un VAR(h) largo por minimos cuadrados, h por AIC hasta        */
/*             (log T)^1.5: sus residuos hacen de innovaciones.              */
/*   Etapa II  por cada candidato n, minimos cuadrados fila a fila sobre     */
/*               w_t = (I - A_0)(w_t - e_t) - SUM A_k w_t-k + SUM M_k e_t-k  */
/*             y el criterio log|Sigma(n)| + c_T dim(n) / T, con             */
/*             c_T = log T (SC) o 2 log log T (HQ). Despues, una ronda de la */
/*             correccion de Hannan y Kavalieris (1984): los residuos se     */
/*             recalculan con el modelo recien ajustado (sus raices          */
/*             recogidas dentro) y se repite la etapa II.                    */
/*   La busqueda  todos los n de {0..n_max}^m mientras no pasen de 256; si   */
/*             no, los dos pasos de Hannan y Kavalieris.                     */
/*  Con factor de fila, la fila r se ajusta sobre (F_r/G_r)(B) w.            */
/*  Solo da la tabla: no estima por maxima verosimilitud.                    */
/* ------------------------------------------------------------------------- */
#define ES_FULL_MAX 256
typedef struct {
    int    n[ESC_MAX + 1];
    int    dim, T, p;
    double logdet;
    double ***A, ***M;          /* [k][r][c], k = 0..p, 0-based; A[0] = M[0] = A_0 */
    double S[ESC_MAX + 1][ESC_MAX + 1];    /* la covarianza de los residuos */
} EsFit;

static int      es_m, es_T;
static int      es_modelo = 0;  /* 1: la etapa II deja fuera lo que el modelo no tiene libre (-links, -zero) */
static double **es_wd;          /* la serie estacionaria, centrada: [t][c] */
static double **es_f, **es_g;   /* los factores de fila: es_f[r][1..es_df[r]] */
static int     *es_df, *es_dg;

static double **es_mat(int r, int c)
{
    int i;
    double **a = (double **)calloc((size_t)(r > 0 ? r : 1), sizeof(double *));
    for (i = 0; i < r; i++) a[i] = (double *)calloc((size_t)(c > 0 ? c : 1), sizeof(double));
    return a;
}
static void es_free_mat(double **a, int r) { int i; for (i = 0; i < r; i++) free(a[i]); free(a); }
static double ***es_ten(int k, int m)
{
    int i;
    double ***a = (double ***)calloc((size_t)k, sizeof(double **));
    for (i = 0; i < k; i++) a[i] = es_mat(m, m);
    return a;
}
static void es_free_ten(double ***a, int k, int m) { int i; for (i = 0; i < k; i++) es_free_mat(a[i], m); free(a); }

/* minimos cuadrados: b = argmin |y - X b|, u los residuos. X[fila][col].
   Por la descomposicion en valores singulares de X (no por las ecuaciones
   normales, que elevan al cuadrado el numero de condicion): en los
   candidatos que sobran los regresores son casi colineales, y los
   coeficientes -- de los que salen las raices y la correccion de Hannan y
   Kavalieris -- solo quedan determinados asi. Es el lstsq del porte, con su
   umbral: los valores singulares por debajo de eps * max(n, k) * el mayor
   se dan por cero (la solucion de norma minima).                           */
static void es_ls(double **X, const double *y, int n, int k, double *b, double *u)
{
    int i, j, r;
    if (k > 0) {
        real **a = matrix(1, n, 1, k), **v = matrix(1, k, 1, k), *sv = vector(1, k);
        double smax = 0.0, tol;
        for (r = 0; r < n; r++) for (i = 0; i < k; i++) a[r + 1][i + 1] = X[r][i];
        svdcp(a, n, k, sv, v);
        for (j = 1; j <= k; j++) if (sv[j] > smax) smax = sv[j];
        tol = 2.220446049250313e-16 * (n > k ? n : k) * smax;
        for (i = 0; i < k; i++) b[i] = 0.0;
        for (j = 1; j <= k; j++) {
            double s = 0.0;
            if (sv[j] <= tol) continue;
            for (r = 0; r < n; r++) s += a[r + 1][j] * y[r];
            s /= sv[j];
            for (i = 0; i < k; i++) b[i] += v[i + 1][j] * s;
        }
        free_vector(sv, 1, k); free_matrix(v, 1, k, 1, k); free_matrix(a, 1, n, 1, k);
    }
    for (r = 0; r < n; r++) {
        double s = y[r];
        for (i = 0; i < k; i++) s -= X[r][i] * b[i];
        u[r] = s;
    }
}
/* log|S| de una matriz simetrica definida positiva (Cholesky); -inf si no lo es */
static double es_logdet(double **S, int m)
{
    double **L = es_mat(m, m), ld = 0.0;
    int i, j, k, mal = 0;
    for (i = 0; i < m && !mal; i++)
        for (j = 0; j <= i; j++) {
            double s = S[i][j];
            for (k = 0; k < j; k++) s -= L[i][k] * L[j][k];
            if (i == j) {
                if (s <= 0.0) { mal = 1; break; }
                L[i][i] = sqrt(s);  ld += 2.0 * log(L[i][i]);
            } else L[i][j] = s / L[j][j];
        }
    es_free_mat(L, m);
    return mal ? -HUGE_VAL : ld;
}
/* (F/G)(B) de una columna, con los valores previos a la muestra a cero */
static void es_filtra(const double *x, int T, const double *f, int df, const double *g, int dg, double *z)
{
    int t, j;
    for (t = 0; t < T; t++) {
        double s = x[t];
        for (j = 1; j <= df && j <= t; j++) s -= f[j] * x[t - j];
        z[t] = s;
    }
    if (dg > 0)
        for (t = 1; t < T; t++)
            for (j = 1; j <= dg && j <= t; j++) z[t] += g[j] * z[t - j];
}
/* Etapa I: el VAR(h) por minimos cuadrados (con constante) en t = t0..T-1.
   Devuelve log|Sigma|; si e != NULL, sus residuos en e[t][c] (t >= t0).   */
static double es_var(double **wraw, int h, int t0, double **e)
{
    int    m = es_m, T = es_T, n = T - t0, k = 1 + h * m, r, c, j, l;
    double **X = es_mat(n, k), *y = (double *)calloc((size_t)n, sizeof(double));
    double *b = (double *)calloc((size_t)k, sizeof(double)), **U = es_mat(n, m), **S = es_mat(m, m), ld;
    for (r = 0; r < n; r++) {
        X[r][0] = 1.0;
        for (l = 1; l <= h; l++)
            for (c = 0; c < m; c++) X[r][1 + (l - 1) * m + c] = wraw[t0 + r - l][c];
    }
    for (c = 0; c < m; c++) {
        double *u = (double *)calloc((size_t)n, sizeof(double));
        for (r = 0; r < n; r++) y[r] = wraw[t0 + r][c];
        es_ls(X, y, n, k, b, u);
        for (r = 0; r < n; r++) U[r][c] = u[r];
        free(u);
    }
    for (c = 0; c < m; c++)
        for (j = 0; j < m; j++) {
            double s = 0.0;
            for (r = 0; r < n; r++) s += U[r][c] * U[r][j];
            S[c][j] = s / n;
        }
    ld = es_logdet(S, m);
    if (e) for (r = 0; r < n; r++) for (c = 0; c < m; c++) e[t0 + r][c] = U[r][c];
    es_free_mat(S, m); es_free_mat(U, n); free(b); free(y); es_free_mat(X, n);
    return ld;
}
static int es_nrc(const int *n, int r, int c)
{
    int a = (c < r) ? n[r] + 1 : n[r];
    return a < n[c] ? a : n[c];
}
static int es_dim(const int *n, int m)
{
    int r, c, d = 0;
    for (r = 0; r < m; r++)
        for (c = 0; c < m; c++)
            d += (r == c ? n[r] : es_nrc(n, r, c)) + n[r];
    return d;
}
/* Etapa II para los indices n (0-based), con las innovaciones E[t][c]
   (validas en t >= h), sobre t = t0..T-1.                                  */
static void es_stage2(const int *n, double **E, int t0, EsFit *F)
{
    int    m = es_m, T = es_T, nt = T - t0, p = 0, r, c, k, j, d = 0;
    double **U = es_mat(nt, m), **S = es_mat(m, m), **z = es_mat(m, T);
    for (r = 0; r < m; r++) { F->n[r] = n[r]; if (n[r] > p) p = n[r]; }
    F->p = p;
    F->A = es_ten(p + 1, m);  F->M = es_ten(p + 1, m);
    for (r = 0; r < m; r++) F->A[0][r][r] = 1.0;
    for (r = 0; r < m; r++) {
        int     ncol = 0, tipo[4 * ESC_MAX * (ESC_MAXLAG + 2)], lag[4 * ESC_MAX * (ESC_MAXLAG + 2)], col[4 * ESC_MAX * (ESC_MAXLAG + 2)];
        double **X, *y, *b, *u, *x = (double *)calloc((size_t)T, sizeof(double));
        for (c = 0; c < m; c++) {                  /* todas las series por el factor de ESTA fila */
            for (j = 0; j < T; j++) x[j] = es_wd[j][c];
            es_filtra(x, T, es_f[r], es_df[r], es_g[r], es_dg[r], z[c]);
        }
        free(x);
        for (c = 0; c < m; c++) {
            int k0 = (c == r) ? 1 : n[r] - es_nrc(n, r, c) + 1;
            for (k = k0; k <= n[r]; k++) {
                if (es_modelo && c != r && !(k == 0 ? free_a0(r + 1, c + 1) : free_ar(k, r + 1, c + 1))) continue;
                if (es_modelo && c == r && k <= ESC_MAXLAG && v_zar[r + 1][k]) continue;
                tipo[ncol] = 0; lag[ncol] = k; col[ncol++] = c;
            }
        }
        for (c = 0; c < m; c++)
            for (k = 1; k <= n[r]; k++) {
                if (es_modelo && c != r && !free_ma(k, r + 1, c + 1)) continue;
                if (es_modelo && c == r && k <= ESC_MAXLAG && v_zma[r + 1][k]) continue;
                tipo[ncol] = 1; lag[ncol] = k; col[ncol++] = c;
            }
        X = es_mat(nt, ncol);
        y = (double *)calloc((size_t)nt, sizeof(double));
        b = (double *)calloc((size_t)(ncol > 0 ? ncol : 1), sizeof(double));
        u = (double *)calloc((size_t)nt, sizeof(double));
        for (j = 0; j < nt; j++) {
            int t = t0 + j;
            y[j] = z[r][t];
            for (k = 0; k < ncol; k++)
                X[j][k] = tipo[k] ? E[t - lag[k]][col[k]]
                        : (lag[k] == 0 ? z[col[k]][t] - E[t][col[k]] : z[col[k]][t - lag[k]]);
        }
        es_ls(X, y, nt, ncol, b, u);
        for (j = 0; j < nt; j++) U[j][r] = u[j];
        for (k = 0; k < ncol; k++) {
            if (tipo[k]) F->M[lag[k]][r][col[k]] = b[k];
            else         F->A[lag[k]][r][col[k]] = -b[k];
        }
        d += ncol;
        free(u); free(b); free(y); es_free_mat(X, nt);
    }
    for (r = 0; r < m; r++) for (c = 0; c < m; c++) F->M[0][r][c] = F->A[0][r][c];
    for (r = 0; r < m; r++)
        for (c = 0; c < m; c++) {
            double s = 0.0;
            for (j = 0; j < nt; j++) s += U[j][r] * U[j][c];
            S[r][c] = s / nt;
        }
    F->logdet = es_logdet(S, m);
    F->dim = d;  F->T = nt;
    for (r = 0; r < m; r++) for (c = 0; c < m; c++) F->S[r][c] = S[r][c];
    es_free_mat(z, m); es_free_mat(S, m); es_free_mat(U, nt);
}
static void es_free_fit(EsFit *F)
{
    if (F->A) es_free_ten(F->A, F->p + 1, es_m);
    if (F->M) es_free_ten(F->M, F->p + 1, es_m);
    F->A = F->M = NULL;
}
/* X = A_0^-1 B, con A_0 triangular inferior de diagonal unidad */
static void es_a0inv(double **A0, double **B, double **X, int m)
{
    int r, c, j;
    for (r = 0; r < m; r++)
        for (j = 0; j < m; j++) {
            double s = B[r][j];
            for (c = 0; c < r; c++) s -= A0[r][c] * X[c][j];
            X[r][j] = s;
        }
}
/* la mayor raiz inversa de I - SUM C_k z^k, C_k = -A_0^-1 P_k (k = 1..p) */
static double es_raiz(double **A0, double ***P, int p)
{
    int    m = es_m, k, r, c, i;
    real ***th, *wr, *wi, *wm;
    double **X, mx = 0.0;
    int    ifc = 0;
    if (p < 1) return 0.0;
    th = tensor(0, p, 1, m, 1, m);
    X = es_mat(m, m);
    for (k = 1; k <= p; k++) {
        es_a0inv(A0, P[k], X, m);
        for (r = 0; r < m; r++) for (c = 0; c < m; c++) th[k][r + 1][c + 1] = -X[r][c];
    }
    wr = vector(1, m * p); wi = vector(1, m * p); wm = vector(1, m * p);
    chekma(m, p, th, wr, wi, wm, &ifc);
    for (i = 1; i <= m * p; i++) if (wm[i] > mx) mx = wm[i];
    free_vector(wm, 1, m * p); free_vector(wi, 1, m * p); free_vector(wr, 1, m * p);
    es_free_mat(X, m); free_tensor(th, 0, p, 1, m, 1, m);
    return mx;
}
/* recoge dentro: A_k por lambda_A^k y M_k por lambda_M^k si la mayor raiz
   inversa pasa de rho. Devuelve las lambdas.                               */
static void es_recoge(EsFit *F, double rho, double *la, double *lm)
{
    int    m = es_m, k, r, c;
    double ra = es_raiz(F->A[0], F->A, F->p), rm = es_raiz(F->A[0], F->M, F->p);
    *la = ra >= rho ? rho / ra : 1.0;
    *lm = rm >= rho ? rho / rm : 1.0;
    for (k = 1; k <= F->p; k++)
        for (r = 0; r < m; r++)
            for (c = 0; c < m; c++) {
                F->A[k][r][c] *= pow(*la, k);
                F->M[k][r][c] *= pow(*lm, k);
            }
}
/* las innovaciones del modelo echelon sobre la serie centrada, valores
   previos a cero: A_0 e_t = A_0 w_t + SUM A'_k w_t-k - SUM M'_k e_t-k, con
   A' y M' las filas por sus factores                                       */
static void es_residuos(const EsFit *F, double **e)
{
    int    m = es_m, T = es_T, p = F->p, K, k, r, c, j, t, dmax = 0;
    double ***A, ***M, **X = es_mat(m, m);
    for (r = 0; r < m; r++) {
        if (es_df[r] > dmax) dmax = es_df[r];
        if (es_dg[r] > dmax) dmax = es_dg[r];
    }
    K = p + dmax;
    A = es_ten(K + 1, m);  M = es_ten(K + 1, m);
    for (k = 0; k <= K; k++)
        for (r = 0; r < m; r++)
            for (c = 0; c < m; c++) {
                double a = k <= p ? F->A[k][r][c] : 0.0, b = k <= p ? F->M[k][r][c] : 0.0;
                for (j = 1; j <= es_df[r] && j <= k; j++) if (k - j <= p) a -= es_f[r][j] * F->A[k - j][r][c];
                for (j = 1; j <= es_dg[r] && j <= k; j++) if (k - j <= p) b -= es_g[r][j] * F->M[k - j][r][c];
                A[k][r][c] = a;  M[k][r][c] = b;
            }
    for (k = 1; k <= K; k++) {                 /* A_0^-1 A'_k y A_0^-1 M'_k */
        es_a0inv(F->A[0], A[k], X, m);
        for (r = 0; r < m; r++) for (c = 0; c < m; c++) A[k][r][c] = X[r][c];
        es_a0inv(F->A[0], M[k], X, m);
        for (r = 0; r < m; r++) for (c = 0; c < m; c++) M[k][r][c] = X[r][c];
    }
    for (t = 0; t < T; t++)
        for (r = 0; r < m; r++) {
            double s = es_wd[t][r];
            for (k = 1; k <= K && k <= t; k++)
                for (c = 0; c < m; c++) s += A[k][r][c] * es_wd[t - k][c] - M[k][r][c] * e[t - k][c];
            e[t][r] = s;
        }
    es_free_ten(M, K + 1, m); es_free_ten(A, K + 1, m); es_free_mat(X, m);
}
/* etapa II y una ronda de la correccion de Hannan y Kavalieris */
static void es_refinada(const int *n, double **E1, int h, int t0, EsFit *F)
{
    EsFit    G;
    double   la, lm, **e = es_mat(es_T, es_m);
    int      t, c;
    memset(&G, 0, sizeof G);
    es_stage2(n, E1, t0, &G);
    es_recoge(&G, 0.95, &la, &lm);
    es_residuos(&G, e);
    for (t = 0; t < h; t++) for (c = 0; c < es_m; c++) e[t][c] = 0.0;   /* no se usan */
    es_free_fit(&G);
    es_stage2(n, e, t0, F);
    es_free_mat(e, es_T);
}
static double es_crit(const EsFit *F, int sc)
{
    double cT = sc ? log((double)F->T) : 2.0 * log(log((double)F->T));
    return F->logdet + cT * F->dim / F->T;
}

/* Los datos de las dos etapas: la serie estacionaria (centrada para la II),
   los factores de fila del estado actual, y la etapa I. Devuelve las
   innovaciones E[t][c] del VAR(h) largo (validas en t >= h).               */
static double **es_etapa1(double ***wraw_out, int *h_out, int *hmax_out, int *dmax_out)
{
    int     m = n_ser, T = n_stat, i, j, t, c, h, h_max, dmax = 0;
    double **wraw, **E, *aic;

    es_m = m;  es_T = T;
    wraw = es_mat(T, m);  es_wd = es_mat(T, m);
    for (c = 0; c < m; c++) {
        double s = 0.0;
        for (t = 0; t < T; t++) { wraw[t][c] = w[c + 1][t + 1]; s += wraw[t][c]; }
        for (t = 0; t < T; t++) es_wd[t][c] = wraw[t][c] - s / T;
    }
    es_f = (double **)calloc((size_t)m, sizeof(double *));  es_g = (double **)calloc((size_t)m, sizeof(double *));
    es_df = (int *)calloc((size_t)m, sizeof(int));          es_dg = (int *)calloc((size_t)m, sizeof(int));
    for (i = 0; i < m; i++) {
        struct Tusmodel M = v_parte(i + 1, 0);
        real *v;
        es_df[i] = total_ar_order(&M);  es_dg[i] = total_ma_order(&M);
        es_f[i] = (double *)calloc((size_t)es_df[i] + 2, sizeof(double));
        es_g[i] = (double *)calloc((size_t)es_dg[i] + 2, sizeof(double));
        if (es_df[i] > 0) { v = vector(1, es_df[i]); expand_ar_factors(&M, v, es_df[i]);
                            for (j = 1; j <= es_df[i]; j++) es_f[i][j] = v[j]; free_vector(v, 1, es_df[i]); }
        if (es_dg[i] > 0) { v = vector(1, es_dg[i]); expand_ma_factors(&M, v, es_dg[i]);
                            for (j = 1; j <= es_dg[i]; j++) es_g[i][j] = v[j]; free_vector(v, 1, es_dg[i]); }
        if (es_df[i] > dmax) dmax = es_df[i];
        if (es_dg[i] > dmax) dmax = es_dg[i];
    }
    h_max = (int)pow(log((double)T), 1.5);
    { int tope = (T - 1) / (m + 1) - 1; if (h_max > tope) h_max = tope; if (h_max < 1) h_max = 1; }
    aic = (double *)calloc((size_t)h_max + 1, sizeof(double));
    h = 1;
    for (i = 1; i <= h_max; i++) {
        aic[i] = es_var(wraw, i, h_max, NULL) + 2.0 * i * m * m / (double)(T - h_max);
        if (aic[i] < aic[h]) h = i;
    }
    free(aic);
    E = es_mat(T, m);
    es_var(wraw, h, h, E);
    *wraw_out = wraw;  *h_out = h;  *hmax_out = h_max;  *dmax_out = dmax;
    return E;
}
static void es_cierra(double **E, double **wraw)
{
    int i;
    es_free_mat(E, es_T);
    for (i = 0; i < es_m; i++) { free(es_f[i]); free(es_g[i]); }
    free(es_f); free(es_g); free(es_df); free(es_dg);
    es_free_mat(es_wd, es_T); es_free_mat(wraw, es_T);
}

/* La busqueda. Devuelve 0, o 1 si la muestra no da. Escribe la tabla.      */
static int ech_search(FILE *out, int n_max, int sc)
{
    int     m = n_ser, T = n_stat, h_max, h, t0, i, t, c, j, dmax = 0, nfit = 0, cap, lleno, f;
    double **wraw, **E, *aic, best;
    EsFit  *fits;
    int     elegido = -1, otro = -1;

    (void)t; (void)c;
    E = es_etapa1(&wraw, &h, &h_max, &dmax);
    aic = NULL;
    t0 = h + n_max + dmax;
    { int nn[ESC_MAX + 1]; for (i = 0; i < m; i++) nn[i] = n_max;
      if (T - t0 < 2 * es_dim(nn, m) / m + 10) {
          for (f = 0; f < 2; f++)
              fprintf(f ? out : stdout, "ERROR: %d observations are too few for a largest index of %d with a "
                                        "VAR(%d) first stage\n", T, n_max, h);
          return 1;
      } }
    cap = 1; lleno = 1;
    for (i = 0; i < m; i++) { if (cap > ES_FULL_MAX) { lleno = 0; break; } cap *= (n_max + 1); }
    if (cap > ES_FULL_MAX) lleno = 0;
    if (!lleno) cap = (n_max + 1) + m * (n_max + 1) + 2;
    fits = (EsFit *)calloc((size_t)cap, sizeof(EsFit));

    if (lleno) {
        int n[ESC_MAX + 1];
        for (i = 0; i < m; i++) n[i] = 0;
        for (;;) {
            es_refinada(n, E, h, t0, &fits[nfit++]);
            for (i = m - 1; i >= 0; i--) { if (++n[i] <= n_max) break; n[i] = 0; }   /* el ultimo corre mas */
            if (i < 0) break;
        }
        for (i = 0; i < nfit; i++)
            if (elegido < 0 || es_crit(&fits[i], sc) < es_crit(&fits[elegido], sc)) elegido = i;
    } else {                                   /* Hannan y Kavalieris: dos pasos */
        int n[ESC_MAX + 1], cur;
        for (j = 0; j <= n_max; j++) {
            for (i = 0; i < m; i++) n[i] = j;
            es_refinada(n, E, h, t0, &fits[nfit++]);
            if (elegido < 0 || es_crit(&fits[nfit - 1], sc) < es_crit(&fits[elegido], sc)) elegido = nfit - 1;
        }
        cur = elegido;
        for (i = 0; i < m; i++)
            for (;;) {
                if (fits[cur].n[i] == 0) break;
                int ya = -1, l;
                for (j = 0; j < m; j++) n[j] = fits[cur].n[j];
                n[i]--;
                for (l = 0; l < nfit && ya < 0; l++) {       /* un candidato no se ajusta dos veces */
                    int ig = 1;
                    for (j = 0; j < m; j++) if (fits[l].n[j] != n[j]) ig = 0;
                    if (ig) ya = l;
                }
                if (ya < 0) { es_refinada(n, E, h, t0, &fits[nfit]); ya = nfit++; }
                if (es_crit(&fits[ya], sc) < es_crit(&fits[cur], sc)) cur = ya; else break;
            }
        elegido = cur;
    }
    for (i = 0; i < nfit; i++)
        if (otro < 0 || es_crit(&fits[i], !sc) < es_crit(&fits[otro], !sc)) otro = i;
    best = es_crit(&fits[elegido], sc);
    (void)best;

    for (f = 0; f < 2; f++) {
        FILE *o = f ? out : stdout;
        fprintf(o, "=============================================================\n");
        fprintf(o, "  THE ECHELON INDEX SEARCH (Lutkepohl and Poskitt 1996, stages I-II)\n");
        fprintf(o, "=============================================================\n");
        fprintf(o, "  Stage I: a long VAR(%d) by least squares, its order by AIC up to %d.\n", h, h_max);
        fprintf(o, "  Stage II: least squares per candidate, with one round of Hannan and\n");
        fprintf(o, "  Kavalieris' correction, on the same %d observations for all of them.\n", fits[0].T);
        if (dmax > 0)
            fprintf(o, "  Each row is fitted on the series filtered by its row factor (the\n"
                       "  univariate model's seasonal and fixed-frequency factors).\n");
        fprintf(o, "  %s, %d candidates.\n", lleno ? "Every candidate" : "Hannan and Kavalieris' two steps", nfit);
        fprintf(o, "  Series, in order:");
        for (i = 1; i <= m; i++) fprintf(o, " %s", Ts[i].name);
        fprintf(o, "   (the order is part of the echelon form)\n\n");
        fprintf(o, "  %-14s %5s %14s %14s %14s\n", "indices", "dim", "log|S|", "HQ", "SC");
        for (i = 0; i < nfit; i++) {
            char nb[64] = "";
            for (j = 0; j < m; j++) snprintf(nb + strlen(nb), sizeof nb - strlen(nb), "%s%d", j ? "," : "", fits[i].n[j]);
            fprintf(o, "  %-14s %5d %14.6f %14.6f %14.6f%s\n", nb, fits[i].dim, fits[i].logdet,
                    es_crit(&fits[i], 0), es_crit(&fits[i], 1), i == elegido ? "  <-" : "");
        }
        fprintf(o, "\n  Chosen by %s:", sc ? "SC" : "HQ");
        for (j = 0; j < m; j++) fprintf(o, "%s%d", j ? "," : " ", fits[elegido].n[j]);
        fprintf(o, "      (by %s:", sc ? "HQ" : "SC");
        for (j = 0; j < m; j++) fprintf(o, "%s%d", j ? "," : " ", fits[otro].n[j]);
        fprintf(o, ")\n  This is an identification by least squares: the model is then fitted by\n"
                   "  exact maximum likelihood with -echelon and those indices.\n\n");
    }
    for (i = 0; i < nfit; i++) es_free_fit(&fits[i]);
    free(fits); free(aic);
    es_cierra(E, wraw);
    return 0;
}

static void orders(int *P, int *Q)
{
    int a;
    *P = cx_p; *Q = cx_q;
    for (a = 1; a <= act_m; a++) {
        int i = act[a], da = ar_deg[i], dm = ma_deg[i];
        if (v_on) {              /* la fila entera por su factor */
            da = (v_reg_ar[i] > cx_p ? v_reg_ar[i] : cx_p) + v_fac_ar[i];
            dm = (v_reg_ma[i] > cx_q ? v_reg_ma[i] : cx_q) + v_fac_ma[i];
        }
        if (da > *P) *P = da;
        if (dm > *Q) *Q = dm;
    }
    if (*P < 1) *P = 1;             /* elf exige p >= 1 */
}

static void shootx_esc(real *x, struct Tvarma *armax, int *ifaultx, int firstx, int lastx)
{
    int m = act_m, P, Q, a, b, k, t;
    real c0[ESC_MAX + 1][ESC_MAX + 1];           /* C_0 de las series activas */

    *ifaultx = 0;
    orders(&P, &Q);

    /* Los ordenes dependen solo de la ESTRUCTURA, no de los valores: se aloja
       una vez, en firstx, antes de mirar si el punto es admisible, para que un
       punto rechazado no deje nada a medio alojar.                           */
    if (firstx) {
        armax->m = m;  armax->n = n_stat;  armax->p = P;  armax->q = Q;
        armax->mu    = vector(1, m);
        armax->phi   = tensor(0, P, 1, m, 1, m);
        armax->theta = tensor(0, Q, 1, m, 1, m);
        armax->qq    = matrix(1, m, 1, m);
        armax->w     = matrix(1, n_stat, 1, m);
        armax->a     = matrix(1, n_stat, 1, m);
        for (t = 1; t <= n_stat; t++)
            for (a = 1; a <= m; a++) armax->a[t][a] = 0.0;
    }

    esc_unpack(x);

    {   int libres = 0;
        for (a = 1; a <= m; a++) {
            int nd = det_n(act[a]);
            if (invalid_fixfreq(&Tm[act[a]])) { *ifaultx = 6; goto fin; }
            if (nd > 0 && unstable_delta(&Tm[act[a]])) { *ifaultx = 6; goto fin; }
            libres += nd;
        }
        /* Con deterministas libres, la serie estacionaria depende de los
           parametros: se rehace. n_stat no cambia (depende solo del operador). */
        if (libres > 0 && build_stationary() != 0) { *ifaultx = 6; goto fin; }
    }

    for (k = 0; k <= P; k++)
        for (a = 1; a <= m; a++)
            for (b = 1; b <= m; b++) armax->phi[k][a][b] = (k == 0 && a == b) ? 1.0 : 0.0;
    for (k = 0; k <= Q; k++)
        for (a = 1; a <= m; a++)
            for (b = 1; b <= m; b++) armax->theta[k][a][b] = (k == 0 && a == b) ? 1.0 : 0.0;

    /* La diagonal: el modelo de cada .pre, factores multiplicados. La
       convencion de elf y la de expand_*_factors son la misma,
       Phi(B) = I - SUM phi_k B^k.                                         */
    for (a = 1; a <= m; a++) {
        int i = act[a];
        /* en la ruta V la diagonal lleva solo lo regular; el resto del
           univariante multiplica la fila mas abajo */
        struct Tusmodel M = v_on ? v_parte(i, 1) : Tm[i];
        int da = v_on ? v_reg_ar[i] : ar_deg[i], dm = v_on ? v_reg_ma[i] : ma_deg[i];
        if (da > 0) {
            real *ph = vector(1, da);
            expand_ar_factors(&M, ph, da);
            for (k = 1; k <= da; k++) armax->phi[k][a][a] = ph[k];
            free_vector(ph, 1, da);
        }
        if (dm > 0) {
            real *th = vector(1, dm);
            expand_ma_factors(&M, th, dm);
            for (k = 1; k <= dm; k++) armax->theta[k][a][a] = th[k];
            free_vector(th, 1, dm);
        }
    }
    /* Fuera de la diagonal: la dinamica cruzada. */
    for (k = 1; k <= cx_p; k++)
        for (a = 1; a <= m; a++)
            for (b = 1; b <= m; b++)
                if (a != b && free_ar(k, act[a], act[b])) armax->phi[k][a][b] = cAR[k][act[a]][act[b]];
    for (k = 1; k <= cx_q; k++)
        for (a = 1; a <= m; a++)
            for (b = 1; b <= m; b++)
                if (a != b && free_ma(k, act[a], act[b])) armax->theta[k][a][b] = cMA[k][act[a]][act[b]];

    /* La forma echelon: C_0 de las series activas (0 si no hay A_0). */
    if (ech_on)
        for (a = 1; a <= m; a++) {
            c0[a][0] = 0.0;
            for (b = 1; b <= m; b++)
                c0[a][b] = free_a0(act[a], act[b]) ? cA0[act[a]][act[b]] : 0.0;
        }
    /* La ruta V: cada fila por su factor (lo anual y lo de frecuencia fija
       del univariante).                                                    */
    if (v_on)
        for (a = 1; a <= m; a++) {
            int i = act[a];
            struct Tusmodel M = v_parte(i, 0);
            if (v_fac_ar[i] > 0) {
                real *f = vector(1, v_fac_ar[i]);
                expand_ar_factors(&M, f, v_fac_ar[i]);
                v_fila_por_factor(armax->phi, a, m, v_reg_ar[i] > cx_p ? v_reg_ar[i] : cx_p,
                                  f, v_fac_ar[i], ech_on ? c0[a] : NULL);
                free_vector(f, 1, v_fac_ar[i]);
            }
            if (v_fac_ma[i] > 0) {
                real *g = vector(1, v_fac_ma[i]);
                expand_ma_factors(&M, g, v_fac_ma[i]);
                v_fila_por_factor(armax->theta, a, m, v_reg_ma[i] > cx_q ? v_reg_ma[i] : cx_q,
                                  g, v_fac_ma[i], ech_on ? c0[a] : NULL);
                free_vector(g, 1, v_fac_ma[i]);
            }
        }

    /* La forma echelon: a la forma estandar, Phi_k = A_0^-1 Phi~_k y
       Theta_k = A_0^-1 Theta~_k. A_0 = I - C_0 es triangular de diagonal
       unidad: fila a fila, X[a] = X~[a] + SUM_{b<a} C_0[a][b] X[b].        */
    if (ech_on)
        for (a = 2; a <= m; a++)
            for (b = 1; b < a; b++) {
                int c;
                if (c0[a][b] == 0.0) continue;
                for (k = 1; k <= P; k++)
                    for (c = 1; c <= m; c++) armax->phi[k][a][c] += c0[a][b] * armax->phi[k][b][c];
                for (k = 1; k <= Q; k++)
                    for (c = 1; c <= m; c++) armax->theta[k][a][c] += c0[a][b] * armax->theta[k][b][c];
            }

    /* Q, normalizada con Q_11 = 1: est() concentra sigma2, y Q -> cQ es una
       direccion exactamente plana (Mauricio 1995, ec. 2.1).               */
    for (a = 1; a <= m; a++)
        for (b = 1; b <= m; b++) armax->qq[a][b] = 0.0;
    armax->qq[1][1] = 1.0;
    for (a = 2; a <= m; a++) armax->qq[a][a] = exp(lvar[act[a]]);
    if (!cx_diagcov)
        for (a = 2; a <= m; a++)
            for (b = 1; b < a; b++)
                armax->qq[a][b] = armax->qq[b][a] = qcov[act[a]][act[b]];

    for (a = 1; a <= m; a++) armax->mu[a] = mu[act[a]];
    for (t = 1; t <= n_stat; t++)
        for (a = 1; a <= m; a++) armax->w[t][a] = w[act[a]][t];
    armax->sigma2 = 1.0;

    if (Q > 0) {                              /* invertibilidad del MA */
        real *wr = vector(1, m * Q), *wi = vector(1, m * Q), *wm = vector(1, m * Q);
        int ifc = 0;
        chekma(m, Q, armax->theta, wr, wi, wm, &ifc);
        free_vector(wm, 1, m * Q); free_vector(wi, 1, m * Q); free_vector(wr, 1, m * Q);
        if (ifc != 0) *ifaultx = 4;
    }

fin:
    if (lastx) {
        free_matrix(armax->a, 1, armax->n, 1, armax->m);
        free_matrix(armax->w, 1, armax->n, 1, armax->m);
        free_matrix(armax->qq, 1, armax->m, 1, armax->m);
        free_tensor(armax->theta, 0, armax->q, 1, armax->m, 1, armax->m);
        free_tensor(armax->phi, 0, armax->p, 1, armax->m, 1, armax->m);
        free_vector(armax->mu, 1, armax->m);
    }
}

/* ------------------------------------------------------------------------- */
/*  Un ajuste                                                                */
/* ------------------------------------------------------------------------- */
typedef struct {
    int    npar;
    real  *x, *dev, **cov;
    real   logL, sigma2;
    int    ifault;
    int    se_how;             /* EST_SE_*: which Hessian gave dev and cov */
    struct Tvarma vm;          /* vivo tras fit_run hasta fit_free */
} Fit;

static real xitol_met = 1.0e-3;

/* La verosimilitud exacta en el punto actual, sin optimizar: la misma cuenta
   que est() hace al final.                                                  */
static void fit_eval(Fit *F)
{
    const real LOG2PI = 1.837877066;
    struct Tvarma *v = &F->vm;
    real pi1, pi2, pi3;
    int  ifault = 0;
    varma_lik(v->m, v->n, v->p, v->q, v->mu, v->phi, v->theta, v->qq, v->w, 1.0,
        xitol_met, TRUE, v->a, &pi1, &pi2, &pi3, &ifault);
    F->ifault = ifault;
    if (ifault) return;
    F->logL = -0.5 * v->m * v->n * (LOG2PI - log(v->m) - log(v->n) + 1.0)
              - 0.5 * v->n * (v->m * log(pi1) + log(pi2));
    F->sigma2 = pi1 / (v->n * v->m);
}

/* optimize = 0: solo evaluar en el estado actual.  El estado (Tm, mu, la
   parte cruzada, Q) queda en el optimo al volver.                          */
static void fit_run(Fit *F, int optimize)
{
    int ifault = 0, np;
    memset(F, 0, sizeof *F);
    F->npar = esc_npar();
    np = F->npar > 0 ? F->npar : 1;
    F->x   = vector(1, np);
    F->dev = vector(1, np);
    F->cov = matrix(1, np, 1, np);
    esc_pack(F->x);

    shootx_esc(F->x, &F->vm, &ifault, 1, 0);
    F->vm.xitol = xitol_met;
    if (ifault) { F->ifault = ifault; return; }

    if (optimize && F->npar > 0) {
        est(&shootx_esc, F->npar, F->x, F->dev, F->cov, 500, 10, 1.0e-7, 1.0e-7,
            xitol_met, F->vm.a, &F->sigma2, &F->logL, &ifault);
        F->ifault = ifault;
        F->se_how = est_se_how;
        if (ifault) return;
    }
    /* El estado global quedo en el ultimo punto que probo el optimizador, que
       no tiene por que ser el optimo: se reponen los parametros finales y se
       recalcula todo ahi, residuos incluidos.                              */
    shootx_esc(F->x, &F->vm, &ifault, 0, 0);
    if (ifault) { F->ifault = ifault; return; }
    fit_eval(F);
}

/* ------------------------------------------------------------------------- */
/*  -start ols: el arranque de drvarma original (init_varma, la via .inp),   */
/*  llevado a la escalera. La misma aritmetica que Ladder(start="ols") del   */
/*  porte (drvarma-python, ladder._ols_start).                               */
/*                                                                           */
/*  Minimos cuadrados del VAR(p), sin constante, sobre las series            */
/*  estacionarias centradas del estado diagonal: Phi_1..Phi_p, los residuos  */
/*  E en t = p+1..n y S = E'E / (n - p). El arranque de la escalera MAS los  */
/*  parametros de la RELACION:                                               */
/*   - los AR cruzados libres toman Phi_k[a][b] (Box y Jenkins:              */
/*     Phi(B) = I - SUM Phi_k B^k: el cruzado es +Phi_k); -links y -zero se  */
/*     respetan (free_ar);                                                   */
/*   - las covarianzas de Q (Q_11 = 1, Mauricio 1995, ec. 2.1) toman la      */
/*     CORRELACION de los residuos, como el original, en la normalizacion de */
/*     la escalera: q_ab = r_ab sqrt(Q_aa Q_bb), con las Q_aa del ajuste     */
/*     diagonal (las varianzas de cada serie, la puerta, no se tocan);       */
/*   - los MA cruzados, en cero, como el original.                           */
/*  Los modelos PROPIOS no se siembran nunca: el principio de la escalera    */
/*  (los univariantes primero, de art y mtram) vale en las dos rutas. En la  */
/*  U son los optimos de los .pre; en la V (especificaciones con el ARMA     */
/*  propio a cero) los estima antes el ajuste diagonal, por MV exacta. Los   */
/*  minimos cuadrados solo anaden la relacion. Si elf no acepta el punto, se */
/*  vuelve a cero y se dice (start_used).                                    */
/*  Desde cero, la MV exacta se para a las 2 iteraciones por debajo del VAR  */
/*  anidado en el VAR monetario de EE. UU. (drvarma-python BUG-0027).        */
/* ------------------------------------------------------------------------- */
/* -1: sin -start. El arranque por defecto es ols con q = 0 (un VAR sobre las
   diagonales) y cero con MA: con q = 0 llega al optimo en todos los casos
   medidos y saca del estancamiento a los VAR monetarios (BUG-0027); con
   q > 0 no siembra el MA y no es mejor que cero (tests/rutas). Cambiara
   cuando exista la siembra del MA.                                         */
static int  opt_start_ols = -1;
static char start_used[96] = "zero";

static void fit_free(Fit *F);

static void start_ols(void)
{
    int  m = act_m, p = cx_p, n = n_stat, np = act_m * cx_p, T = n_stat - cx_p;
    int  a, b, k, t, i, r, *ip;
    real *mean, **Z, **ZtZ, *rhs, **B, **S;
    Fit  chk;

    if (p < 1 || T <= np + 1) {
        snprintf(start_used, sizeof start_used,
                 "zero (no least squares: p = 0 or too few observations)");
        return;
    }
    mean = vector(1, m);
    for (a = 1; a <= m; a++) {
        real s = 0.0;
        for (t = 1; t <= n; t++) s += w[act[a]][t];
        mean[a] = s / n;
    }
    Z = matrix(1, T, 1, np);
    for (r = 1; r <= T; r++)
        for (k = 1; k <= p; k++)
            for (b = 1; b <= m; b++)
                Z[r][(k - 1) * m + b] = w[act[b]][r + p - k] - mean[b];
    ZtZ = matrix(1, np, 1, np);
    for (i = 1; i <= np; i++)
        for (k = 1; k <= np; k++) {
            real s = 0.0;
            for (r = 1; r <= T; r++) s += Z[r][i] * Z[r][k];
            ZtZ[i][k] = s;
        }
    ip = ivector(1, np);
    ludcp(ZtZ, np, ip);
    B = matrix(1, np, 1, m);          /* B[(k-1)m+b][a] = Phi_k[a][b] */
    rhs = vector(1, np);
    for (a = 1; a <= m; a++) {
        for (i = 1; i <= np; i++) {
            real s = 0.0;
            for (r = 1; r <= T; r++) s += Z[r][i] * (w[act[a]][r + p] - mean[a]);
            rhs[i] = s;
        }
        lusol(ZtZ, rhs, np, ip);
        for (i = 1; i <= np; i++) B[i][a] = rhs[i];
    }
    S = matrix(1, m, 1, m);
    for (a = 1; a <= m; a++)
        for (b = 1; b <= m; b++) S[a][b] = 0.0;
    for (r = 1; r <= T; r++) {
        real e[ESC_MAX + 1];
        for (a = 1; a <= m; a++) {
            real s = w[act[a]][r + p] - mean[a];
            for (i = 1; i <= np; i++) s -= Z[r][i] * B[i][a];
            e[a] = s;
        }
        for (a = 1; a <= m; a++)
            for (b = 1; b <= m; b++) S[a][b] += e[a] * e[b];
    }
    for (a = 1; a <= m; a++)
        for (b = 1; b <= m; b++) S[a][b] /= T;

    /* Los AR cruzados libres. */
    for (k = 1; k <= p; k++)
        for (a = 1; a <= m; a++)
            for (b = 1; b <= m; b++)
                if (a != b && free_ar(k, act[a], act[b]))
                    cAR[k][act[a]][act[b]] = B[(k - 1) * m + b][a];
    if (!cx_diagcov)
        for (a = 2; a <= m; a++)
            for (b = 1; b < a; b++) {
                real qa = exp(lvar[act[a]]), qb = (b == 1) ? 1.0 : exp(lvar[act[b]]);
                real rr = S[a][b] / sqrt(S[a][a] * S[b][b]);
                qcov[act[a]][act[b]] = rr * sqrt(qa * qb);
            }

    /* Admisible?  Si elf no acepta el punto, a cero, como el porte. */
    fit_run(&chk, 0);
    fit_free(&chk);              /* antes de reponer: fit_free desempaqueta chk.x */
    if (chk.ifault) {
        for (k = 0; k <= ESC_MAXLAG; k++)
            for (a = 0; a <= ESC_MAX; a++)
                for (b = 0; b <= ESC_MAX; b++) cAR[k][a][b] = 0.0;
        for (a = 0; a <= ESC_MAX; a++)
            for (b = 0; b <= ESC_MAX; b++) qcov[a][b] = 0.0;
        snprintf(start_used, sizeof start_used, "zero (the ols start was not admissible)");
    } else
        snprintf(start_used, sizeof start_used, "ols");

    free_matrix(S, 1, m, 1, m);
    free_vector(rhs, 1, np);
    free_matrix(B, 1, np, 1, m);
    free_ivector(ip, 1, np);
    free_matrix(ZtZ, 1, np, 1, np);
    free_matrix(Z, 1, T, 1, np);
    free_vector(mean, 1, m);
}

/* ------------------------------------------------------------------------- */
/*  La siembra de la ruta V: TODA la matriz por minimos cuadrados.           */
/*                                                                           */
/*  En la V lo propio es del vector, asi que el arranque de drvarma original */
/*  (init_varma) vale entero: los minimos cuadrados del VAR(p) dan la        */
/*  diagonal y los cruzados. El MA, propio y cruzado, a cero, como alli.     */
/*  Con factor de fila, la ecuacion de la fila a es                          */
/*      F_a(B) w_a,t = SUM_k SUM_b Phi_k[a,b] F_a(B) w_b,t-k + G_a(B) a_a,t  */
/*  y se ajusta sobre TODAS las series filtradas por (F_a/G_a)(B), el factor */
/*  de ESA fila (valores previos a la muestra a cero, como la etapa II del   */
/*  porte).                                                                  */
/*  Q: las varianzas son las del ajuste diagonal exacto, la V sin cruzados   */
/*  (lvar no se toca); las covarianzas, la correlacion de los residuos de    */
/*  minimos cuadrados en esa escala, como start_ols en la U.                 */
/*  Si elf no acepta el punto se vuelve a la V sin cruzados (lo propio a su  */
/*  optimo diagonal, los cruzados a cero) y se dice.                         */
/* ------------------------------------------------------------------------- */
static void start_ols_v(void)
{
    int  m = act_m, p = cx_p, q = cx_q, n = n_stat, np = act_m * cx_p;
    int  a, b, k, t, i, r, j, *ip, dmax = 0, t0, T, rechazado;
    real *mean, **Z, **ZtZ, *rhs, **E, **S, **z, **ar0, **ma0;
    Fit  chk;

    for (a = 1; a <= m; a++) {
        if (v_fac_ar[act[a]] > dmax) dmax = v_fac_ar[act[a]];
    }
    t0 = p + dmax + 1;  T = n - t0 + 1;
    if (p < 1 || T <= np + 1) {
        snprintf(start_used, sizeof start_used,
                 "zero (no least squares: p = 0 or too few observations)");
        return;
    }
    /* lo propio como esta (el optimo de la V sin cruzados), por si hay que volver */
    ar0 = matrix(1, m, 0, p);  ma0 = matrix(1, m, 0, q > 0 ? q : 1);
    for (a = 1; a <= m; a++) {
        for (k = 1; k <= v_reg_ar[act[a]]; k++) ar0[a][k] = Tm[act[a]].Ar1[1][k];
        for (k = 1; k <= v_reg_ma[act[a]]; k++) ma0[a][k] = Tm[act[a]].Ma1[1][k];
    }
    mean = vector(1, m);
    for (a = 1; a <= m; a++) {
        real s = 0.0;
        for (t = 1; t <= n; t++) s += w[act[a]][t];
        mean[a] = s / n;
    }
    z   = matrix(1, m, 1, n);
    Z   = matrix(1, T, 1, np);
    ZtZ = matrix(1, np, 1, np);
    ip  = ivector(1, np);
    rhs = vector(1, np);
    E   = matrix(1, T, 1, m);
    for (a = 1; a <= m; a++) {
        struct Tusmodel M = v_parte(act[a], 0);
        int   da = v_fac_ar[act[a]], dg = v_fac_ma[act[a]];
        real *f = vector(0, da > 0 ? da : 1), *g = vector(0, dg > 0 ? dg : 1);
        if (da > 0) expand_ar_factors(&M, f, da);
        if (dg > 0) expand_ma_factors(&M, g, dg);
        for (b = 1; b <= m; b++)                     /* (F_a/G_a)(B) de cada serie */
            for (t = 1; t <= n; t++) {
                real s = w[act[b]][t] - mean[b];
                for (j = 1; j <= da && j < t; j++) s -= f[j] * (w[act[b]][t - j] - mean[b]);
                for (j = 1; j <= dg && j < t; j++) s += g[j] * z[b][t - j];
                z[b][t] = s;
            }
        free_vector(g, 0, dg > 0 ? dg : 1);  free_vector(f, 0, da > 0 ? da : 1);
        for (r = 1; r <= T; r++)
            for (k = 1; k <= p; k++)
                for (b = 1; b <= m; b++) Z[r][(k - 1) * m + b] = z[b][t0 + r - 1 - k];
        for (i = 1; i <= np; i++)
            for (k = 1; k <= np; k++) {
                real s = 0.0;
                for (r = 1; r <= T; r++) s += Z[r][i] * Z[r][k];
                ZtZ[i][k] = s;
            }
        ludcp(ZtZ, np, ip);
        for (i = 1; i <= np; i++) {
            real s = 0.0;
            for (r = 1; r <= T; r++) s += Z[r][i] * z[a][t0 + r - 1];
            rhs[i] = s;
        }
        lusol(ZtZ, rhs, np, ip);
        for (r = 1; r <= T; r++) {
            real s = z[a][t0 + r - 1];
            for (i = 1; i <= np; i++) s -= Z[r][i] * rhs[i];
            E[r][a] = s;
        }
        for (k = 1; k <= p; k++)
            for (b = 1; b <= m; b++) {
                real c = rhs[(k - 1) * m + b];
                if (b == a) { if (k <= v_reg_ar[act[a]] && Tm[act[a]].Ia1[1][k]) Tm[act[a]].Ar1[1][k] = c; }
                else if (free_ar(k, act[a], act[b])) cAR[k][act[a]][act[b]] = c;
            }
        for (k = 1; k <= v_reg_ma[act[a]]; k++) Tm[act[a]].Ma1[1][k] = 0.0;
    }
    S = matrix(1, m, 1, m);
    for (a = 1; a <= m; a++)
        for (b = 1; b <= m; b++) {
            real s = 0.0;
            for (r = 1; r <= T; r++) s += E[r][a] * E[r][b];
            S[a][b] = s / T;
        }
    if (!cx_diagcov)
        for (a = 2; a <= m; a++)
            for (b = 1; b < a; b++) {
                real qa = exp(lvar[act[a]]), qb = (b == 1) ? 1.0 : exp(lvar[act[b]]);
                qcov[act[a]][act[b]] = S[a][b] / sqrt(S[a][a] * S[b][b]) * sqrt(qa * qb);
            }

    fit_run(&chk, 0);
    rechazado = chk.ifault;
    fit_free(&chk);                 /* antes de reponer (BUG-0006) */
    if (rechazado) {
        for (a = 1; a <= m; a++) {
            for (k = 1; k <= v_reg_ar[act[a]]; k++) Tm[act[a]].Ar1[1][k] = ar0[a][k];
            for (k = 1; k <= v_reg_ma[act[a]]; k++) Tm[act[a]].Ma1[1][k] = ma0[a][k];
        }
        for (k = 0; k <= ESC_MAXLAG; k++)
            for (a = 0; a <= ESC_MAX; a++)
                for (b = 0; b <= ESC_MAX; b++) cAR[k][a][b] = 0.0;
        for (a = 0; a <= ESC_MAX; a++)
            for (b = 0; b <= ESC_MAX; b++) qcov[a][b] = 0.0;
        snprintf(start_used, sizeof start_used,
                 "the model without cross terms (the ols start was not admissible)");
    } else
        snprintf(start_used, sizeof start_used, "ols, the whole matrix");

    free_matrix(S, 1, m, 1, m);
    free_matrix(E, 1, T, 1, m);
    free_vector(rhs, 1, np);
    free_ivector(ip, 1, np);
    free_matrix(ZtZ, 1, np, 1, np);
    free_matrix(Z, 1, T, 1, np);
    free_matrix(z, 1, m, 1, n);
    free_vector(mean, 1, m);
    free_matrix(ma0, 1, m, 0, q > 0 ? q : 1);
    free_matrix(ar0, 1, m, 0, p);
}

/* ------------------------------------------------------------------------- */
/*  -factortest: EL CONTRASTE DEL FACTOR DE FILA (ruta V)                    */
/*                                                                           */
/*  Multiplicar la fila por los factores anuales y de frecuencia fija del    */
/*  univariante es una RESTRICCION: es la salvedad de Tiao y Box a mezclar   */
/*  operadores univariantes con el modelo conjunto. Se contrasta por LR      */
/*  contra las mismas filas con la restriccion quitada: toda posicion que el */
/*  producto F_a(B)[Phi(B)]_a (y G_a(B)[Theta(B)]_a) alcanza, propia y       */
/*  cruzada, libre. La alternativa arranca en la expansion del optimo, donde */
/*  su verosimilitud es la misma, asi que el LR es >= 0 por construccion.    */
/*  Es factor_test() del porte (drvarma-python, la forma echelon).           */
/*                                                                           */
/*  Reescribe la estructura del modelo (lo propio pasa a ser un operador     */
/*  regular con huecos, los cruzados ganan retardos): por eso va lo ultimo.  */
/* ------------------------------------------------------------------------- */
static int    opt_factortest = 0;
static const char *fault_msg(int ifault);
static real ***ft_phi = NULL, ***ft_th = NULL;
static int    ft_P, ft_Q, ft_npar;
static real   ft_logL;

/* los retardos donde el factor (AR si ar, MA si no) de la serie i puede no
   ser cero, el 0 incluido; s[0..ESC_MAXLAG] */
static void ft_soporte(int i, int ar, unsigned char *s)
{
    struct Tusmodel *M = &Tm[i];
    unsigned char n[ESC_MAXLAG + 1];
    int f, j, l, nf = ar ? M->NumAr2 : M->NumMa2, *ord = ar ? M->p2 : M->q2;
    int nff = ar ? M->NumAr1f : M->NumMa1f;
    memset(s, 0, ESC_MAXLAG + 1);  s[0] = 1;
    for (f = 1; f <= nf; f++) {                   /* anuales: multiplos del periodo */
        memset(n, 0, sizeof n);
        for (l = 0; l <= ESC_MAXLAG; l++)
            if (s[l])
                for (j = 0; j <= ord[f]; j++)
                    if (l + j * M->sper <= ESC_MAXLAG) n[l + j * M->sper] = 1;
        memcpy(s, n, sizeof n);
    }
    for (f = 1; f <= nff; f++) {                  /* frecuencia fija: grado 2 */
        memset(n, 0, sizeof n);
        for (l = 0; l <= ESC_MAXLAG; l++)
            if (s[l])
                for (j = 0; j <= 2; j++)
                    if (l + j <= ESC_MAXLAG) n[l + j] = 1;
        memcpy(s, n, sizeof n);
    }
}

static void v_factor_test(FILE *out, int p, int q)
{
    static unsigned char sA[ESC_MAX + 1][ESC_MAXLAG + 1], sM[ESC_MAX + 1][ESC_MAXLAG + 1];
    static unsigned char oA[ESC_MAXLAG + 1][ESC_MAX + 1][ESC_MAX + 1], oM[ESC_MAXLAG + 1][ESC_MAX + 1][ESC_MAX + 1];
    static unsigned char a0[ESC_MAX + 1][ESC_MAX + 1];
    int  m = n_ser, i, j, k, l, jj, dA = 0, dM = 0, hay = 0, f;
    Fit  A;
    real logL0, logL1, lr, pv;
    int  np1, df;

    for (i = 1; i <= m; i++) {
        if (v_fac_ar[i] > dA) dA = v_fac_ar[i];
        if (v_fac_ma[i] > dM) dM = v_fac_ma[i];
        if (v_fac_ar[i] > 0 || v_fac_ma[i] > 0) hay = 1;
    }
    for (f = 0; f < 2; f++) {
        FILE *o = f ? out : stdout;
        fprintf(o, "\n=============================================================\n");
        fprintf(o, "  THE ROW FACTOR (-factortest)\n");
        fprintf(o, "=============================================================\n");
        if (!hay)
            fprintf(o, "  No series has seasonal or fixed-frequency factors: there is no\n"
                       "  row factor to test.\n");
        else if (p + dA > ESC_MAXLAG || q + dM > ESC_MAXLAG)
            fprintf(o, "  Not available: the rows without the factor need cross lags up to\n"
                       "  %d, beyond the engine's %d.\n", p + dA > q + dM ? p + dA : q + dM, ESC_MAXLAG);
    }
    if (!hay || p + dA > ESC_MAXLAG || q + dM > ESC_MAXLAG) return;

    /* La forma echelon: las filas ANTES de A_0^-1. El ajuste guardo la forma
       estandar, Phi = A_0^-1 Phi~: Phi~ = (I - C_0) Phi. La alternativa es
       otra forma echelon con el mismo A_0, y lo que se libera son las
       posiciones de esas filas.                                            */
    if (ech_on) {
        real c0[ESC_MAX + 1][ESC_MAX + 1];
        for (i = 1; i <= m; i++)
            for (j = 1; j <= m; j++) c0[i][j] = (i != j && free_a0(i, j)) ? cA0[i][j] : 0.0;
        for (i = m; i >= 2; i--)                    /* de abajo arriba: usa filas aun sin tocar */
            for (j = 1; j < i; j++) {
                if (c0[i][j] == 0.0) continue;
                for (k = 1; k <= ft_P; k++) for (l = 1; l <= m; l++) ft_phi[k][i][l] -= c0[i][j] * ft_phi[k][j][l];
                for (k = 1; k <= ft_Q; k++) for (l = 1; l <= m; l++) ft_th[k][i][l]  -= c0[i][j] * ft_th[k][j][l];
            }
    }
    for (i = 1; i <= m; i++) { ft_soporte(i, 1, sA[i]); ft_soporte(i, 0, sM[i]); }
    for (k = 0; k <= ESC_MAXLAG; k++)
        for (i = 1; i <= m; i++)
            for (j = 1; j <= m; j++) {
                oA[k][i][j] = (unsigned char)(k >= 1 && k <= p && i != j && free_ar(k, i, j));
                oM[k][i][j] = (unsigned char)(k >= 1 && k <= q && i != j && free_ma(k, i, j));
            }
    for (i = 1; i <= m; i++)
        for (j = 1; j <= m; j++) a0[i][j] = (unsigned char)(i != j && free_a0(i, j));
    /* lo propio: un operador regular con los retardos que el producto alcanza */
    for (i = 1; i <= m; i++) {
        int   DA = v_reg_ar[i] + v_fac_ar[i], DM = v_reg_ma[i] + v_fac_ma[i];
        real *ca = vector(0, DA > 0 ? DA : 1), *cm = vector(0, DM > 0 ? DM : 1);
        for (l = 1; l <= DA; l++) ca[l] = (l <= ft_P) ? ft_phi[l][i][i] : 0.0;
        for (l = 1; l <= DM; l++) cm[l] = (l <= ft_Q) ? ft_th[l][i][i]  : 0.0;
        Tm[i].NumAr2 = Tm[i].NumMa2 = 0;  Tm[i].NumAr1f = Tm[i].NumMa1f = 0;
        v_un_factor(&Tm[i].NumAr1, &Tm[i].p1, &Tm[i].Ar1, &Tm[i].Ia1, DA, ca);
        v_un_factor(&Tm[i].NumMa1, &Tm[i].q1, &Tm[i].Ma1, &Tm[i].Im1, DM, cm);
        for (l = 1; l <= DA; l++) {
            int libre = 0;
            for (jj = 0; jj <= l && jj <= ESC_MAXLAG; jj++)
                if (sA[i][jj] && l - jj <= v_reg_ar[i] && !(l - jj >= 1 && v_zar[i][l - jj])) libre = 1;
            if (!libre) { Tm[i].Ia1[1][l] = 0; Tm[i].Ar1[1][l] = 0.0; }
        }
        for (l = 1; l <= DM; l++) {
            int libre = 0;
            for (jj = 0; jj <= l && jj <= ESC_MAXLAG; jj++)
                if (sM[i][jj] && l - jj <= v_reg_ma[i] && !(l - jj >= 1 && v_zma[i][l - jj])) libre = 1;
            if (!libre) { Tm[i].Im1[1][l] = 0; Tm[i].Ma1[1][l] = 0.0; }
        }
        ar_deg[i] = total_ar_order(&Tm[i]);
        ma_deg[i] = total_ma_order(&Tm[i]);
        free_vector(cm, 0, DM > 0 ? DM : 1);  free_vector(ca, 0, DA > 0 ? DA : 1);
    }
    /* los cruzados: el retardo l de la fila i es libre si l = k + jj, con k un
       retardo libre del modelo y jj un retardo del factor de la fila */
    for (l = 1; l <= ESC_MAXLAG; l++)
        for (i = 1; i <= m; i++)
            for (j = 1; j <= m; j++) {
                int la = 0, lm = 0;
                if (i == j) continue;
                for (jj = 0; jj < l; jj++) {
                    if (sA[i][jj] && oA[l - jj][i][j]) la = 1;
                    if (sM[i][jj] && oM[l - jj][i][j]) lm = 1;
                }
                /* A_0 (= M_0) por el factor: el termino contemporaneo llega al retardo jj */
                if (a0[i][j] && sA[i][l]) la = 1;
                if (a0[i][j] && sM[i][l]) lm = 1;
                cx_zar[l][i][j] = !la;  cx_zma[l][i][j] = !lm;
                cAR[l][i][j] = (la && l <= ft_P) ? ft_phi[l][i][j] : 0.0;
                cMA[l][i][j] = (lm && l <= ft_Q) ? ft_th[l][i][j]  : 0.0;
            }
    v_on = 0;
    cx_p = p + dA;  cx_q = q + dM;

    quiet_mode = 1;
    fit_run(&A, 0);                              /* el mismo punto, sin el factor */
    logL0 = A.logL;  np1 = A.npar;
    i = A.ifault;
    fit_free(&A);
    if (i || fabs(logL0 - ft_logL) > 1.0e-6 * (1.0 + fabs(ft_logL))) {
        for (f = 0; f < 2; f++)
            fprintf(f ? out : stdout,
                    "  The rows without the factor do not reproduce the fitted model at its\n"
                    "  optimum (logL %.6f against %.6f%s): the test is not reported.\n",
                    logL0, ft_logL, i ? "; the point was rejected" : "");
        return;
    }
    if (np1 == ft_npar) {                        /* el factor no alcanza ninguna posicion mas */
        for (f = 0; f < 2; f++)
            fprintf(f ? out : stdout,
                    "  The rows with a factor have no own or cross term it could reach\n"
                    "  beyond those of the model: there is nothing to test (0 df).\n");
        return;
    }
    fit_run(&A, 1);
    if (A.ifault) {
        for (f = 0; f < 2; f++)
            fprintf(f ? out : stdout, "  The estimation without the factor failed: %s.\n", fault_msg(A.ifault));
        fit_free(&A);
        return;
    }
    logL1 = A.logL;
    df = np1 - ft_npar;
    lr = 2.0 * (logL1 - ft_logL);
    pv = df > 0 ? 1.0 - chisq(lr > 0.0 ? lr : 0.0, df) : 1.0;
    for (f = 0; f < 2; f++) {
        FILE *o = f ? out : stdout;
        fprintf(o, "  The univariate models' seasonal and fixed-frequency factors multiply\n");
        fprintf(o, "  their whole rows: a restriction. Against the same rows with every\n");
        fprintf(o, "  position the product reaches free (it starts at this optimum):\n");
        fprintf(o, "    with the row factor      logL %14.6f   %3d parameters\n", ft_logL, ft_npar);
        fprintf(o, "    without it               logL %14.6f   %3d parameters   (optimizer %s after %d iterations)\n",
                logL1, np1, (opt_termcode == 1 || opt_termcode == 2) ? "converged" : "STOPPED", opt_iters);
        fprintf(o, "    row factor: LR = %.4f   df = %d   p-value = %.4f\n", lr, df, pv);
        if (!(opt_termcode == 1 || opt_termcode == 2))
            fprintf(o, "  The search without the factor did not certify an optimum: its logL is\n"
                       "  a lower bound, and so is the LR.\n");
    }
    fit_free(&A);
}

/* ------------------------------------------------------------------------- */
/*  -start echelon: LA SIEMBRA DE LA ETAPA II (la forma echelon)             */
/*                                                                           */
/*  Las estimaciones de la etapa II de Lutkepohl y Poskitt como arranque de  */
/*  la maxima verosimilitud: el VAR largo y los minimos cuadrados sobre las  */
/*  posiciones libres de la forma -- las que el modelo tiene libres: los     */
/*  enlaces y los ceros quedan fuera. Siembra TODO: lo propio (AR y MA), los */
/*  cruzados, A_0 y Q (las varianzas y covarianzas de los residuos de la     */
/*  etapa II, con Q_11 = 1). Los minimos cuadrados pueden dejar una raiz     */
/*  fuera del circulo: se recogen dentro (por lambda^k) y se dice. Si elf no */
/*  acepta el punto, se vuelve al modelo sin cruzados. Es _echelon_start     */
/*  del porte: la etapa II llana, sin la ronda de Hannan y Kavalieris.       */
/* ------------------------------------------------------------------------- */
static void start_echelon(void)
{
    int     m = n_ser, h, h_max, dmax, t0, i, j, k, nmax = 0, n0[ESC_MAX + 1], rechazado;
    double **wraw, **E, la, lm;
    real  **ar0, **ma0, lv0[ESC_MAX + 1];
    EsFit   F;
    Fit     chk;

    for (i = 1; i <= m; i++) { n0[i - 1] = ech_n[i]; if (ech_n[i] > nmax) nmax = ech_n[i]; }
    E = es_etapa1(&wraw, &h, &h_max, &dmax);
    t0 = h + nmax + dmax;
    if (n_stat - t0 < 2 * es_dim(n0, m) / m + 10) {
        snprintf(start_used, sizeof start_used, "zero (too few observations for the echelon start)");
        es_cierra(E, wraw);
        return;
    }
    memset(&F, 0, sizeof F);
    es_modelo = 1;
    es_stage2(n0, E, t0, &F);
    es_modelo = 0;
    es_recoge(&F, 0.95, &la, &lm);

    ar0 = matrix(1, m, 0, nmax > 0 ? nmax : 1);  ma0 = matrix(1, m, 0, nmax > 0 ? nmax : 1);
    for (i = 1; i <= m; i++) {
        lv0[i] = lvar[i];
        for (k = 1; k <= ech_n[i]; k++) { ar0[i][k] = Tm[i].Ar1[1][k]; ma0[i][k] = Tm[i].Ma1[1][k]; }
    }
    for (i = 1; i <= m; i++) {
        for (k = 1; k <= ech_n[i]; k++) {
            if (Tm[i].Ia1[1][k]) Tm[i].Ar1[1][k] = -F.A[k][i - 1][i - 1];
            if (Tm[i].Im1[1][k]) Tm[i].Ma1[1][k] = -F.M[k][i - 1][i - 1];
        }
        for (j = 1; j <= m; j++) {
            if (i == j) continue;
            for (k = 1; k <= nmax; k++) {
                if (free_ar(k, i, j)) cAR[k][i][j] = -F.A[k][i - 1][j - 1];
                if (free_ma(k, i, j)) cMA[k][i][j] = -F.M[k][i - 1][j - 1];
            }
            if (free_a0(i, j)) cA0[i][j] = -F.A[0][i - 1][j - 1];
        }
    }
    for (i = 2; i <= m; i++) lvar[i] = log(F.S[i - 1][i - 1] / F.S[0][0]);
    if (!cx_diagcov)
        for (i = 2; i <= m; i++)
            for (j = 1; j < i; j++) qcov[i][j] = F.S[i - 1][j - 1] / F.S[0][0];

    fit_run(&chk, 0);
    rechazado = chk.ifault;
    fit_free(&chk);                 /* antes de reponer (BUG-0006) */
    if (rechazado) {
        for (i = 1; i <= m; i++) {
            lvar[i] = lv0[i];
            for (k = 1; k <= ech_n[i]; k++) { Tm[i].Ar1[1][k] = ar0[i][k]; Tm[i].Ma1[1][k] = ma0[i][k]; }
        }
        memset(cAR, 0, sizeof cAR);  memset(cMA, 0, sizeof cMA);
        memset(cA0, 0, sizeof cA0);  memset(qcov, 0, sizeof qcov);
        snprintf(start_used, sizeof start_used,
                 "the model without cross terms (the echelon start was not admissible)");
    } else if (la < 1.0 || lm < 1.0) {
        char a[40] = "", b[40] = "";
        if (la < 1.0) snprintf(a, sizeof a, "AR x %.3f^k", la);
        if (lm < 1.0) snprintf(b, sizeof b, "%sMA x %.3f^k", la < 1.0 ? ", " : "", lm);
        snprintf(start_used, sizeof start_used, "echelon (%s%s: inside the unit circle)", a, b);
    } else
        snprintf(start_used, sizeof start_used, "echelon");

    free_matrix(ma0, 1, m, 0, nmax > 0 ? nmax : 1);  free_matrix(ar0, 1, m, 0, nmax > 0 ? nmax : 1);
    es_free_fit(&F);
    es_cierra(E, wraw);
}

static void fit_free(Fit *F)
{
    int ifault = 0, np = F->npar > 0 ? F->npar : 1;
    if (F->vm.mu) shootx_esc(F->x, &F->vm, &ifault, 0, 1);
    free_matrix(F->cov, 1, np, 1, np);
    free_vector(F->dev, 1, np);
    free_vector(F->x, 1, np);
}

/* ------------------------------------------------------------------------- */
/*  LOS ERRORES TIPICOS DE LA IRF, POR EL METODO DELTA                        */
/*                                                                           */
/*  La IRF ortogonalizada es O_h = Psi_h L, con Psi_h los pesos MA(inf) del   */
/*  VARMA y L el Cholesky de Sigma. Var(O_h[i][k]) = g' V g, con g su         */
/*  gradiente y V la covarianza de lo estimado, en DOS bloques que con        */
/*  normalidad son independientes (la informacion es diagonal por bloques):   */
/*                                                                           */
/*    - la dinamica: los parametros de x salvo los de Q (van al final, ver    */
/*      esc_pack). Su covarianza es la de la tabla de parametros, F->cov      */
/*      (2 f H^-1 / n). El gradiente, por diferencias centrales A TRAVES DE   */
/*      shootx_esc: el mismo mapa x -> (Phi, Theta) que estima, sin derivar   */
/*      a mano los factores de cada .pre. Sigma queda fija.                   */
/*    - Sigma: Cov(S_ab, S_cd) = (S_ac S_bd + S_ad S_bc) / n. El gradiente,   */
/*      por diferencias centrales del Cholesky. Con -diagcov solo varia la    */
/*      diagonal.                                                            */
/*                                                                           */
/*  La acumulada es la suma: su gradiente, la suma de los gradientes.         */
/*  Bandas SIMETRICAS de la linealizacion; se apuntan sus limites en el .out. */
/*  Sin errores tipicos de los parametros (frontera, Hessiana no definida),   */
/*  no hay bandas y se dice.                                                  */
/* ------------------------------------------------------------------------- */
static int irf_chol(real **S, real **L, int m)
{
    int a, b, ifault = 0;
    real d1, d2;
    for (a = 1; a <= m; a++) for (b = 1; b <= m; b++) L[a][b] = S[a][b];
    choldcp(L, m, &d1, &d2, &ifault);
    for (a = 1; a <= m; a++) for (b = a + 1; b <= m; b++) L[a][b] = 0.0;
    return ifault;
}

/* O[h] = Psi_h L, h = 0..H, del modelo vm y el factor L */
static void irf_orth(struct Tvarma *vm, real **L, int H, real ***psi, real ***O)
{
    int m = vm->m, h, a, b, k;
    compute_psi_weights(m, vm->p, vm->q, vm->phi, vm->theta, H, psi);
    for (h = 0; h <= H; h++)
        for (a = 1; a <= m; a++)
            for (b = 1; b <= m; b++) {
                real acc = 0.0;
                for (k = 1; k <= m; k++) acc += psi[h][a][k] * L[k][b];
                O[h][a][b] = acc;
            }
}

static void irf_tabla_se(FILE *out, const char *titulo, real ***se, int H, int m)
{
    int k, h, i;
    fprintf(out, "\n=============================================================\n");
    fprintf(out, "%s\n", titulo);
    fprintf(out, "=============================================================\n");
    fprintf(out, "Delta method: the parameters' covariance (the table above) and\n"
                 "Sigma's, Cov(S_ab,S_cd) = (S_ac S_bd + S_ad S_bc)/n. Symmetric:\n"
                 "a band is the response +/- 2 s.e.; it understates the uncertainty\n"
                 "near a unit root and at long horizons.\n\n");
    for (k = 1; k <= m; k++) {
        fprintf(out, "Shock to %s:\n", Ts[act[k]].name);
        fprintf(out, "Horizon");
        for (i = 1; i <= m; i++) fprintf(out, "%9s", Ts[act[i]].name);
        fprintf(out, "\n");
        for (h = 0; h <= H; h++) {
            fprintf(out, "%5d  ", h);
            for (i = 1; i <= m; i++) fprintf(out, "%9.4f", se[h][i][k]);
            fprintf(out, "\n");
        }
        fprintf(out, "\n");
    }
}

static void irf_delta(Fit *F, int H, FILE *out)
{
    int m = F->vm.m, n = F->vm.n, np = F->npar, nq, nd, ns, j, a, b, c, d, h, i, k, g;
    int ifault = 0, pares[ESC_MAX * (ESC_MAX + 1) / 2 + 1][2];
    real **S, **L, **Lp, **Lm, ***psi, ***O, ***Op, ***Om, ***J, ***Ja, ***se, ***sea, *xp;
    real **Vs;

    nq = (m - 1) + (cx_diagcov ? 0 : m * (m - 1) / 2);     /* los de Q, al final */
    nd = np - nq;
    for (j = 1; j <= nd; j++)
        if (isnan(F->dev[j])) {
            fprintf(out, "\n(no standard errors for the impulse responses: the parameters "
                         "have none -- %s)\n", est_se_label(F->se_how));
            return;
        }

    /* los pares (a <= b) de Sigma que varian */
    ns = 0;
    for (a = 1; a <= m; a++)
        for (b = a; b <= m; b++)
            if (a == b || !cx_diagcov) { pares[++ns][0] = a; pares[ns][1] = b; }
    g = nd + ns;                                         /* columnas de J   */

    S   = matrix(1, m, 1, m);  L = matrix(1, m, 1, m);
    Lp  = matrix(1, m, 1, m);  Lm = matrix(1, m, 1, m);
    psi = tensor(0, H, 1, m, 1, m);
    O   = tensor(0, H, 1, m, 1, m);
    Op  = tensor(0, H, 1, m, 1, m);
    Om  = tensor(0, H, 1, m, 1, m);
    se  = tensor(0, H, 1, m, 1, m);
    sea = tensor(0, H, 1, m, 1, m);
    xp  = vector(1, np > 0 ? np : 1);
    Vs  = matrix(1, ns, 1, ns);
    /* J[h][(i-1)*m+k][col]: d O_h[i][k] / d theta_col */
    J   = tensor(0, H, 1, m * m, 1, g);
    Ja  = tensor(0, H, 1, m * m, 1, g);

    for (a = 1; a <= m; a++)
        for (b = 1; b <= m; b++) S[a][b] = F->sigma2 * F->vm.qq[a][b];
    if (irf_chol(S, L, m)) {
        fprintf(out, "\n(no standard errors for the impulse responses: Sigma is not "
                     "positive definite)\n");
        goto fin;
    }

    /* [1] la dinamica, por shootx_esc */
    for (j = 1; j <= nd; j++) {
        real e = 1.0e-5 * (fabs(F->x[j]) > 1.0 ? fabs(F->x[j]) : 1.0), den = 0.0;
        int okp, okm;
        for (i = 1; i <= np; i++) xp[i] = F->x[i];
        xp[j] = F->x[j] + e;
        shootx_esc(xp, &F->vm, &ifault, 0, 0);
        okp = (ifault == 0);
        if (okp) irf_orth(&F->vm, L, H, psi, Op);
        xp[j] = F->x[j] - e;
        shootx_esc(xp, &F->vm, &ifault, 0, 0);
        okm = (ifault == 0);
        if (okm) irf_orth(&F->vm, L, H, psi, Om);
        shootx_esc(F->x, &F->vm, &ifault, 0, 0);
        if (!okp || !okm) irf_orth(&F->vm, L, H, psi, O);
        if (okp && okm) den = 2.0 * e; else if (okp || okm) den = e;
        for (h = 0; h <= H; h++)
            for (i = 1; i <= m; i++)
                for (k = 1; k <= m; k++)
                    J[h][(i - 1) * m + k][j] = den > 0.0
                        ? ((okp ? Op[h][i][k] : O[h][i][k]) - (okm ? Om[h][i][k] : O[h][i][k])) / den
                        : 0.0;
    }

    /* [2] Sigma, por su Cholesky; la dinamica, la estimada */
    irf_orth(&F->vm, L, H, psi, O);
    for (c = 1; c <= ns; c++) {
        real e;
        a = pares[c][0]; b = pares[c][1];
        e = 1.0e-5 * sqrt(S[a][a] * S[b][b]);
        S[a][b] += e; if (a != b) S[b][a] += e;
        irf_chol(S, Lp, m);
        S[a][b] -= 2.0 * e; if (a != b) S[b][a] -= 2.0 * e;
        irf_chol(S, Lm, m);
        S[a][b] += e; if (a != b) S[b][a] += e;
        for (h = 0; h <= H; h++)
            for (i = 1; i <= m; i++)
                for (k = 1; k <= m; k++) {
                    real dp = 0.0, dm = 0.0;
                    int t;
                    for (t = 1; t <= m; t++) { dp += psi[h][i][t] * Lp[t][k]; dm += psi[h][i][t] * Lm[t][k]; }
                    J[h][(i - 1) * m + k][nd + c] = (dp - dm) / (2.0 * e);
                }
    }
    for (c = 1; c <= ns; c++)
        for (d = 1; d <= ns; d++) {
            int a1 = pares[c][0], b1 = pares[c][1], c1 = pares[d][0], d1 = pares[d][1];
            Vs[c][d] = (S[a1][c1] * S[b1][d1] + S[a1][d1] * S[b1][c1]) / n;
        }

    /* [3] la acumulada y las varianzas */
    for (h = 0; h <= H; h++)
        for (i = 1; i <= m * m; i++)
            for (c = 1; c <= g; c++)
                Ja[h][i][c] = J[h][i][c] + (h > 0 ? Ja[h - 1][i][c] : 0.0);
    for (h = 0; h <= H; h++)
        for (i = 1; i <= m; i++)
            for (k = 1; k <= m; k++) {
                int r = (i - 1) * m + k, w;
                for (w = 0; w < 2; w++) {
                    real ***JJ = w ? Ja : J, v = 0.0;
                    for (c = 1; c <= nd; c++)
                        for (d = 1; d <= nd; d++) v += JJ[h][r][c] * F->cov[c][d] * JJ[h][r][d];
                    for (c = 1; c <= ns; c++)
                        for (d = 1; d <= ns; d++) v += JJ[h][r][nd + c] * Vs[c][d] * JJ[h][r][nd + d];
                    (w ? sea : se)[h][i][k] = v > 0.0 ? sqrt(v) : 0.0;
                }
            }

    irf_tabla_se(out, "  STD. ERRORS OF THE ORTHOGONALIZED IMPULSE RESPONSES", se, H, m);
    irf_tabla_se(out, "  STD. ERRORS OF THE ACCUMULATED IMPULSE RESPONSES", sea, H, m);

fin:
    shootx_esc(F->x, &F->vm, &ifault, 0, 0);
    free_tensor(Ja, 0, H, 1, m * m, 1, g);
    free_tensor(J, 0, H, 1, m * m, 1, g);
    free_matrix(Vs, 1, ns, 1, ns);
    free_vector(xp, 1, np > 0 ? np : 1);
    free_tensor(sea, 0, H, 1, m, 1, m);
    free_tensor(se, 0, H, 1, m, 1, m);
    free_tensor(Om, 0, H, 1, m, 1, m);
    free_tensor(Op, 0, H, 1, m, 1, m);
    free_tensor(O, 0, H, 1, m, 1, m);
    free_tensor(psi, 0, H, 1, m, 1, m);
    free_matrix(Lm, 1, m, 1, m); free_matrix(Lp, 1, m, 1, m);
    free_matrix(L, 1, m, 1, m);  free_matrix(S, 1, m, 1, m);
}

/* ------------------------------------------------------------------------- */
/*  -irfboot N: LAS BANDAS DE LA IRF POR BOOTSTRAP PARAMETRICO                */
/*                                                                           */
/*  La otra cara del metodo delta (irf_delta): sin linealizar, y con bandas   */
/*  que pueden ser asimetricas. El diseño es el de drvec (-irfboot, en        */
/*  drvec.c) y el de sima (irf_bands), para que las tres digan lo mismo:      */
/*                                                                           */
/*    - el modelo ajustado se simula N veces: el VARMA estacionario de las    */
/*      w, con su media y con innovaciones normales de la Sigma estimada,     */
/*      y un tramo inicial que se tira (50 + 10 P). EL MISMO GENERADOR que    */
/*      drvec (boot_normal, su semilla): las bandas se reproducen;            */
/*    - cada replica se REESTIMA como los datos: la misma escalera, los       */
/*      mismos enlaces y ceros, desde el optimo. Sin la Hessiana en el        */
/*      optimo (aqui no hacen falta errores tipicos) y con elf;               */
/*    - las bandas son los percentiles de Efron (2.5, 5, 50, 95, 97.5) de la  */
/*      IRF ortogonalizada y de su acumulada, con la Sigma de cada replica.   */
/*                                                                           */
/*  Al .out, la tabla de 5 y 95 y de 2.5 y 97.5; entera, a <base>.irfboot,    */
/*  con el formato del de drvec (what,shock,response,k,point,p2.5,...).       */
/*  Con -redet no: las w se rehacen de los datos al estimar.                  */
/* ------------------------------------------------------------------------- */
static uint64_t boot_rng = 987654321ULL;

static real boot_normal(void)
{
    /* Box-Muller sobre un LCG propio, el de drvec: determinista y sin libc. */
    static int have = 0;
    static real spare = 0.0;
    real u1, u2, r, th;
    if (have) { have = 0; return spare; }
    do {
        boot_rng = boot_rng * 6364136223846793005ULL + 1442695040888963407ULL;
        u1 = ((real)((boot_rng >> 33) & 0x7FFFFFFF)) / 2147483648.0;
    } while (u1 <= 1.0e-12);
    boot_rng = boot_rng * 6364136223846793005ULL + 1442695040888963407ULL;
    u2 = ((real)((boot_rng >> 33) & 0x7FFFFFFF)) / 2147483648.0;
    r  = sqrt(-2.0 * log(u1));
    th = 2.0 * M_PI * u2;
    spare = r * sin(th); have = 1;
    return r * cos(th);
}

static int cmp_real_boot(const void *a, const void *b)
{
    real x = *(const real *) a, y = *(const real *) b;
    return (x > y) - (x < y);
}

/* Una muestra de las w del modelo v (con Sigma = s2 * qq): sim[t][a]. */
static int boot_simula(struct Tvarma *v, real s2, real **sim)
{
    int m = v->m, n = v->n, P = v->p, Q = v->q, burn = 50 + 10 * P, T = n + burn;
    int t, i, j, k;
    real **wb = matrix(1, T, 1, m), **ab = matrix(1, T, 1, m);
    real **S = matrix(1, m, 1, m), **L = matrix(1, m, 1, m), *z = vector(1, m);
    int bad;

    for (i = 1; i <= m; i++) for (j = 1; j <= m; j++) S[i][j] = s2 * v->qq[i][j];
    bad = irf_chol(S, L, m);
    if (!bad)
        for (t = 1; t <= T; t++) {
            for (i = 1; i <= m; i++) z[i] = boot_normal();
            for (i = 1; i <= m; i++) {
                real acc = 0.0;
                for (k = 1; k <= i; k++) acc += L[i][k] * z[k];
                ab[t][i] = acc;
            }
            for (i = 1; i <= m; i++) {
                real acc = ab[t][i];
                for (j = 1; j <= P; j++) if (t - j >= 1)
                    for (k = 1; k <= m; k++) acc += v->phi[j][i][k] * (wb[t-j][k] - v->mu[k]);
                for (j = 1; j <= Q; j++) if (t - j >= 1)
                    for (k = 1; k <= m; k++) acc -= v->theta[j][i][k] * ab[t-j][k];
                wb[t][i] = v->mu[i] + acc;
            }
        }
    if (!bad)
        for (t = 1; t <= n; t++)
            for (i = 1; i <= m; i++) sim[t][i] = wb[t + burn][i];
    free_vector(z, 1, m);
    free_matrix(L, 1, m, 1, m); free_matrix(S, 1, m, 1, m);
    free_matrix(ab, 1, T, 1, m); free_matrix(wb, 1, T, 1, m);
    return bad;
}

static void irf_bootstrap(Fit *F, int H, int N, const char *base, FILE *out)
{
    int m = F->vm.m, n = F->vm.n, b, a, i, k, h, nok = 0, nfail = 0, w2, ifault = 0;
    long cells = (long) (H + 1) * m * m;
    real *irf = (real *) malloc(sizeof(real) * (size_t) cells * (size_t) N);
    real *acu = (real *) malloc(sizeof(real) * (size_t) cells * (size_t) N);
    real **sim = matrix(1, n, 1, m), **saved = matrix(1, m, 1, n), *col = vector(1, N);
    real **S = matrix(1, m, 1, m), **L = matrix(1, m, 1, m);
    real ***psi = tensor(0, H, 1, m, 1, m), ***O = tensor(0, H, 1, m, 1, m);
    real ***O0 = tensor(0, H, 1, m, 1, m);
    struct Tvarma vh;
    FILE *save_out = outputv, *nul = tmpfile(), *fb;
    int save_quiet = quiet_mode, save_fd = est_fdhess, save_lik = est_lik;
    char fname[600];

    if (!irf || !acu || !nul) {
        fprintf(out, "\n(no bootstrap bands: out of memory)\n");
        goto fin;
    }
    /* el modelo ajustado, con su Sigma, y su IRF (la del informe) */
    vh = F->vm;
    for (a = 1; a <= m; a++) for (i = 1; i <= m; i++) S[a][i] = F->sigma2 * F->vm.qq[a][i];
    irf_chol(S, L, m);
    irf_orth(&F->vm, L, H, psi, O0);
    for (a = 1; a <= m; a++)
        for (k = 1; k <= n; k++) saved[a][k] = w[act[a]][k];

    printf("IRF bootstrap (-irfboot %d): ", N); fflush(stdout);
    outputv = nul; quiet_mode = 1; est_fdhess = 0; est_lik = LIK_ELF;
    for (b = 1; b <= N; b++) {
        Fit Fb;
        int good = 0;
        if (boot_simula(&vh, F->sigma2, sim) == 0) {
            for (a = 1; a <= m; a++)
                for (k = 1; k <= n; k++) w[act[a]][k] = sim[k][a];
            esc_unpack(F->x);                     /* desde el optimo        */
            fit_run(&Fb, 1);
            if (Fb.ifault == 0) {
                for (a = 1; a <= m; a++)
                    for (i = 1; i <= m; i++) S[a][i] = Fb.sigma2 * Fb.vm.qq[a][i];
                if (irf_chol(S, L, m) == 0) {
                    irf_orth(&Fb.vm, L, H, psi, O);
                    good = 1;
                }
            }
            fit_free(&Fb);
        }
        if (good) {
            long base0 = (long) nok * cells;
            for (i = 1; i <= m; i++)
                for (k = 1; k <= m; k++) {
                    real ac = 0.0;
                    for (h = 0; h <= H; h++) {
                        long c = base0 + ((long) h * m + (i - 1)) * m + (k - 1);
                        ac += O[h][i][k];
                        irf[c] = O[h][i][k];
                        acu[c] = ac;
                    }
                }
            nok++;
        } else nfail++;
        if (b % 50 == 0) { printf("%d ", b); fflush(stdout); }
    }
    outputv = save_out; quiet_mode = save_quiet; est_fdhess = save_fd; est_lik = save_lik;
    for (a = 1; a <= m; a++)
        for (k = 1; k <= n; k++) w[act[a]][k] = saved[a][k];
    shootx_esc(F->x, &F->vm, &ifault, 0, 0);       /* el estado, al optimo   */
    printf("\n  %d replications refitted, %d failed\n", nok, nfail);

    fprintf(out, "\n=============================================================\n");
    fprintf(out, "  BOOTSTRAP BANDS OF THE ORTHOGONALIZED IMPULSE RESPONSES\n");
    fprintf(out, "=============================================================\n");
    fprintf(out, "%d replications simulated from the fitted model (Gaussian shocks\n"
                 "with the fitted Sigma) and REFITTED as the data were; %d failed and\n"
                 "are left out. Percentile intervals (Efron). Same orthogonalisation\n"
                 "as the responses above. The whole table is in %s.irfboot.\n",
            nok, nfail, base);
    if (nok < 20) {
        fprintf(out, "\nToo few replications for bands (%d).\n", nok);
        goto fin;
    }
    snprintf(fname, sizeof fname, "%s.irfboot", base);
    fb = fopen(fname, "w");
    if (fb) fprintf(fb, "# drvarma -irfboot %d: %d replications, percentile bands\n"
                        "what,shock,response,k,point,p2.5,p5,p50,p95,p97.5\n", N, nok);
    for (w2 = 0; w2 < 2; w2++) {
        real *src = w2 ? acu : irf;
        for (k = 1; k <= m; k++)
            for (i = 1; i <= m; i++) {
                real ac0 = 0.0;
                fprintf(out, "\n%s of %s to a shock to %s:\n"
                             "    h        point       5%%      95%%     2.5%%    97.5%%\n",
                        w2 ? "Accumulated response" : "Response",
                        Ts[act[i]].name, Ts[act[k]].name);
                for (h = 0; h <= H; h++) {
                    real q[5], pt;
                    const real pp[5] = { 0.025, 0.05, 0.50, 0.95, 0.975 };
                    int t, j;
                    for (t = 0; t < nok; t++)
                        col[t + 1] = src[(long) t * cells + ((long) h * m + (i - 1)) * m + (k - 1)];
                    qsort(&col[1], (size_t) nok, sizeof(real), cmp_real_boot);
                    for (j = 0; j < 5; j++) {
                        int ix = (int) ceil(pp[j] * nok);
                        if (ix < 1) ix = 1;
                        if (ix > nok) ix = nok;
                        q[j] = col[ix];
                    }
                    ac0 += O0[h][i][k];
                    pt = w2 ? ac0 : O0[h][i][k];
                    fprintf(out, "%5d %12.6f %8.4f %8.4f %8.4f %8.4f\n",
                            h, pt, q[1], q[3], q[0], q[4]);
                    if (fb) fprintf(fb, "%s,%s,%s,%d,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g\n",
                                    w2 ? "acc" : "irf", Ts[act[k]].name, Ts[act[i]].name,
                                    h, pt, q[0], q[1], q[2], q[3], q[4]);
                }
            }
    }
    if (fb) { fclose(fb); printf("  bands written to the .out and to %s\n", fname); }

fin:
    if (nul) fclose(nul);
    free_tensor(O0, 0, H, 1, m, 1, m); free_tensor(O, 0, H, 1, m, 1, m);
    free_tensor(psi, 0, H, 1, m, 1, m);
    free_matrix(L, 1, m, 1, m); free_matrix(S, 1, m, 1, m);
    free_vector(col, 1, N);
    free_matrix(saved, 1, m, 1, n); free_matrix(sim, 1, n, 1, m);
    free(acu); free(irf);
}

static const char *fault_msg(int f)
{
    switch (f) {
        case 1: return "Q matrix not positive definite";
        case 2: return "unit root in AR";
        case 3: return "AR nonstationary";
        case 4: return "MA noninvertible";
        case 5: return "numerical problem";
        case 6: return "inadmissible point (fixed-frequency factor or deterministic denominator)";
        default: return "unknown";
    }
}

/* ------------------------------------------------------------------------- */
/*  Informe                                                                  */
/* ------------------------------------------------------------------------- */
static void operator_str(struct Tusmodel *M, char *s, size_t n)
{
    char tmp[64];
    s[0] = '\0';
    if (M->nrdiff > 0) {
        snprintf(tmp, sizeof tmp, M->nrdiff > 1 ? "D^%d " : "D ", M->nrdiff);
        strncat(s, tmp, n - strlen(s) - 1);
    }
    if (M->nadiff > 0) {
        snprintf(tmp, sizeof tmp, M->nadiff > 1 ? "D%d^%d" : "D%d", M->sper, M->nadiff);
        strncat(s, tmp, n - strlen(s) - 1);
    }
    if (s[0] == '\0') snprintf(s, n, "none");
    if (M->ornsop > M->nrdiff + M->nadiff * M->sper) {
        snprintf(tmp, sizeof tmp, " (order %d)", M->ornsop);
        strncat(s, tmp, n - strlen(s) - 1);
    }
}

static void factor_str(struct Tusmodel *M, int is_ar, char *s, size_t n)
{
    char tmp[64];
    int  f, n1 = is_ar ? M->NumAr1 : M->NumMa1, n2 = is_ar ? M->NumAr2 : M->NumMa2;
    int  nf = is_ar ? M->NumAr1f : M->NumMa1f;
    int *o1 = is_ar ? M->p1 : M->q1, *o2 = is_ar ? M->p2 : M->q2;
    s[0] = '\0';
    for (f = 1; f <= n1; f++) { snprintf(tmp, sizeof tmp, "(%d)", o1[f]); strncat(s, tmp, n - strlen(s) - 1); }
    for (f = 1; f <= n2; f++) { snprintf(tmp, sizeof tmp, "(%d)_%d", o2[f], M->sper); strncat(s, tmp, n - strlen(s) - 1); }
    if (nf > 0) { snprintf(tmp, sizeof tmp, "+%d fixed-freq", nf); strncat(s, tmp, n - strlen(s) - 1); }
    if (s[0] == '\0') snprintf(s, n, "-");
}

static void print_series_table(FILE *f)
{
    int i;
    fprintf(f, "Univariate models (one .pre per series):\n");
    fprintf(f, "  %-3s %-14s %6s %9s %9s %7s %-12s %4s %-16s %-16s %s\n",
            "#", "series", "nobs", "start", "end", "lambda", "operator", "det",
            "AR", "MA", "mean");
    for (i = 1; i <= n_ser; i++) {
        char op[64], ar[64], ma[64];
        int ey, es;
        ObsToDate(Ts[i].begyear, Ts[i].begtime, Ts[i].nobs, Ts[i].freq, &ey, &es);
        operator_str(&Tm[i], op, sizeof op);
        factor_str(&Tm[i], 1, ar, sizeof ar);
        factor_str(&Tm[i], 0, ma, sizeof ma);
        fprintf(f, "  %-3d %-14s %6d %4d/%-4d %4d/%-4d %7g %-12s %4d %-16s %-16s %s\n",
                i, Ts[i].name, Ts[i].nobs, Ts[i].begtime, Ts[i].begyear, es, ey,
                Tm[i].boxlam, op, Tm[i].NdetVar, ar, ma,
                Tm[i].Imu ? "estimated" : "fixed");
    }
    for (i = 1; i <= n_ser; i++)
        fprintf(f, "  [%d] %s\n", i, pre_path[i]);
}

static void print_param_table(FILE *f, Fit *F)
{
    char (*names)[ESC_NAMELEN];
    int  i;
    if (F->npar <= 0) { fprintf(f, "  (no free parameters)\n"); return; }
    names = malloc((F->npar + 1) * sizeof *names);
    esc_names(names);
    fprintf(f, "  %-34s %12s %12s %9s %8s\n", "Parameter", "Estimate", "Std.Error", "t-stat", "p-val");
    for (i = 1; i <= F->npar; i++) {
        real se = F->dev[i], t;
        if (isnan(se)) {            /* no standard errors: est_se_label says why */
            fprintf(f, "  %-34s %12.6f\n", names[i], F->x[i]);
            continue;
        }
        t = (se > 0.0) ? F->x[i] / se : 0.0;
        real pv = (se > 0.0) ? 2.0 * (1.0 - normal_cdf(fabs(t))) : 1.0;
        fprintf(f, "  %-34s %12.6f %12.6f %9.3f %8.4f\n", names[i], F->x[i], se, t, pv);
    }
    fprintf(f, "  Standard errors: %s\n", est_se_label(F->se_how));
    free(names);
}

/* ------------------------------------------------------------------------- */
/*  Los factores MA propios, cada uno leido solo (23-nov-2026).              */
/*                                                                           */
/*  La verosimilitud solo se evalua donde el operador MA del SISTEMA es      */
/*  invertible (chekma, elf), como Mauricio (1995, sec. 3): la escalera no   */
/*  toma el invertible de un coeficiente, porque en un sistema invertir un   */
/*  elemento de Theta no es el mismo modelo (salvo series independientes).   */
/*  Un factor MA propio puede quedar asi fuera del circulo unidad leido      */
/*  solo: fue nunca estima un factor asi (toma el invertible en su cast).    */
/*  Se dice, sin cambiar nada. Caso: rata almizclera y vison, VARMA(2,1),    */
/*  ruta U, arranque cero: se para (sin punto mas bajo) con theta_MUSKRAT   */
/*  = 1.5483, sin tocar el muro del sistema.                                 */
/* ------------------------------------------------------------------------- */
#define OWN_MA_MAXQ 64
/* El mayor modulo de las raices inversas de 1 - c[1] z - ... - c[q] z^q.   */
static real own_ma_modulus(const real *c, int q)
{
    real **a, *wr, *wi, mx = 0.0;
    int  i, j;
    if (q <= 0) return 0.0;
    if (q == 1) return fabs(c[1]);
    a = matrix(1, q, 1, q);  wr = vector(1, q);  wi = vector(1, q);
    for (i = 1; i <= q; i++)
        for (j = 1; j <= q; j++) a[i][j] = 0.0;
    for (j = 1; j <= q; j++) a[1][j] = c[j];
    for (i = 2; i <= q; i++) a[i][i - 1] = 1.0;
    eigenqr(a, q, wr, wi);
    for (i = 1; i <= q; i++) if (a[i][i] > mx) mx = a[i][i];
    free_vector(wi, 1, q);  free_vector(wr, 1, q);  free_matrix(a, 1, q, 1, q);
    return mx;
}

/* Cuantos factores MA propios quedan fuera (modulo >= 1 - MA_WALL_TOL); con
   f != NULL, el bloque del .out; con aviso, una linea en pantalla.          */
static int own_ma_outside(FILE *f, int aviso)
{
    int  a, k, j, n = 0, q;
    real c[OWN_MA_MAXQ + 1], md;
    char line[256], lista[1024];
    lista[0] = '\0';
    for (a = 1; a <= act_m; a++) {
        struct Tusmodel *M = &Tm[act[a]];
        const char *nm = Ts[act[a]].name;
        for (int kind = 0; kind < 3; kind++) {
            int nf = kind == 0 ? M->NumMa1 : kind == 1 ? M->NumMa2 : M->NumMa1f;
            for (k = 1; k <= nf; k++) {
                const char *what;
                char coef[64] = "";
                if (kind == 0) {
                    q = M->q1[k]; if (q > OWN_MA_MAXQ) q = OWN_MA_MAXQ;
                    for (j = 1; j <= q; j++) c[j] = M->Ma1[k][j];
                    md = own_ma_modulus(c, q);
                    what = "regular MA";
                } else if (kind == 1) {
                    q = M->q2[k]; if (q > OWN_MA_MAXQ) q = OWN_MA_MAXQ;
                    for (j = 1; j <= q; j++) c[j] = M->Ma2[k][j];
                    md = own_ma_modulus(c, q);   /* in B^s: outside iff >= 1 */
                    what = "seasonal MA";
                } else {
                    real c2 = M->Ma1f[k][2];
                    q = 2;
                    md = c2 < 0.0 ? sqrt(-c2) : 0.0;
                    what = "fixed-frequency MA";
                    snprintf(coef, sizeof coef, "c2 = %.6f", c2);
                }
                if (md < 1.0 - MA_WALL_TOL) continue;
                if (kind != 2 && q == 1)
                    snprintf(coef, sizeof coef, "%s = %.6f", kind == 0 ? "theta" : "Theta", c[1]);
                n++;
                if (f) {
                    if (n == 1)
                        fprintf(f, "\nOWN MA FACTORS NOT INVERTIBLE, EACH READ ALONE:\n");
                    snprintf(line, sizeof line, "%s(%d)", what, q);
                    fprintf(f, "  %-14s %-22s %-20s inverse root modulus %.6f (%s)\n",
                            nm, kind == 2 ? what : line, coef, md,
                            md >= 1.0 + MA_WALL_TOL ? "beyond the unit circle"
                                                     : "on the unit circle");
                }
                if (strlen(lista) + strlen(nm) + 32 < sizeof lista) {
                    char it[64];
                    snprintf(it, sizeof it, "%s%.40s (%.4f)", lista[0] ? ", " : "", nm, md);
                    strcat(lista, it);
                }
            }
        }
    }
    if (n > 0 && f)
        fprintf(f, "  The likelihood is evaluated only where the MA operator of the SYSTEM\n"
                   "  is invertible, so this is a point of the joint model: each own factor\n"
                   "  is one block of that operator, not a univariate model. Read alone it\n"
                   "  is not invertible -- fue never estimates such a factor -- and it is\n"
                   "  not to be read or reported as one.\n");
    if (n > 0 && aviso)
        printf("WARNING: own MA factor(s) not invertible read alone: %s\n", lista);
    return n;
}

static void print_sigma(FILE *f, Fit *F)
{
    int a, b, m = F->vm.m;
    fprintf(f, "\nInnovation covariance Sigma = sigma2 * Q  (sigma2 = %.8g):\n", F->sigma2);
    for (a = 1; a <= m; a++) {
        fprintf(f, "  %-14s", Ts[act[a]].name);
        for (b = 1; b <= m; b++) fprintf(f, " %14.6g", F->sigma2 * F->vm.qq[a][b]);
        fprintf(f, "\n");
    }
    fprintf(f, "Correlations:\n");
    for (a = 1; a <= m; a++) {
        fprintf(f, "  %-14s", Ts[act[a]].name);
        for (b = 1; b <= m; b++)
            fprintf(f, " %8.4f", F->vm.qq[a][b] / sqrt(F->vm.qq[a][a] * F->vm.qq[b][b]));
        fprintf(f, "\n");
    }
}

/* ------------------------------------------------------------------------- */
/*  Forecasting (phase 2)                                                    */
/*                                                                           */
/*  The system forecasts the stationary series w. Each series goes back to   */
/*  its LEVEL with its own .pre model, through lib/fuepre (the same code as  */
/*  drtran): deterministic terms, integration with its operator, inverse     */
/*  Box-Cox. The bands come from the psi weights of the VARMA integrated     */
/*  with 1/rnsop(B) and Sigma = sigma2 Q, on the transformed scale, and are  */
/*  mapped back through the inverse Box-Cox.                                 */
/*                                                                           */
/*  The forecast is made from the CURRENT end of each series (Ts[i].nobs):   */
/*  the recursive evaluation moves that end and calls again, with the        */
/*  parameters fixed. At every origin the stationary series and the exact    */
/*  residuals are rebuilt on the data up to that origin, as fue does when it */
/*  forecasts from a .fuf.                                                   */
/* ------------------------------------------------------------------------- */
static int nobs_full[ESC_MAX + 1];

typedef struct {
    int   nb;                  /* history used: obs 1..nb of the series    */
    real *ystar;               /* 1..nb+L: transformed level, det included */
    real *lvl, *lo, *hi;       /* 1..L: level and 95% band                 */
    real *sd, *sdp, *sda;      /* 1..L: sd of the transformed level, of its */
                               /* period and of its annual variation       */
    real *err;                 /* 0..L: the exact residual of obs nb-L+j    */
    int  *has_err;             /*       (0 where there is none)            */
} Fc;

static void fc_alloc(Fc *c, int nb, int L)
{
    c->nb = nb;
    c->ystar = vector(1, nb + L);
    c->lvl = vector(1, L); c->lo = vector(1, L); c->hi = vector(1, L);
    c->sd = vector(1, L);  c->sdp = vector(1, L); c->sda = vector(1, L);
    c->err = vector(0, L); c->has_err = ivector(0, L);
    { int j; for (j = 0; j <= L; j++) { c->err[j] = 0.0; c->has_err[j] = 0; } }
}

static void fc_free(Fc *c, int L)
{
    free_ivector(c->has_err, 0, L); free_vector(c->err, 0, L);
    free_vector(c->sda, 1, L); free_vector(c->sdp, 1, L); free_vector(c->sd, 1, L);
    free_vector(c->hi, 1, L);  free_vector(c->lo, 1, L);  free_vector(c->lvl, 1, L);
    free_vector(c->ystar, 1, c->nb + L);
}

/* var at lead l of SUM_t V(t) a_{l-t}, V(t) = P(t) - P(t-d) (d = 0: P(t)) */
static real lead_var(real **P, real **sigma, int m, int l, int d)
{
    real v = 0.0;
    int  t, j, k;
    for (t = 0; t < l; t++)
        for (j = 1; j <= m; j++) {
            real pj = P[j][t] - ((d > 0 && t >= d) ? P[j][t - d] : 0.0);
            if (pj == 0.0) continue;
            for (k = 1; k <= m; k++) {
                real pk = P[k][t] - ((d > 0 && t >= d) ? P[k][t - d] : 0.0);
                v += pj * sigma[j][k] * pk;
            }
        }
    return v;
}

/* Forecast L steps from the current end of every active series, with the
   parameters x (fixed) and the concentrated sigma2. fc[1..act_m]. 0 = ok. */
static int esc_forecast(real *x, real sigma2, int L, Fc *fc)
{
    struct Tvarma vf;
    int    ifault = 0, ifa = 0, m = act_m, a, b, j, k, l, t;
    real   pi1, pi2, pi3, **sigma, **f1, ***v1, ***v2, ***v3, ***psi, **P, *uu;

    if (build_stationary() != 0) return 1;
    shootx_esc(x, &vf, &ifault, 1, 0);
    if (ifault) { shootx_esc(x, &vf, &ifault, 0, 1); return 2; }

    /* the EXACT residuals (atf = TRUE): the MA part forecasts with them */
    elf(vf.m, vf.n, vf.p, vf.q, vf.mu, vf.phi, vf.theta, vf.qq, vf.w, 1.0,
        xitol_met, TRUE, vf.a, &pi1, &pi2, &pi3, &ifa);
    if (ifa) { shootx_esc(x, &vf, &ifault, 0, 1); return 3; }

    sigma = matrix(1, m, 1, m);
    for (a = 1; a <= m; a++)
        for (b = 1; b <= m; b++) sigma[a][b] = sigma2 * vf.qq[a][b];

    f1 = matrix(1, m, 1, L);
    v1 = tensor(1, L, 1, m, 1, m);
    v2 = tensor(1, L, 1, m, 1, m);
    v3 = tensor(1, L, 1, m, 1, m);
    forecast_model(m, n_stat, vf.p, vf.q, vf.mu, vf.phi, vf.theta, sigma,
                   vf.w, vf.a, f1, v1, v2, v3, 0, L, data_freq, NULL);
    psi = tensor(0, L, 1, m, 1, m);
    compute_psi_weights(m, vf.p, vf.q, vf.phi, vf.theta, L, psi);

    P  = matrix(1, m, 0, L);
    uu = vector(0, L);
    for (a = 1; a <= m; a++) {
        int i = act[a];
        Fc *c = &fc[a];

        fuepre_level_forecast(&Tm[i], &Ts[i], Ts[i].nobs, f1[a], L,
                              c->ystar, c->lvl, NULL);

        /* THE ERR COLUMN of the forecast report (write_fuf_report): the exact
           residual of each of the last L+1 observations. The stationary
           series are aligned at their END (build_stationary drops from the
           start), so obs nb-L+j is t = n_stat-L+j.                         */
        for (j = 0; j <= L; j++) {
            int tt = n_stat - L + j;
            if (tt >= 1 && tt <= n_stat) { c->err[j] = vf.a[tt][a]; c->has_err[j] = 1; }
        }

        /* psi of the transformed level of series i: 1/rnsop_i(B) psi_w(B) */
        fuepre_integrator(&Tm[i], L, uu);
        for (j = 1; j <= m; j++)
            for (t = 0; t <= L; t++) {
                real acc = 0.0;
                for (k = 0; k <= t; k++) acc += uu[k] * psi[t - k][a][j];
                P[j][t] = acc;
            }
        for (l = 1; l <= L; l++) {
            real yc = c->ystar[Ts[i].nobs + l];
            c->sd[l]  = sqrt(lead_var(P, sigma, m, l, 0));
            c->sdp[l] = sqrt(lead_var(P, sigma, m, l, 1));
            c->sda[l] = sqrt(lead_var(P, sigma, m, l, data_freq));
            c->lo[l]  = fuepre_bc_inverse(&Tm[i], &Ts[i], yc - 1.96 * c->sd[l], NULL);
            c->hi[l]  = fuepre_bc_inverse(&Tm[i], &Ts[i], yc + 1.96 * c->sd[l], NULL);
        }
    }

    free_vector(uu, 0, L);
    free_matrix(P, 1, m, 0, L);
    free_tensor(psi, 0, L, 1, m, 1, m);
    free_tensor(v3, 1, L, 1, m, 1, m);
    free_tensor(v2, 1, L, 1, m, 1, m);
    free_tensor(v1, 1, L, 1, m, 1, m);
    free_matrix(f1, 1, m, 1, L);
    free_matrix(sigma, 1, m, 1, m);
    shootx_esc(x, &vf, &ifault, 0, 1);
    return 0;
}

/* The sample of every series ends k periods before its full end. They all
   end on the same date (fuepre_check_alignment), so this cuts them all at
   the same date.                                                          */
static void set_origin(int k)
{
    int i;
    for (i = 1; i <= n_ser; i++) Ts[i].nobs = nobs_full[i] - k;
}

/* The forecast table of each series, and the same numbers to NAME.forecast
   in the format of the .inp path (Level, Low95, High95 in original units;
   period and annual variation on the transformed scale, x 100/refactor,
   which for lambda = 0 is a percentage).                                  */
static void write_forecast(FILE *out, FILE *ff, Fc *fc, int L)
{
    int a, l, per, sub, freq = data_freq;
    if (ff) {
        fprintf(ff, "Forecasts from the ladder VARMA (cross orders p=%d q=%d)\n", cx_p, cx_q);
        fprintf(ff, "horizon=%d, freq=%d; each series with its own .pre transformation\n", L, freq);
        /* Con todas las series en logaritmos la cabecera es la de siempre,
           byte a byte; si alguna no lo esta, dice las dos reglas (BUG-0005). */
        int todas_log = 1;
        for (a = 1; a <= act_m; a++)
            if (fabs(Tm[act[a]].boxlam) >= 1e-8) todas_log = 0;
        fprintf(ff, "Level/Low95/High95 in original units. mon%%/ann%% = period/annual "
                    "variation and std (%s).\n\n",
                todas_log ? "100*delta/refactor; % for lambda=0"
                          : "% for lambda=0: 100*delta/refactor; otherwise delta/refactor");
    }
    for (a = 1; a <= act_m; a++) {
        int i = act[a], nb = fc[a].nb;
        /* BUG-0005: un tanto por ciento solo en logaritmos; si no, el cambio
           de la transformada en sus unidades. La regla de fuf, drtran, drvec
           y de la tabla de exactitud de mas abajo. */
        real sc = (fabs(Tm[i].boxlam) < 1e-8 ? 100.0 : 1.0) / Ts[i].refactor;
        FILE *f;
        int  pass;
        ObsToDate(Ts[i].begyear, Ts[i].begtime, nb, freq, &per, &sub);
        for (pass = 0; pass < 2; pass++) {
            f = pass ? ff : out;
            if (!f) continue;
            if (pass == 0)
                fprintf(f, "\n  Forecast of %s from %d/%d (lambda = %g):\n",
                        Ts[i].name, sub, per, Tm[i].boxlam);
            else
                fprintf(f, "Series %d (%s):\n", a, Ts[i].name);
            fprintf(f, "  date   %10s %10s %10s %8s %7s %8s %7s\n",
                    "Level", "Low95", "High95", "mon%", "std", "ann%", "std");
            for (l = 1; l <= L; l++) {
                real *y = fc[a].ystar;
                real g2 = y[nb + l] - y[nb + l - 1];
                real g3 = (nb + l - freq >= 1) ? y[nb + l] - y[nb + l - freq] : 0.0;
                int  p2, s2;
                ObsToDate(Ts[i].begyear, Ts[i].begtime, nb + l, freq, &p2, &s2);
                fprintf(f, "%3d/%4d %10.4f %10.4f %10.4f %8.4f %7.4f %8.4f %7.4f\n",
                        s2, p2, fc[a].lvl[l], fc[a].lo[l], fc[a].hi[l],
                        sc * g2, sc * fc[a].sdp[l], sc * g3, sc * fc[a].sda[l]);
            }
            if (pass) fprintf(f, "\n");
        }
    }
}

/* THE FORECAST REPORT OF fuf (forecast_table_acii, engines/fuf/src/usfo.c),
   series by series, in the .out: the same table drtran writes (its headers,
   labels, columns and four decimals, a space always between fields), so the
   analyst reads the same table in fue, drtran and here. It goes AFTER the
   block above, which stays: lib/outvarma and sim_gui read that one (its
   Low95/High95). The last L+1 observations, with their period and annual
   variation and their exact residual (ERR), then the L forecasts with their
   standard deviations. In % if the model is in logarithms; if not, in the
   units of the series (/refactor), as drtran.                             */
static void write_fuf_report(FILE *out, Fc *fc, int L)
{
    int a, l, obs, per, sub, freq = data_freq;

    for (a = 1; a <= act_m; a++) {
        int   i  = act[a], nb = fc[a].nb;
        real *y  = fc[a].ystar;
        real  vs = (fabs(Tm[i].boxlam) < 1e-8 ? 100.0 : 1.0) / Ts[i].refactor;

        ObsToDate(Ts[i].begyear, Ts[i].begtime, nb, freq, &per, &sub);
        fprintf(out, "\nFORECAST REPORT: \n");
        fprintf(out, "VARIABLE NAME: %s\n", Ts[i].name);
        if (freq > 1) fprintf(out, "FORECAST ORIGIN : %2d/%4d\n", sub, per);
        else          fprintf(out, "FORECAST ORIGIN : %4d\n", per);
        fprintf(out, "LEAD TIME FOR FORECASTING: %d\n\n", L);
        fprintf(out, " +----------------------------------------------------------------------------+\n");
        fprintf(out, " |       |       LEVEL        |              VARIATION              |         |\n");
        fprintf(out, " | DATE  +--------------------+-------------------------------------+   ERR   |\n");
        fprintf(out, " |       |   VALUE   |  STD   | TRIMEST |  STD   |  ANUAL  |  STD   |         |\n");
        fprintf(out, " |       |           |        |  /MONT  |        |         |        |         |\n");
        fprintf(out, " +----------------------------------------------------------------------------+\n");

        for (obs = nb - L; obs <= nb; obs++) {
            if (obs < 1) continue;
            ObsToDate(Ts[i].begyear, Ts[i].begtime, obs, freq, &per, &sub);
            if (freq > 1) fprintf(out, "  %2d/%4d", sub, per);
            else          fprintf(out, "     %4d", per);
            fprintf(out, " %11.4f %8s", Ts[i].data[obs], "-");
            if (obs - 1 >= 1) fprintf(out, " %9.4f %8s", vs * (y[obs] - y[obs - 1]), "-");
            else              fprintf(out, " %9s %8s", "-", "-");
            if (freq > 1 && obs - freq >= 1)
                 fprintf(out, " %9.4f %8s", vs * (y[obs] - y[obs - freq]), "-");
            else fprintf(out, " %9s %8s", "-", "-");
            if (fc[a].has_err[obs - (nb - L)])
                 fprintf(out, " %9.4f\n", vs * fc[a].err[obs - (nb - L)]);
            else fprintf(out, " %9s\n", "-");
        }
        for (l = 1; l <= L; l++) {
            real f2 = y[nb + l] - y[nb + l - 1];
            real f3 = (nb + l - freq >= 1) ? y[nb + l] - y[nb + l - freq] : 0.0;
            ObsToDate(Ts[i].begyear, Ts[i].begtime, nb + l, freq, &per, &sub);
            if (freq > 1) fprintf(out, "  %2d/%4d", sub, per);
            else          fprintf(out, "     %4d", per);
            fprintf(out, " %11.4f %8.4f", fc[a].lvl[l], vs * fc[a].sd[l]);
            fprintf(out, " %9.4f %8.4f", vs * f2, vs * fc[a].sdp[l]);
            if (freq > 1) fprintf(out, " %9.4f %8.4f", vs * f3, vs * fc[a].sda[l]);
            else          fprintf(out, " %9s %8s", "-", "-");
            fprintf(out, " %9s\n", "-");
        }
        fprintf(out, " +----------------------------------------------------------------------------+\n");
    }
}

/* The point forecast from the end of the estimation sample, and, with
   -estwin, the fixed-parameter forecasts from every later origin
   (NAME.recursive, in the format of the .inp path) and their errors.     */
static int esc_forecasts(real *x, real sigma2, int L, int estwin, const char *name)
{
    Fc   fc[ESC_MAX + 1];
    char fname[600];
    FILE *ff, *fr;
    int  a, l, k, k0, rc;

    for (a = 1; a <= act_m; a++) fc_alloc(&fc[a], Ts[act[a]].nobs, L);
    rc = esc_forecast(x, sigma2, L, fc);
    if (rc == 0) {
        snprintf(fname, sizeof fname, "%s.forecast", name);
        ff = fopen(fname, "w");
        fprintf(outputv, "\n=============================================================\n");
        fprintf(outputv, "  FORECASTS (each series back to its level with its .pre model)\n");
        fprintf(outputv, "=============================================================\n");
        write_forecast(outputv, ff, fc, L);
        if (ff) { fclose(ff); printf("Forecasts written to %s\n", fname); }
        write_fuf_report(outputv, fc, L);
    }
    for (a = 1; a <= act_m; a++) fc_free(&fc[a], L);
    if (rc || estwin <= 0) return rc;

    /* Recursive: origins from the end of the estimation window to the end
       of the data, one period at a time, parameters FIXED.                 */
    {
        real *sae[ESC_MAX + 1], *sse[ESC_MAX + 1], *sape[ESC_MAX + 1];
        int  *cnt[ESC_MAX + 1], no = 0;

        snprintf(fname, sizeof fname, "%s.recursive", name);
        fr = fopen(fname, "w");
        if (fr) {
            fprintf(fr, "Recursive fixed-parameter forecasts, ladder VARMA (p=%d q=%d)\n", cx_p, cx_q);
            fprintf(fr, "estwin=%d obs of %s, horizon=%d\n", estwin, Ts[1].name, L);
            fprintf(fr, "origin series horizon level\n");
        }
        for (a = 1; a <= act_m; a++) {
            sae[a] = vector(1, L); sse[a] = vector(1, L); sape[a] = vector(1, L);
            cnt[a] = ivector(1, L);
            for (l = 1; l <= L; l++) { sae[a][l] = sse[a][l] = sape[a][l] = 0.0; cnt[a][l] = 0; }
        }
        k0 = nobs_full[1] - estwin;
        for (k = k0; k >= 0; k--) {
            int per, sub;
            set_origin(k);
            for (a = 1; a <= act_m; a++) fc_alloc(&fc[a], Ts[act[a]].nobs, L);
            if (esc_forecast(x, sigma2, L, fc) == 0) {
                no++;
                ObsToDate(Ts[1].begyear, Ts[1].begtime, Ts[1].nobs, Ts[1].freq, &per, &sub);
                for (a = 1; a <= act_m; a++) {
                    int i = act[a], nb = Ts[i].nobs;
                    for (l = 1; l <= L; l++) {
                        if (fr) fprintf(fr, "%d/%d %s %d %.6f\n", sub, per, Ts[i].name, l, fc[a].lvl[l]);
                        if (nb + l <= nobs_full[i]) {
                            real act_v = Ts[i].data[nb + l], e = act_v - fc[a].lvl[l];
                            sae[a][l] += fabs(e); sse[a][l] += e * e;
                            sape[a][l] += (fabs(act_v) > 1e-12) ? fabs(e / act_v) : 0.0;
                            cnt[a][l]++;
                        }
                    }
                }
            }
            for (a = 1; a <= act_m; a++) fc_free(&fc[a], L);
        }
        set_origin(k0);
        if (fr) { fclose(fr); printf("Recursive forecasts written to %s\n", fname); }

        fprintf(outputv, "\n=============================================================\n");
        fprintf(outputv, "  RECURSIVE FORECAST EVALUATION (out of sample)\n");
        fprintf(outputv, "=============================================================\n");
        fprintf(outputv, "  Parameters estimated ONCE on the estimation window and held\n"
                         "  FIXED while the origin moves forward one period at a time;\n"
                         "  %d origins. The univariate models are the yardstick: compare\n"
                         "  with the diagonal system (p = q = 0, -diagcov).\n", no);
        for (a = 1; a <= act_m; a++) {
            fprintf(outputv, "\n  %s\n    h      n        MAE         RMSE        MAPE(%%)\n", Ts[act[a]].name);
            for (l = 1; l <= L; l++) {
                if (cnt[a][l] == 0) continue;
                fprintf(outputv, "  %3d  %5d  %11.6f  %11.6f  %10.4f\n", l, cnt[a][l],
                        sae[a][l] / cnt[a][l], sqrt(sse[a][l] / cnt[a][l]),
                        100.0 * sape[a][l] / cnt[a][l]);
            }
            free_ivector(cnt[a], 1, L);
            free_vector(sape[a], 1, L); free_vector(sse[a], 1, L); free_vector(sae[a], 1, L);
        }
    }
    return 0;
}

/* ------------------------------------------------------------------------- */
/*  La linea de ordenes                                                      */
/* ------------------------------------------------------------------------- */
/* WHAT a file is, is decided by its CONTENT, not by its name: a file is a
   univariate model of fue -- an .inp specification or a .pre optimum, the
   same format -- if fue's own validator accepts it (engines/fue/src/
   inpcheck.c, compiled here as inp_check_fue, as the fue GUI does). art
   used to refuse a file because it was CALLED .pre, and a renamed one went
   through (DISENO-escalera.md 1.2).                                       */
static int ends_with(const char *s, const char *ext)
{
    size_t n = strlen(s), e = strlen(ext);
    return n > e && strcmp(s + n - e, ext) == 0;
}

static int is_integer(const char *s)
{
    if (*s == '-' || *s == '+') s++;
    if (!*s) return 0;
    for (; *s; s++) if (*s < '0' || *s > '9') return 0;
    return 1;
}

int escalera_requested(int argc, char *argv[])
{
    char why[400];
    FILE *f;
    if (argc < 2 || is_integer(argv[1]) || argv[1][0] == '-') return 0;
    if (inp_check_fue(argv[1], why, sizeof why) == 0) return 1;
    /* A file that exists under its own name and is called .pre or .inp is
       meant as a model: the ladder says what is wrong with it. The .inp
       path takes a name WITHOUT extension and adds it.                   */
    if ((ends_with(argv[1], ".pre") || ends_with(argv[1], ".inp")) &&
        (f = fopen(argv[1], "r")) != NULL) {
        fclose(f);
        return 1;
    }
    return 0;
}

void escalera_usage(const char *prog)
{
    printf("       %s A.pre B.pre [C.pre ...] p q [-diagcov] [-redet] [-fixarma]\n", prog);
    printf("                                  [-m method] [-o NAME] [-forecast H [-estwin N]]\n");
    printf("                                  [-irfboot N] [-det LIST] [-detbase file|gate]\n");
    printf("                                  [-route u|v] [-factortest] [-echelon n1,n2,...]\n");
    printf("                                  [-hessian fd|bfgs] [-lik elf|shea|both]\n");
    printf("                                  [-links \"A<-B,C<-A\"] [-zero \"AR1[A<-B],...\"]\n");
    printf("                                  [-start zero|ols]\n");
    printf("  THE LADDER: each series comes with its univariate model from fue (.pre):\n");
    printf("       Box-Cox, deterministic terms, differencing, mean and ARMA factors.\n");
    printf("       The VARMA keeps each model on its DIAGONAL; p and q are the orders of\n");
    printf("       the CROSS dynamics. p = q = 0 with -diagcov reproduces the univariates.\n");
    printf("  -diagcov : diagonal innovation covariance (default: full)\n");
    printf("  -route   : u (default), the ladder: each series keeps its univariate ARMA\n"
           "             and the cross terms are added. v, the vector first: past the\n"
           "             gate, each series' regular ARMA is replaced by a free\n"
           "             AR(p)/MA(q) -- the whole VARMA(p,q) matrix is free -- and its\n"
           "             seasonal and fixed-frequency factors multiply its row. The LR\n"
           "             is then against the same model without cross terms\n");
    printf("  -echelon : the echelon form (Lutkepohl and Poskitt), on route V: one\n"
           "             Kronecker index per series, in the order of the files. Row r\n"
           "             has its own AR(n_r)/MA(n_r); the indices say which cross lags\n"
           "             and which contemporaneous terms (AR0[r<-c]) exist. Equal\n"
           "             indices are the VARMA(n,n). Give 0 0 as cross orders\n");
    printf("  -echelon search : the Kronecker indices from the data (Lutkepohl and\n"
           "             Poskitt's stages I and II): a long VAR, then least squares\n"
           "             per candidate and a criterion (-criterion SC|HQ, default\n"
           "             SC), indices up to -nmax N (default 3). It gives the table\n"
           "             and stops: no maximum likelihood. Give 0 0 as cross orders\n");
    printf("  -factortest : on route V, the LR of the row factor: the fitted model\n"
           "             against the same rows with every position the factor reaches\n"
           "             free (Tiao and Box's caveat on univariate operators)\n");
    printf("  -redet   : re-estimate the deterministic terms (default: fixed); it is -det all\n");
    printf("  -det LIST: re-estimate only some: all, none, a class (seasonal: cos sin\n"
           "             alter; calendar: easter; interv: impulse compimp step ramp;\n"
           "             trend), SERIES:class, or a coefficient's name as the output\n"
           "             writes it (\"w0_IPC[step 8 1990]\"), separated by commas. Only\n"
           "             what the file marks as free is freed\n");
    printf("  -detbase : where the fixed ones stay: file (the file's values) or gate\n"
           "             (estimated at the gate, series by series, on the common\n"
           "             window, then held). Default: gate for a series the common\n"
           "             window trims, file for the rest\n");
    printf("  -fixarma : keep the univariate ARMA factors fixed at the .pre\n");
    printf("  -links   : cross dynamics only on these pairs, \"A<-B\" = B enters the\n"
           "             equation of A (AR and MA, every lag up to p and q); names as\n"
           "             in the .pre files. Default: every pair\n");
    printf("  -zero    : single cross coefficients held at zero, \"AR1[A<-B],MA2[B<-A]\"\n"
           "             (Tiao and Box's simplification by coefficient); each one has\n"
           "             to be in the model: its lag within p or q, its pair linked\n");
    printf("  -start   : where the cross terms start: zero, or ols, the original\n"
           "             drvarma's init_varma: least squares of the VAR(p) seeds\n"
           "             the cross AR and Q's covariances; the own models stay\n"
           "             the univariate ones (the ladder's principle).\n"
           "             Default: ols with q = 0, zero with q > 0\n");
    printf("  -hessian : standard errors from fdhess at the optimum (fd, default) or\n"
           "             from the BFGS Hessian of the search (bfgs)\n");
    printf("  -o NAME  : results to NAME.out (default: the .pre names joined by '_')\n");
    printf("  -irfboot N  : bands for the impulse responses by parametric bootstrap: N\n"
           "                simulations of the fitted model, each refitted; Efron's\n"
           "                percentiles to the .out and to NAME.irfboot (not with -redet)\n");
    printf("  -forecast H : forecast H periods -> NAME.forecast, each series in its level\n");
    printf("  -estwin N   : estimate on the first N observations of the first series,\n");
    printf("                then fixed-parameter forecasts from every origin to the end\n");
    printf("                -> NAME.recursive and an out-of-sample evaluation\n");
}

static void base_of(const char *path, char *out, size_t n)
{
    const char *b = strrchr(path, '/');
    size_t len;
    b = b ? b + 1 : path;
    len = strlen(b);
    if (len > 4 && (strcmp(b + len - 4, ".pre") == 0 || strcmp(b + len - 4, ".inp") == 0)) len -= 4;
    if (len >= n) len = n - 1;
    memcpy(out, b, len); out[len] = '\0';
}

/* ------------------------------------------------------------------------- */
/*  El programa                                                              */
/* ------------------------------------------------------------------------- */
int escalera_main(int argc, char *argv[])
{
    char  outname[512] = "", outfile[520], why[600];
    int   irfboot_n = 0;
    const char *links_arg = NULL;
    const char *zero_arg = NULL, *ech_arg = NULL;
    int   ech_busca = 0, ech_nmax = 3, ech_sc = 1;
    int   i, a, argi, met_esc = 1, p, q, rc = 0, fc_h = 0, estwin = 0;
    real  logL_uni[ESC_MAX + 1], s2_uni[ESC_MAX + 1], move[ESC_MAX + 1];
    real  sum_uni = 0.0, logL_gate, logL_diag;
    Fit   F, D;

    /* [1] Los .pre, y detras p y q */
    n_ser = 0;
    for (argi = 1; argi < argc && !is_integer(argv[argi]) && argv[argi][0] != '-'; argi++) {
        char why[400];
        if (n_ser == ESC_MAX) {
            printf("ERROR: at most %d series\n", ESC_MAX);
            return 1;
        }
        if (inp_check_fue(argv[argi], why, sizeof why) != 0) {
            printf("ERROR: %s is not a univariate model file of fue: %s\n",
                   argv[argi], why);
            if (ends_with(argv[argi], ".inp"))
                printf("       If it is a multivariate drvarma .inp (deprecated since 5.0),\n"
                       "       give its name without .inp, or convert it:\n"
                       "         drvarma -split %s\n", argv[argi]);
            return 2;
        }
        pre_path[++n_ser] = argv[argi];
    }
    if (argi + 1 >= argc) {
        printf("drvarma %s -- ladder mode\nUsage:\n", DRVARMA_VERSION_FULL);
        escalera_usage(argv[0]);
        return 1;
    }
    p = atoi(argv[argi]);
    q = atoi(argv[argi + 1]);
    if (p < 0 || q < 0 || p > ESC_MAXLAG || q > ESC_MAXLAG) {
        printf("ERROR: cross orders p=%d q=%d out of range 0..%d\n", p, q, ESC_MAXLAG);
        return 1;
    }
    cx_diagcov = 0; opt_fixarma = 0; det_lista[0] = 0; det_base = 0; det_etapa = 1;
    opt_route_v = 0; v_on = 0; opt_factortest = 0; ech_on = 0; ech_arg = NULL;
    memset(cA0, 0, sizeof cA0);
    est_fdhess = 1;          /* standard errors from the Hessian AT the optimum */
    for (argi += 2; argi < argc; argi++) {
        if      (strcmp(argv[argi], "-diagcov") == 0) cx_diagcov = 1;
        else if (strcmp(argv[argi], "-redet") == 0)   snprintf(det_lista, sizeof det_lista, "all");
        else if (strcmp(argv[argi], "-det") == 0 && argi + 1 < argc)
            snprintf(det_lista, sizeof det_lista, "%s", argv[++argi]);
        else if (strcmp(argv[argi], "-detbase") == 0 && argi + 1 < argc &&
                 (strcmp(argv[argi + 1], "file") == 0 || strcmp(argv[argi + 1], "gate") == 0))
            det_base = strcmp(argv[++argi], "gate") == 0 ? 2 : 1;
        else if (strcmp(argv[argi], "-fixarma") == 0) opt_fixarma = 1;
        else if (strcmp(argv[argi], "-factortest") == 0) opt_factortest = 1;
        else if (strcmp(argv[argi], "-echelon") == 0 && argi + 1 < argc &&
                 strcmp(argv[argi + 1], "search") == 0) { argi++; ech_busca = 1; }
        else if (strcmp(argv[argi], "-nmax") == 0 && argi + 1 < argc) ech_nmax = atoi(argv[++argi]);
        else if (strcmp(argv[argi], "-criterion") == 0 && argi + 1 < argc &&
                 (strcmp(argv[argi + 1], "SC") == 0 || strcmp(argv[argi + 1], "HQ") == 0))
            ech_sc = (strcmp(argv[++argi], "SC") == 0);
        else if (strcmp(argv[argi], "-echelon") == 0 && argi + 1 < argc) {
            ech_arg = argv[++argi];  opt_route_v = 1;      /* la echelon va por la ruta V */
        }
        else if (strcmp(argv[argi], "-route") == 0 && argi + 1 < argc &&
                 (strcmp(argv[argi + 1], "u") == 0 || strcmp(argv[argi + 1], "v") == 0))
            opt_route_v = (strcmp(argv[++argi], "v") == 0);
        else if (strcmp(argv[argi], "-m") == 0 && argi + 1 < argc) met_esc = atoi(argv[++argi]);
        else if (strcmp(argv[argi], "-o") == 0 && argi + 1 < argc)
            snprintf(outname, sizeof outname, "%s", argv[++argi]);
        else if (strcmp(argv[argi], "-irfboot") == 0 && argi + 1 < argc)
            irfboot_n = atoi(argv[++argi]);
        else if (strcmp(argv[argi], "-forecast") == 0 && argi + 1 < argc)
            fc_h = atoi(argv[++argi]);
        else if (strcmp(argv[argi], "-estwin") == 0 && argi + 1 < argc)
            estwin = atoi(argv[++argi]);
        else if (strcmp(argv[argi], "-links") == 0 && argi + 1 < argc)
            links_arg = argv[++argi];
        else if (strcmp(argv[argi], "-zero") == 0 && argi + 1 < argc)
            zero_arg = argv[++argi];
        else if (strcmp(argv[argi], "-lik") == 0 && argi + 1 < argc &&
                 (strcmp(argv[argi + 1], "elf") == 0 || strcmp(argv[argi + 1], "shea") == 0
                  || strcmp(argv[argi + 1], "both") == 0)) {
            argi++;
            est_lik = strcmp(argv[argi], "shea") == 0 ? LIK_SHEA
                    : strcmp(argv[argi], "both") == 0 ? LIK_BOTH : LIK_ELF;
            }
        else if (strcmp(argv[argi], "-start") == 0 && argi + 1 < argc &&
                 (strcmp(argv[argi + 1], "ols") == 0 || strcmp(argv[argi + 1], "zero") == 0))
            opt_start_ols = (strcmp(argv[++argi], "ols") == 0);
        else if (strcmp(argv[argi], "-start") == 0 && argi + 1 < argc &&
                 strcmp(argv[argi + 1], "echelon") == 0) { argi++; opt_start_ols = 2; }
        else if (strcmp(argv[argi], "-hessian") == 0 && argi + 1 < argc &&
                 (strcmp(argv[argi + 1], "fd") == 0 || strcmp(argv[argi + 1], "bfgs") == 0))
            est_fdhess = (strcmp(argv[++argi], "fd") == 0);
        else {
            /* Lo que la via .inp admite y la escalera todavia no: mejor un
               error que una opcion ignorada en silencio.                   */
            printf("ERROR: option %s is not available in ladder mode (.pre input)\n",
                   argv[argi]);
            printf("       -mean, -deseason, -scale and the transformation come from\n"
                   "       each .pre.\n");
            return 1;
        }
    }
    int want_diagcov = cx_diagcov;
    if (ech_busca && (p != 0 || q != 0 || ech_nmax < 0 || ech_nmax > ESC_MAXLAG)) {
        printf("ERROR: -echelon search: give 0 0 as cross orders, and -nmax between 0 and %d\n",
               ESC_MAXLAG);
        return 1;
    }
    if (opt_start_ols == 2 && !ech_arg) {
        printf("ERROR: -start echelon is the echelon form's: give -echelon n1,n2,...\n");
        return 1;
    }
    if (opt_route_v && opt_fixarma) {
        printf("ERROR: -route v with -fixarma: on route V the own ARMA is the vector's,\n"
               "       free by definition; there is no univariate ARMA to hold.\n");
        return 1;
    }
    if (opt_factortest && !opt_route_v) {
        printf("ERROR: -factortest is route V's (-route v): it tests the row factor.\n");
        return 1;
    }
    if (opt_route_v && !ech_arg && p == 0 && q == 0) {
        printf("ERROR: -route v needs p or q above zero: it is the full VARMA(p,q).\n");
        return 1;
    }
    if (irfboot_n < 0 || (irfboot_n > 0 && det_lista[0] && strcmp(det_lista, "none") != 0)) {
        printf("ERROR: -irfboot N needs N >= 1, and not -redet or -det (the stationary\n"
               "       series are rebuilt from the data at each estimation)\n");
        return 1;
    }
    if (fc_h < 0 || (estwin > 0 && fc_h <= 0)) {
        printf("ERROR: -estwin needs a horizon: give -forecast H\n");
        return 1;
    }
    xitol_met = (met_esc == 2) ? -1.0e-3 : 1.0e-3;

    if (outname[0] == '\0') {
        for (i = 1; i <= n_ser; i++) {
            char b[200];
            base_of(pre_path[i], b, sizeof b);
            if (i > 1) strncat(outname, "_", sizeof outname - strlen(outname) - 1);
            strncat(outname, b, sizeof outname - strlen(outname) - 1);
        }
    }
    snprintf(outfile, sizeof outfile, "%s.out", outname);

    /* [2] Leer, alinear, transformar */
    for (i = 1; i <= n_ser; i++) {
        if (read_fue_pre(pre_path[i], &Tm[i], &Ts[i], &DataMat[i]) != 0) {
            printf("ERROR: cannot read %s\n", pre_path[i]);
            return 2;
        }
        ar_deg[i] = total_ar_order(&Tm[i]);
        ma_deg[i] = total_ma_order(&Tm[i]);
        mu[i] = Tm[i].mu;
    }
    if (fuepre_check_alignment(Ts, n_ser, why, sizeof why) != 0) {
        printf("ERROR: %s\n", why);
        return 4;
    }
    fuepre_warn_refactors(Ts, n_ser, stderr);
    for (i = 1; i <= n_ser; i++) nobs_full[i] = Ts[i].nobs;

    /* -links, resolved against the names of the .pre files */
    opt_links = 0;
    if (links_arg) {
        char buf[1024], *tok, *save = NULL;
        int  j;
        if (p == 0 && q == 0) {
            printf("ERROR: -links needs cross dynamics: give p or q > 0\n");
            return 1;
        }
        memset(cx_link, 0, sizeof cx_link);
        snprintf(buf, sizeof buf, "%s", links_arg);
        for (tok = strtok_r(buf, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
            char *arrow = strstr(tok, "<-"), *lhs = tok, *rhs;
            int   ti = 0, tj = 0;
            if (!arrow) {
                printf("ERROR: -links: '%s' is not of the form A<-B\n", tok);
                return 1;
            }
            *arrow = '\0'; rhs = arrow + 2;
            while (*lhs == ' ') lhs++;
            while (*rhs == ' ') rhs++;
            for (j = (int) strlen(lhs) - 1; j >= 0 && lhs[j] == ' '; j--) lhs[j] = '\0';
            for (j = (int) strlen(rhs) - 1; j >= 0 && rhs[j] == ' '; j--) rhs[j] = '\0';
            for (j = 1; j <= n_ser; j++) {
                if (strcmp(Ts[j].name, lhs) == 0) ti = j;
                if (strcmp(Ts[j].name, rhs) == 0) tj = j;
            }
            if (!ti || !tj || ti == tj) {
                printf("ERROR: -links: '%s<-%s' does not name two different series\n", lhs, rhs);
                return 1;
            }
            cx_link[ti][tj] = 1;
        }
        opt_links = 1;
    }

    /* -zero, resolved against the names of the .pre files: each coefficient
       has to exist in the model asked for (its lag within p or q, its pair
       linked), or it is an error -- never a restriction ignored in silence. */
    /* -echelon n1,n2,...: un indice por serie, en el orden de los ficheros.
       Los ordenes cruzados son el mayor de ellos (p y q se dan 0 0, o ese). */
    if (ech_arg) {
        char buf[256], *tok, *save = NULL;
        int  nn = 0, nmax = 0;
        snprintf(buf, sizeof buf, "%s", ech_arg);
        for (tok = strtok_r(buf, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
            char *fin;
            long v = strtol(tok, &fin, 10);
            while (*fin == ' ') fin++;
            if (*fin || v < 0 || v > ESC_MAXLAG || nn >= n_ser) { nn = -1; break; }
            ech_n[++nn] = (int)v;
            if ((int)v > nmax) nmax = (int)v;
        }
        if (nn != n_ser) {
            printf("ERROR: -echelon: give %d Kronecker indices between 0 and %d, one per\n"
                   "       series in the order of the files, separated by commas\n", n_ser, ESC_MAXLAG);
            return 1;
        }
        if (!((p == 0 && q == 0) || (p == nmax && q == nmax))) {
            printf("ERROR: -echelon: the cross orders are the largest index, %d: give 0 0\n", nmax);
            return 1;
        }
        p = q = nmax;
        ech_on = 1;
    }

    opt_zero = 0;
    memset(cx_zar, 0, sizeof cx_zar);
    memset(cx_zma, 0, sizeof cx_zma);
    memset(v_zar, 0, sizeof v_zar);  memset(v_zma, 0, sizeof v_zma);  v_nzeros = 0;
    if (zero_arg) {
        char buf[2048], *tok, *save = NULL;
        if (p == 0 && q == 0) {
            printf("ERROR: -zero needs cross dynamics: give p or q > 0\n");
            return 1;
        }
        snprintf(buf, sizeof buf, "%s", zero_arg);
        for (tok = strtok_r(buf, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
            char kind[3] = "", lhs[ESC_NAMELEN] = "", rhs[ESC_NAMELEN] = "";
            int  k = 0, ti = 0, tj = 0, j, top;
            while (*tok == ' ') tok++;
            {   /* lo propio: phi_SERIE[B^k] o theta_SERIE[B^k], solo en la ruta V */
                char pt[8] = "", ser[ESC_NAMELEN] = "";
                int  kk = 0, ti2 = 0, j2;
                if (sscanf(tok, "%5[a-z]_%63[^[][B^%d]", pt, ser, &kk) == 3 &&
                    (strcmp(pt, "phi") == 0 || strcmp(pt, "theta") == 0)) {
                    int ar = strcmp(pt, "phi") == 0, tope;
                    if (!opt_route_v) {
                        printf("ERROR: -zero %s: an own coefficient can be held at zero on route V\n"
                               "       (-route v, -echelon), where the own part is the vector's. On\n"
                               "       route U it is the univariate model's, and its zeros are art's.\n", tok);
                        return 1;
                    }
                    for (j2 = 1; j2 <= n_ser; j2++) if (strcmp(Ts[j2].name, ser) == 0) ti2 = j2;
                    tope = ti2 ? (ech_on ? ech_n[ti2] : (ar ? p : q)) : 0;
                    if (!ti2 || kk < 1 || kk > tope) {
                        printf("ERROR: -zero: %s is not an own coefficient of the model (%s order %d)\n",
                               tok, ti2 ? Ts[ti2].name : "no such series;", tope);
                        return 1;
                    }
                    if (ar) v_zar[ti2][kk] = 1; else v_zma[ti2][kk] = 1;
                    v_nzeros++;
                    continue;
                }
            }
            if (sscanf(tok, "%2[ARM]%d[%63[^<]<-%63[^]]]", kind, &k, lhs, rhs) != 4 ||
                (strcmp(kind, "AR") && strcmp(kind, "MA"))) {
                printf("ERROR: -zero: '%s' is not of the form AR1[A<-B] or MA1[A<-B]\n", tok);
                return 1;
            }
            top = strcmp(kind, "AR") == 0 ? p : q;
            if (k < 1 || k > top) {
                printf("ERROR: -zero: %s lag %d is not in the model (%s order %d)\n",
                       kind, k, kind, top);
                return 1;
            }
            for (j = 1; j <= n_ser; j++) {
                if (strcmp(Ts[j].name, lhs) == 0) ti = j;
                if (strcmp(Ts[j].name, rhs) == 0) tj = j;
            }
            if (!ti || !tj || ti == tj) {
                printf("ERROR: -zero: '%s<-%s' does not name two different series\n", lhs, rhs);
                return 1;
            }
            if (!linked(ti, tj)) {
                printf("ERROR: -zero: %s<-%s is not a linked pair (-links)\n", lhs, rhs);
                return 1;
            }
            if (strcmp(kind, "AR") == 0) cx_zar[k][ti][tj] = 1;
            else                          cx_zma[k][ti][tj] = 1;
        }
        opt_zero = 1;
    }

    /* La forma echelon: su tabla de indices sobre la mascara de los ceros.
       Los AR cruzados i <- j, solo en n_i - n_ij + 1 .. n_i; los MA, en
       1 .. n_i.                                                            */
    if (ech_on) {
        int k, j;
        for (i = 1; i <= n_ser; i++)
            for (j = 1; j <= n_ser; j++) {
                if (i == j) continue;
                for (k = 1; k <= ESC_MAXLAG; k++) {
                    if (k < ech_n[i] - ech_nrc(i, j) + 1 || k > ech_n[i]) cx_zar[k][i][j] = 1;
                    if (k > ech_n[i]) cx_zma[k][i][j] = 1;
                }
            }
    }

    /* -estwin N: the parameters are estimated on the first N observations of
       the FIRST series -- the others are cut at the same date -- and then
       held fixed while the forecast origin moves to the end of the data.   */
    if (estwin > 0) {
        if (estwin > nobs_full[1] || estwin < 2) {
            printf("ERROR: -estwin %d out of range (1..%d)\n", estwin, nobs_full[1]);
            return 1;
        }
        for (i = 1; i <= n_ser; i++)
            if (nobs_full[i] - (nobs_full[1] - estwin) < 2) {
                printf("ERROR: -estwin %d leaves %s without data\n", estwin, Ts[i].name);
                return 1;
            }
        set_origin(nobs_full[1] - estwin);
    }
    if (build_stationary() != 0) {
        printf("ERROR: could not build the stationary series\n");
        return 2;
    }
    if (det_monta() != 0) return 1;

    /* Las globales que leen los diagnosticos de drvarma. Los residuos
       arrancan donde arranca la ventana comun: su fecha se da directamente,
       sin desplazamiento por diferencias.                                  */
    {
        int by, bs;
        ObsToDate(Ts[1].begyear, Ts[1].begtime, Ts[1].nobs - n_stat + 1,
                  Ts[1].freq, &by, &bs);
        data_freq = Ts[1].freq;  data_start_year = by;  data_start_sub = bs;
        trans_d = 0;  trans_D = 0;
    }
    series_names = (char **) malloc((n_ser + 1) * sizeof(char *));
    for (i = 1; i <= n_ser; i++) series_names[i] = Ts[i].name;

    if (NULL == (outputv = fopen(outfile, "w"))) {
        printf("ERROR: cannot create %s\n", outfile);
        return 1;
    }
    macheps = cmacheps();

    {
        int ey, es, by = data_start_year, bs = data_start_sub;
        ObsToDate(Ts[1].begyear, Ts[1].begtime, Ts[1].nobs, Ts[1].freq, &ey, &es);
        for (i = 0; i < 2; i++) {
            FILE *f = i ? outputv : stdout;
            fprintf(f, "Program          : drvarma %s (ladder mode: .pre input)\n", DRVARMA_VERSION_FULL);
            fprintf(f, "Output File      : %s\n", outfile);
            if (ech_on) {
                int k3;
                fprintf(f, "Model            : echelon form, Kronecker indices");
                for (k3 = 1; k3 <= n_ser; k3++) fprintf(f, "%s%d", k3 > 1 ? "," : " ", ech_n[k3]);
                fprintf(f, " (route V; the univariates' seasonal factors on the rows)\n");
            } else if (opt_route_v)
                fprintf(f, "Model            : VARMA(%d,%d), route V: the whole matrix free, the "
                           "univariates' seasonal factors on the rows\n", p, q);
            else
            fprintf(f, "Model            : VARMA with univariate diagonals; cross orders p=%d q=%d\n", p, q);
            if (opt_links) {
                int li, lj, first = 1;
                fprintf(f, "Cross links      : ");
                for (li = 1; li <= n_ser; li++)
                    for (lj = 1; lj <= n_ser; lj++)
                        if (li != lj && cx_link[li][lj]) {
                            fprintf(f, "%s%s<-%s", first ? "" : ", ", Ts[li].name, Ts[lj].name);
                            first = 0;
                        }
                fprintf(f, "\n");
            }
            if (opt_zero) {
                int li, lj, k, first = 1;
                fprintf(f, "Zero coefficients: ");
                for (k = 1; k <= p; k++)
                    for (li = 1; li <= n_ser; li++)
                        for (lj = 1; lj <= n_ser; lj++)
                            if (cx_zar[k][li][lj] && li != lj &&      /* no los que quita la tabla de indices */
                                !(ech_on && (k < ech_n[li] - ech_nrc(li, lj) + 1 || k > ech_n[li]))) {
                                fprintf(f, "%sAR%d[%s<-%s]", first ? "" : ", ", k, Ts[li].name, Ts[lj].name);
                                first = 0;
                            }
                for (k = 1; k <= q; k++)
                    for (li = 1; li <= n_ser; li++)
                        for (lj = 1; lj <= n_ser; lj++)
                            if (cx_zma[k][li][lj] && li != lj && !(ech_on && k > ech_n[li])) {
                                fprintf(f, "%sMA%d[%s<-%s]", first ? "" : ", ", k, Ts[li].name, Ts[lj].name);
                                first = 0;
                            }
                for (li = 1; li <= n_ser; li++)                      /* los propios (ruta V) */
                    for (k = 1; k <= ESC_MAXLAG; k++) {
                        if (v_zar[li][k]) { fprintf(f, "%sphi_%s[B^%d]", first ? "" : ", ", Ts[li].name, k); first = 0; }
                        if (v_zma[li][k]) { fprintf(f, "%stheta_%s[B^%d]", first ? "" : ", ", Ts[li].name, k); first = 0; }
                    }
                fprintf(f, "\n");
            }
            fprintf(f, "Innovation cov.  : %s\n", want_diagcov ? "diagonal" : "full");
            {   /* "re-estimated" y "fixed at the .pre" son las de siempre (las lee
                   lib/outvarma); lo demas dice que se estima y de donde sale lo fijo */
                int nj = 0, ng = 0, nt = 0, k2;
                for (k2 = 1; k2 <= n_ser; k2++) {
                    nj += det_n_etapa(k2, 1); ng += det_n_etapa(k2, 0); nt += n_det_free_params(&Tm[k2]);
                }
                if (nj == nt && nt > 0)     fprintf(f, "Deterministics   : re-estimated\n");
                else if (nj > 0)            fprintf(f, "Deterministics   : re-estimated: %s (%d of %d); "
                                                       "the rest fixed at %s\n", det_lista, nj, nt,
                                                    ng > nj ? "the gate's values" : "the .pre");
                else if (ng > 0)            fprintf(f, "Deterministics   : fixed at the gate's values "
                                                       "(%d estimated there, series by series)\n", ng);
                else                        fprintf(f, "Deterministics   : fixed at the .pre\n");
            }
            fprintf(f, "Univariate ARMA  : %s\n", opt_route_v ? "replaced by the vector's (route V)"
                    : opt_fixarma ? "fixed at the .pre" : "re-estimated jointly");
            fprintf(f, "Estimation method: %d\n", met_esc);
            fprintf(f, "Likelihood       : %s\n", lik_label());
            fprintf(f, "Frequency        : %d\n", data_freq);
            fprintf(f, "Common window    : %d/%d - %d/%d  (%d stationary observations)\n",
                    bs, by, es, ey, n_stat);
            if (estwin > 0) {
                int fy, fs;
                ObsToDate(Ts[1].begyear, Ts[1].begtime, nobs_full[1], Ts[1].freq, &fy, &fs);
                fprintf(f, "Estimation window: the first %d observations of %s; parameters\n"
                           "                   fixed, forecasts from every origin to %d/%d\n",
                        estwin, Ts[1].name, fs, fy);
            }
            if (fc_h > 0) fprintf(f, "Forecast horizon : %d\n", fc_h);
            fprintf(f, "\n");
            print_series_table(f);
            fprintf(f, "\n");
        }
    }

    /* [3] LA PUERTA.  Cada serie sola, sobre la misma ventana y con el mismo
       cast; luego el sistema diagonal EVALUADO en esos optimos. La identidad
       es exacta: con Q diagonal y Q_ii/Q_11 = s2_i/s2_1 el sistema factoriza
       y logL = SUM logL_i. Se evalua, no se optimiza: lo que se certifica es
       la verosimilitud y el cast, no la convergencia de un optimizador.    */
    /* The gate is an IDENTITY, so it is evaluated with the UNTRUNCATED
       likelihood whatever -m says (2026-09-28). With -m 1 elf truncates the xi
       sequence at 1e-3, and the truncation is not the same for one series as
       for m: m6's pairs failed by up to 0.0018 and the report blamed the cast.
       fue estimates its .pre untruncated too, so "move" is measured on the
       same likelihood. The estimation below uses the method asked for.      */
    {
    real xitol_asked = xitol_met;
    xitol_met = -1.0e-3;
    quiet_mode = 1;
    cx_p = cx_q = 0;  cx_diagcov = 1;
    det_etapa = 0;                       /* la puerta: su mascara */
    for (i = 1; i <= n_ser; i++) {
        real *x0, *x1;
        int   n0, k;
        act_m = 1; act[1] = i;

        n0 = esc_npar();
        x0 = vector(1, n0 > 0 ? n0 : 1);
        esc_pack(x0);
        fit_run(&F, 1);
        if (F.ifault) {
            fprintf(outputv, "\n**** univariate fit of %s FAILED: %s (code %d)\n",
                    Ts[i].name, fault_msg(F.ifault), F.ifault);
            printf("ERROR: univariate fit of %s failed: %s\n", Ts[i].name, fault_msg(F.ifault));
            fit_free(&F); free_vector(x0, 1, n0 > 0 ? n0 : 1);
            fclose(outputv);
            return 3;
        }
        x1 = F.x;
        move[i] = 0.0;
        for (k = 1; k <= n0; k++)
            if (fabs(x1[k] - x0[k]) > move[i]) move[i] = fabs(x1[k] - x0[k]);
        logL_uni[i] = F.logL;
        s2_uni[i]   = F.sigma2;
        sum_uni    += F.logL;
        fit_free(&F);
        free_vector(x0, 1, n0 > 0 ? n0 : 1);
    }

    det_etapa = 1;                       /* el conjunto: la suya */
    act_m = n_ser;
    for (a = 1; a <= n_ser; a++) act[a] = a;
    for (a = 1; a <= n_ser; a++) lvar[a] = log(s2_uni[a] / s2_uni[1]);
    fit_run(&D, 0);
    logL_gate = D.logL;
    xitol_met = xitol_asked;
    }
    {
        int pass = (D.ifault == 0) &&
                   fabs(logL_gate - sum_uni) <= GATE_TOL * (1.0 + fabs(sum_uni));
        for (i = 0; i < 2; i++) {
            FILE *f = i ? outputv : stdout;
            int k;
            fprintf(f, "=============================================================\n");
            fprintf(f, "  THE DIAGONAL GATE\n");
            fprintf(f, "  Each series alone (same window, same cast), then the diagonal\n");
            fprintf(f, "  system evaluated at those optima: the two must agree exactly.\n");
            fprintf(f, "=============================================================\n");
            fprintf(f, "  %-14s %16s %14s %14s\n", "series", "logL", "sigma2", "max|move|");
            for (k = 1; k <= n_ser; k++)
                fprintf(f, "  %-14s %16.6f %14.6g %14.3g%s\n", Ts[k].name, logL_uni[k],
                        s2_uni[k], move[k],
                        move[k] <= PRE_MOVE ? "" : n_trim[k] > 0 ? "  <- trimmed sample"
                                                                 : "  <- not an optimum");
            fprintf(f, "  %-14s %16.6f\n", "SUM", sum_uni);
            fprintf(f, "  %-14s %16.6f\n", "joint diagonal", logL_gate);
            fprintf(f, "  %-14s %16.3g\n", "difference", logL_gate - sum_uni);
            fprintf(f, "  GATE: %s\n", pass ? "PASSED" : "FAILED");
            for (k = 1; k <= n_ser; k++)
                if (move[k] > PRE_MOVE && n_trim[k] > 0)
                {
                    fprintf(f, "  Note: %s moved %.3g from its .pre, but the common window\n"
                               "        drops its first %d stationary observations: it is\n"
                               "        re-estimated on a different sample, and moving is legitimate.\n",
                            Ts[k].name, move[k], n_trim[k]);
                    if (det_n_etapa(k, 0) > det_n_etapa(k, 1))
                        fprintf(f, "        Its deterministic terms were re-estimated here too, on the\n"
                                   "        common window, and are held at those values (-detbase file\n"
                                   "        keeps the file's).\n");
                    else if (det_n_etapa(k, 0) < n_det_free_params(&Tm[k]))
                        fprintf(f, "        Its deterministic terms stay at the file's values (-detbase file).\n");
                }
                else if (move[k] > PRE_MOVE)
                {
                    fprintf(f, "  Note: %s moved %.3g from its .pre on its own sample. A genuine\n"
                               "        .pre is a fixed point (it moves ~1e-5, the rounding of its six\n"
                               "        decimals): this one is a specification, not an optimum.\n",
                            Ts[k].name, move[k]);
                    if (det_n_etapa(k, 0) < n_det_free_params(&Tm[k]))
                        fprintf(f, "        Its deterministic terms stayed at their SEEDS: in a\n"
                                   "        specification they are starting values -- use -redet,\n"
                                   "        or -detbase gate to estimate them here and hold them.\n");
                }
            fprintf(f, "\n");
        }
        if (!pass) {
            printf("ERROR: the diagonal gate failed (difference %.3g). The joint cast does\n"
                   "       not reproduce the univariate models; nothing on top of it can be\n"
                   "       trusted.\n", logL_gate - sum_uni);
            fit_free(&D);
            fclose(outputv);
            return 5;
        }
    }
    fit_free(&D);

    /* [3a] -echelon search: pasada la puerta, la tabla de los indices, y nada
       mas. No estima por maxima verosimilitud.                            */
    if (ech_busca) {
        rc = ech_search(outputv, ech_nmax, ech_sc);
        printf("Full results written to %s\n", outfile);
        fclose(outputv);
        return rc ? 1 : 0;
    }

    /* [3b] La ruta V: pasada la puerta, lo propio pasa a ser el AR(p)/MA(q)
       libre del vector, y lo demas del univariante, el factor de su fila. */
    if (opt_route_v) {
        int k2;
        v_sustituye(p, q);
        for (i = 0; i < 2; i++) {
            FILE *f = i ? outputv : stdout;
            fprintf(f, "=============================================================\n");
            if (ech_on) {
                fprintf(f, "  THE ECHELON FORM (route V), Kronecker indices");
                for (k2 = 1; k2 <= n_ser; k2++) fprintf(f, "%s%d", k2 > 1 ? "," : " ", ech_n[k2]);
                fprintf(f, "\n  Past the gate, each series' regular ARMA is replaced by a free\n");
                fprintf(f, "  AR(n)/MA(n), n its index; the indices say which cross terms\n");
                fprintf(f, "  exist. Its seasonal and fixed-frequency factors multiply its\n");
                fprintf(f, "  whole row. The order of the series is part of the model.\n");
            } else {
            fprintf(f, "  ROUTE V: THE VECTOR FIRST\n");
            fprintf(f, "  Past the gate, each series' regular ARMA is replaced by a free\n");
            fprintf(f, "  AR(%d)/MA(%d), the vector's. Its seasonal and fixed-frequency\n", p, q);
            fprintf(f, "  factors multiply its whole row, cross terms included.\n");
            }
            fprintf(f, "=============================================================\n");
            fprintf(f, "  %-14s %22s %22s\n", "series", "univariate, regular", "row factor (degrees)");
            for (k2 = 1; k2 <= n_ser; k2++)
                fprintf(f, "  %-14s          AR(%d) MA(%d)           AR %d  MA %d\n", Ts[k2].name,
                        v_uni_ar[k2], v_uni_ma[k2], v_fac_ar[k2], v_fac_ma[k2]);
            for (k2 = 1; k2 <= n_ser; k2++) {
                if (v_uni_ar[k2] > v_reg_ar[k2])
                    fprintf(f, "  Note: %s's univariate model has a regular AR(%d): this model,\n"
                               "        with %s = %d, does not contain it.\n", Ts[k2].name, v_uni_ar[k2],
                            ech_on ? "its index" : "p", v_reg_ar[k2]);
                if (v_uni_ma[k2] > v_reg_ma[k2])
                    fprintf(f, "  Note: %s's univariate model has a regular MA(%d): this model,\n"
                               "        with %s = %d, does not contain it.\n", Ts[k2].name, v_uni_ma[k2],
                            ech_on ? "its index" : "q", v_reg_ma[k2]);
            }
            fprintf(f, "\n");
        }
    }

    /* [4] El diagonal OPTIMIZADO: la base del contraste y la semilla. */
    fit_run(&D, 1);
    if (D.ifault) {
        printf("ERROR: diagonal system failed: %s\n", fault_msg(D.ifault));
        fclose(outputv);
        return 3;
    }
    logL_diag = D.logL;

    /* [5] El modelo pedido */
    int is_diag = (p == 0 && q == 0 && want_diagcov);
    if (is_diag) {
        F = D;
    } else {
        fit_free(&D);
        cx_p = p; cx_q = q; cx_diagcov = want_diagcov;
        {   int k, b2;
            for (k = 0; k <= ESC_MAXLAG; k++)
                for (a = 0; a <= ESC_MAX; a++)
                    for (b2 = 0; b2 <= ESC_MAX; b2++) cAR[k][a][b2] = cMA[k][a][b2] = 0.0;
            for (a = 0; a <= ESC_MAX; a++)
                for (b2 = 0; b2 <= ESC_MAX; b2++) qcov[a][b2] = 0.0;
        }
        if (opt_start_ols == 2) {
            start_echelon();
            printf("Start of the cross terms: %s\n", start_used);
            fprintf(outputv, "Start of the cross terms: %s (-start echelon: Lutkepohl and "
                             "Poskitt's stage II least squares, the whole model)\n", start_used);
        } else if (opt_start_ols > 0 || (opt_start_ols < 0 && p > 0 && q == 0)) {
            if (v_on) start_ols_v(); else start_ols();
            printf("Start of the cross terms: %s\n", start_used);
            fprintf(outputv, "Start of the cross terms: %s (%s: the original "
                             "drvarma's init_varma, least squares)\n", start_used,
                    opt_start_ols > 0 ? "-start ols" : "the default with q = 0");
        }
        quiet_mode = 0;
        fit_run(&F, 1);
        quiet_mode = 1;
    }

    if (F.ifault) {
        printf("Estimation error: %s (code %d)\n", fault_msg(F.ifault), F.ifault);
        fprintf(outputv, "\n**** ESTIMATION FAILED: %s (code %d)\n", fault_msg(F.ifault), F.ifault);
        rc = 3;
    } else {
        /* The free cross coefficients (less the -zero ones) and the
           covariances: what the full model adds to the diagonal one.  */
        int ncross = n_cross_free() + (want_diagcov ? 0 : n_ser * (n_ser - 1) / 2);
        nser = n_ser;
        fprintf(outputv, "=============================================================\n");
        fprintf(outputv, "  ESTIMATED MODEL%s\n", is_diag ? " (the diagonal system)" : "");
        fprintf(outputv, "=============================================================\n");
        fprintf(outputv, "Number of parameters: %d\n", F.npar);
        print_param_table(outputv, &F);
        fprintf(outputv, "\nExact log-likelihood: %.6f\n", F.logL);
        lik_check_report(outputv);          /* -lik both: elf against Shea  */
        printf("Number of parameters: %d\nExact log-likelihood: %.6f\n", F.npar, F.logL);
        if (!is_diag) {
            real lr = 2.0 * (F.logL - logL_diag);
            real pv = 1.0 - chisq(lr > 0.0 ? lr : 0.0, ncross);
            if (opt_route_v)
                fprintf(outputv, "\nCross dynamics against the same model without cross terms (route V;\n"
                                 "not against the univariate models, which are the yardstick):\n");
            else
            fprintf(outputv, "\nCross dynamics against the diagonal system (the univariates):\n");
            fprintf(outputv, "  logL diagonal %.6f, logL full %.6f\n", logL_diag, F.logL);
            fprintf(outputv, "  LR = %.4f   df = %d   p-value = %.4f\n", lr, ncross, pv);
            printf("LR against the %s: %.4f (df %d, p = %.4f)\n",
                   opt_route_v ? "model without cross terms" : "univariates", lr, ncross, pv);
        }
        own_ma_outside(outputv, 1);
        print_sigma(outputv, &F);
        print_matrices(&F.vm);
        print_roots(&F.vm);
        {
            /* BUG-0004: F.vm lleva sigma2 = 1 (shootx_esc; la escalera la
               concentra y vive en F.sigma2), y la IRF factoriza
               Sigma = sigma2 * Q. Con 1 factorizaba Q, y el choque de una
               d.t. salia dividido por sigma. Se pone la de verdad solo
               para el informe; la FEVD es un cociente y no cambia.     */
            int  h = (F.vm.n < 40) ? 10 : 20;
            real s2 = F.vm.sigma2;
            F.vm.sigma2 = F.sigma2;
            impulse_response(&F.vm, h, outputv);
            F.vm.sigma2 = s2;
            irf_delta(&F, h, outputv);
            F.vm.sigma2 = F.sigma2;
            variance_decomposition(&F.vm, h, outputv);
            F.vm.sigma2 = s2;
        }
        multivariate_diagnostics(F.vm.a, F.vm.n, F.vm.m, data_freq, esc_ndyn(), outputv);
        {
            int *np = esc_npar_ser();
            diagnose(&F.vm, np);
            free(np);
        }
        if (irfboot_n > 0)
            irf_bootstrap(&F, (F.vm.n < 40) ? 10 : 20, irfboot_n, outname, outputv);
    }

    /* [5b] -factortest keeps the fitted matrices: it runs last, after the
       forecasts, because it rewrites the structure of the model.          */
    if (opt_factortest && F.ifault == 0 && v_on) {
        int k2, a2, b2;
        ft_P = F.vm.p;  ft_Q = F.vm.q;  ft_logL = F.logL;  ft_npar = F.npar;
        ft_phi = tensor(0, ft_P, 1, n_ser, 1, n_ser);
        ft_th  = tensor(0, ft_Q, 1, n_ser, 1, n_ser);
        for (a2 = 1; a2 <= n_ser; a2++)
            for (b2 = 1; b2 <= n_ser; b2++) {
                for (k2 = 0; k2 <= ft_P; k2++) ft_phi[k2][a2][b2] = F.vm.phi[k2][a2][b2];
                for (k2 = 0; k2 <= ft_Q; k2++) ft_th[k2][a2][b2]  = F.vm.theta[k2][a2][b2];
            }
    }

    /* [6] Forecasts. The estimated model is released first: the forecast
       rebuilds the stationary series at other lengths, and the cast fills
       buffers with the current one.                                      */
    {
        int   ok = (F.ifault == 0), np = F.npar > 0 ? F.npar : 1;
        real *xf = vector(1, np), s2 = F.sigma2;
        for (i = 1; i <= np; i++) xf[i] = F.x[i];
        fit_free(&F);
        if (ok && fc_h > 0) {
            rc = esc_forecasts(xf, s2, fc_h, estwin, outname);
            if (rc) printf("Forecasting failed (code %d)\n", rc);
        }
        if (ft_phi != NULL) {
            /* el estado del ajuste, de vuelta: la muestra entera, la
               estructura del modelo pedido y sus parametros */
            set_origin(0);
            act_m = n_ser;
            for (a = 1; a <= n_ser; a++) act[a] = a;
            cx_p = p; cx_q = q; cx_diagcov = want_diagcov; det_etapa = 1;
            if (build_stationary() == 0) {
                esc_unpack(xf);
                v_factor_test(outputv, p, q);
            }
            free_tensor(ft_phi, 0, ft_P, 1, n_ser, 1, n_ser);
            free_tensor(ft_th,  0, ft_Q, 1, n_ser, 1, n_ser);
            ft_phi = ft_th = NULL;
        }
        free_vector(xf, 1, np);
    }

    printf("Full results written to %s\n", outfile);
    fclose(outputv);
    set_origin(0);
    for (i = 1; i <= n_ser; i++) {
        free_vector(w[i], 1, n_stat);  w[i] = NULL;
        free_fue_pre(&Tm[i], &Ts[i], DataMat[i]);
    }
    free(series_names);  series_names = NULL;
    return rc;
}
