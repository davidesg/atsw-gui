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
#include <string.h>
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
static int opt_redet, opt_fixarma;

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
static int esc_npar(void)
{
    int a, i, n = 0;
    for (a = 1; a <= act_m; a++) {
        i = act[a];
        if (!opt_fixarma) n += n_ar_free_params(&Tm[i]) + n_ma_free_params(&Tm[i]);
    }
    if (opt_redet)
        for (a = 1; a <= act_m; a++) n += n_det_free_params(&Tm[act[a]]);
    for (a = 1; a <= act_m; a++) if (Tm[act[a]].Imu) n++;
    n += (cx_p + cx_q) * act_m * (act_m - 1);
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
    if (opt_redet)
        for (a = 1; a <= act_m; a++) idx += pack_det_params(&Tm[act[a]], x, idx);
    for (a = 1; a <= act_m; a++) if (Tm[act[a]].Imu) x[idx++] = mu[act[a]];
    for (k = 1; k <= cx_p; k++)
        for (a = 1; a <= act_m; a++)
            for (b = 1; b <= act_m; b++)
                if (a != b) x[idx++] = cAR[k][act[a]][act[b]];
    for (k = 1; k <= cx_q; k++)
        for (a = 1; a <= act_m; a++)
            for (b = 1; b <= act_m; b++)
                if (a != b) x[idx++] = cMA[k][act[a]][act[b]];
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
    if (opt_redet)
        for (a = 1; a <= act_m; a++) unpack_det_params(&Tm[act[a]], x, &idx);
    for (a = 1; a <= act_m; a++) if (Tm[act[a]].Imu) mu[act[a]] = x[idx++];
    for (k = 1; k <= cx_p; k++)
        for (a = 1; a <= act_m; a++)
            for (b = 1; b <= act_m; b++)
                if (a != b) cAR[k][act[a]][act[b]] = x[idx++];
    for (k = 1; k <= cx_q; k++)
        for (a = 1; a <= act_m; a++)
            for (b = 1; b <= act_m; b++)
                if (a != b) cMA[k][act[a]][act[b]] = x[idx++];
    for (a = 2; a <= act_m; a++) lvar[act[a]] = x[idx++];
    if (!cx_diagcov)
        for (a = 2; a <= act_m; a++)
            for (b = 1; b < a; b++) qcov[act[a]][act[b]] = x[idx++];
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
    if (opt_redet)
        for (a = 1; a <= act_m; a++) {
            struct Tusmodel *M = &Tm[act[a]];
            for (d = 1; d <= M->NdetVar; d++) {
                const char *sp = (M->detspec && M->detspec[d]) ? M->detspec[d] : "det";
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
                if (a != b) {
                    i = act[a]; j = act[b];
                    snprintf(names[idx++], ESC_NAMELEN, "AR%d[%s<-%s]", k, Ts[i].name, Ts[j].name);
                }
    for (k = 1; k <= cx_q; k++)
        for (a = 1; a <= act_m; a++)
            for (b = 1; b <= act_m; b++)
                if (a != b) {
                    i = act[a]; j = act[b];
                    snprintf(names[idx++], ESC_NAMELEN, "MA%d[%s<-%s]", k, Ts[i].name, Ts[j].name);
                }
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
static void orders(int *P, int *Q)
{
    int a;
    *P = cx_p; *Q = cx_q;
    for (a = 1; a <= act_m; a++) {
        if (ar_deg[act[a]] > *P) *P = ar_deg[act[a]];
        if (ma_deg[act[a]] > *Q) *Q = ma_deg[act[a]];
    }
    if (*P < 1) *P = 1;             /* elf exige p >= 1 */
}

static void shootx_esc(real *x, struct Tvarma *armax, int *ifaultx, int firstx, int lastx)
{
    int m = act_m, P, Q, a, b, k, t;

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

    for (a = 1; a <= m; a++) {
        if (invalid_fixfreq(&Tm[act[a]])) { *ifaultx = 6; goto fin; }
        if (opt_redet && unstable_delta(&Tm[act[a]])) { *ifaultx = 6; goto fin; }
    }
    /* Con las deterministas libres, la serie estacionaria depende de los
       parametros: se rehace. n_stat no cambia (depende solo del operador). */
    if (opt_redet && build_stationary() != 0) { *ifaultx = 6; goto fin; }

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
        if (ar_deg[i] > 0) {
            real *ph = vector(1, ar_deg[i]);
            expand_ar_factors(&Tm[i], ph, ar_deg[i]);
            for (k = 1; k <= ar_deg[i]; k++) armax->phi[k][a][a] = ph[k];
            free_vector(ph, 1, ar_deg[i]);
        }
        if (ma_deg[i] > 0) {
            real *th = vector(1, ma_deg[i]);
            expand_ma_factors(&Tm[i], th, ma_deg[i]);
            for (k = 1; k <= ma_deg[i]; k++) armax->theta[k][a][a] = th[k];
            free_vector(th, 1, ma_deg[i]);
        }
    }
    /* Fuera de la diagonal: la dinamica cruzada. */
    for (k = 1; k <= cx_p; k++)
        for (a = 1; a <= m; a++)
            for (b = 1; b <= m; b++)
                if (a != b) armax->phi[k][a][b] = cAR[k][act[a]][act[b]];
    for (k = 1; k <= cx_q; k++)
        for (a = 1; a <= m; a++)
            for (b = 1; b <= m; b++)
                if (a != b) armax->theta[k][a][b] = cMA[k][act[a]][act[b]];

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
    elf(v->m, v->n, v->p, v->q, v->mu, v->phi, v->theta, v->qq, v->w, 1.0,
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

static void fit_free(Fit *F)
{
    int ifault = 0, np = F->npar > 0 ? F->npar : 1;
    if (F->vm.mu) shootx_esc(F->x, &F->vm, &ifault, 0, 1);
    free_matrix(F->cov, 1, np, 1, np);
    free_vector(F->dev, 1, np);
    free_vector(F->x, 1, np);
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
} Fc;

static void fc_alloc(Fc *c, int nb, int L)
{
    c->nb = nb;
    c->ystar = vector(1, nb + L);
    c->lvl = vector(1, L); c->lo = vector(1, L); c->hi = vector(1, L);
    c->sd = vector(1, L);  c->sdp = vector(1, L); c->sda = vector(1, L);
}

static void fc_free(Fc *c, int L)
{
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
        fprintf(ff, "Level/Low95/High95 in original units. mon%%/ann%% = period/annual "
                    "variation and std (100*delta/refactor; %% for lambda=0).\n\n");
    }
    for (a = 1; a <= act_m; a++) {
        int i = act[a], nb = fc[a].nb;
        real sc = 100.0 / Ts[i].refactor;
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
    printf("                                  [-hessian fd|bfgs]\n");
    printf("  THE LADDER: each series comes with its univariate model from fue (.pre):\n");
    printf("       Box-Cox, deterministic terms, differencing, mean and ARMA factors.\n");
    printf("       The VARMA keeps each model on its DIAGONAL; p and q are the orders of\n");
    printf("       the CROSS dynamics. p = q = 0 with -diagcov reproduces the univariates.\n");
    printf("  -diagcov : diagonal innovation covariance (default: full)\n");
    printf("  -redet   : re-estimate the deterministic terms (default: fixed at the .pre)\n");
    printf("  -fixarma : keep the univariate ARMA factors fixed at the .pre\n");
    printf("  -hessian : standard errors from fdhess at the optimum (fd, default) or\n"
           "             from the BFGS Hessian of the search (bfgs)\n");
    printf("  -o NAME  : results to NAME.out (default: the .pre names joined by '_')\n");
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
    cx_diagcov = 0; opt_redet = 0; opt_fixarma = 0;
    est_fdhess = 1;          /* standard errors from the Hessian AT the optimum */
    for (argi += 2; argi < argc; argi++) {
        if      (strcmp(argv[argi], "-diagcov") == 0) cx_diagcov = 1;
        else if (strcmp(argv[argi], "-redet") == 0)   opt_redet = 1;
        else if (strcmp(argv[argi], "-fixarma") == 0) opt_fixarma = 1;
        else if (strcmp(argv[argi], "-m") == 0 && argi + 1 < argc) met_esc = atoi(argv[++argi]);
        else if (strcmp(argv[argi], "-o") == 0 && argi + 1 < argc)
            snprintf(outname, sizeof outname, "%s", argv[++argi]);
        else if (strcmp(argv[argi], "-forecast") == 0 && argi + 1 < argc)
            fc_h = atoi(argv[++argi]);
        else if (strcmp(argv[argi], "-estwin") == 0 && argi + 1 < argc)
            estwin = atoi(argv[++argi]);
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
    for (i = 1; i <= n_ser; i++) nobs_full[i] = Ts[i].nobs;

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
            fprintf(f, "Model            : VARMA with univariate diagonals; cross orders p=%d q=%d\n", p, q);
            fprintf(f, "Innovation cov.  : %s\n", want_diagcov ? "diagonal" : "full");
            fprintf(f, "Deterministics   : %s\n", opt_redet ? "re-estimated" : "fixed at the .pre");
            fprintf(f, "Univariate ARMA  : %s\n", opt_fixarma ? "fixed at the .pre" : "re-estimated jointly");
            fprintf(f, "Estimation method: %d\n", met_esc);
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
    quiet_mode = 1;
    cx_p = cx_q = 0;  cx_diagcov = 1;
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

    act_m = n_ser;
    for (a = 1; a <= n_ser; a++) act[a] = a;
    for (a = 1; a <= n_ser; a++) lvar[a] = log(s2_uni[a] / s2_uni[1]);
    fit_run(&D, 0);
    logL_gate = D.logL;
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
                    fprintf(f, "  Note: %s moved %.3g from its .pre, but the common window\n"
                               "        drops its first %d stationary observations: it is\n"
                               "        re-estimated on a different sample, and moving is legitimate.\n",
                            Ts[k].name, move[k], n_trim[k]);
                else if (move[k] > PRE_MOVE)
                {
                    fprintf(f, "  Note: %s moved %.3g from its .pre on its own sample. A genuine\n"
                               "        .pre is a fixed point (it moves ~1e-5, the rounding of its six\n"
                               "        decimals): this one is a specification, not an optimum.\n",
                            Ts[k].name, move[k]);
                    if (!opt_redet && n_det_free_params(&Tm[k]) > 0)
                        fprintf(f, "        Its deterministic terms stayed at their SEEDS: in a\n"
                                   "        specification they are starting values -- use -redet.\n");
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
        quiet_mode = 0;
        fit_run(&F, 1);
        quiet_mode = 1;
    }

    if (F.ifault) {
        printf("Estimation error: %s (code %d)\n", fault_msg(F.ifault), F.ifault);
        fprintf(outputv, "\n**** ESTIMATION FAILED: %s (code %d)\n", fault_msg(F.ifault), F.ifault);
        rc = 3;
    } else {
        int ncross = (p + q) * n_ser * (n_ser - 1) + (want_diagcov ? 0 : n_ser * (n_ser - 1) / 2);
        nser = n_ser;
        fprintf(outputv, "=============================================================\n");
        fprintf(outputv, "  ESTIMATED MODEL%s\n", is_diag ? " (the diagonal system)" : "");
        fprintf(outputv, "=============================================================\n");
        fprintf(outputv, "Number of parameters: %d\n", F.npar);
        print_param_table(outputv, &F);
        fprintf(outputv, "\nExact log-likelihood: %.6f\n", F.logL);
        printf("Number of parameters: %d\nExact log-likelihood: %.6f\n", F.npar, F.logL);
        if (!is_diag) {
            real lr = 2.0 * (F.logL - logL_diag);
            real pv = 1.0 - chisq(lr > 0.0 ? lr : 0.0, ncross);
            fprintf(outputv, "\nCross dynamics against the diagonal system (the univariates):\n");
            fprintf(outputv, "  logL diagonal %.6f, logL full %.6f\n", logL_diag, F.logL);
            fprintf(outputv, "  LR = %.4f   df = %d   p-value = %.4f\n", lr, ncross, pv);
            printf("LR against the univariates: %.4f (df %d, p = %.4f)\n", lr, ncross, pv);
        }
        print_sigma(outputv, &F);
        print_matrices(&F.vm);
        print_roots(&F.vm);
        {
            int h = (F.vm.n < 40) ? 10 : 20;
            impulse_response(&F.vm, h, outputv);
            variance_decomposition(&F.vm, h, outputv);
        }
        multivariate_diagnostics(F.vm.a, F.vm.n, F.vm.m, outputv);
        diagnose(&F.vm);
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
