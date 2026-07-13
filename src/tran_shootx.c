/*****************************************************************************/
/*  tran_shootx.c -- part of drtran (Box-Jenkins transfer function models).
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

#include "main.h"
#include "drtran.h"
#include "fue_pre_reader.h"

/* -------------------------------------------------------------------------- */
/* compute_irf: pesos nu de un filtro racional omega(B)/delta(B) con retardo b */
/* nu[t] = omega[t-1-b] + sum_{j=1..r} delta[j]*nu[t-j]                        */
/* nu[j] pesa a la entrada en el retardo j-1.                                  */
/* -------------------------------------------------------------------------- */
void compute_irf(real *omega, int s, real *delta, int r, int b,
                 real *nu, int length)
{
    int t, j;

    for (t = 1; t <= length; t++) nu[t] = 0.0;

    for (t = 1; t <= length; t++) {
        real sum = 0.0;
        int  lag = t - 1 - b;
        if (lag >= 0 && lag <= s) sum = omega[lag];
        for (j = 1; j <= r; j++)
            if (t > j) sum += delta[j] * nu[t - j];
        nu[t] = sum;
    }
}

/* -------------------------------------------------------------------------- */
/* Estabilidad del denominador δ(B) de cada variable determinista.            */
/* El filtro 1/δ(B) es recursivo: con raíces dentro del círculo unidad la      */
/* contribución determinista explota. Se comprueba con chekma, que usa la      */
/* misma convención de polinomio (1 - δ₁B - δ₂B² - …).                        */
/* -------------------------------------------------------------------------- */
static int unstable_delta(struct Tusmodel *Tmi)
{
    int i, k;
    real wr[10], wi[10], wmod[10];

    for (i = 1; i <= Tmi->NdetVar; i++) {
        int nd = Tmi->Ndelta[i];
        int ifault_chk = 0;
        real ***t1;

        if (nd <= 0) continue;

        t1 = tensor(0, nd, 1, 1, 1, 1);
        t1[0][1][1] = 1.0;
        for (k = 1; k <= nd; k++) t1[k][1][1] = Tmi->Delta[i][k];

        chekma(1, nd, t1, wr, wi, wmod, &ifault_chk);
        free_tensor(t1, 0, nd, 1, 1, 1, 1);

        if (ifault_chk != 0) return 1;
    }
    return 0;
}

/* -------------------------------------------------------------------------- */
/* shootx — el CAST: vector de parámetros -> estructura VARMA                  */
/*                                                                            */
/* Modelo:   Y_t = SUM_j nu_j(B) X_j,t + N_t                                   */
/*                                                                            */
/* VARMA diagonal de m = 1 + n_inp series:                                     */
/*     w[.][1]   = w_1 - SUM_j transferencia_j     (el ruido N)                */
/*     w[.][i]   = w_i                             (la entrada i-1)            */
/*     phi/theta diagonales: cada serie con su propio ARMA                     */
/*     Q diagonal, con Q[1][1] = 1 (la escala la concentra sigma2)             */
/*                                                                            */
/* Todo el acoplamiento vive en las transferencias restadas a la serie 1.      */
/* -------------------------------------------------------------------------- */
void shootx(real *xfree, struct Tvarma *armax, int *ifaultx, int firstx, int lastx)
{
    /* El optimizador solo ve los parámetros LIBRES. Aquí se expanden a la
       estructura completa, aplicando los fijos y los COMPARTIDOS: un mismo
       grado de libertad puede aparecer en varios sitios de la estructura.  */
    real *x = expand_params(xfree);

    int m = n_ser;
    int idx = 1;
    int i, j, k, t;
    int p, q;

    real omega[MAX_INP + 1][MAX_S + 1];
    real delta[MAX_INP + 1][MAX_R + 1];
    real **nu = NULL;          /* nu[j][.] : pesos de la entrada j */
    real  *transfer = NULL;    /* transferencia total en cada t     */
    real   var[MAX_SER + 1];

    *ifaultx = 0;

    /* --- 1. Parámetros de las transferencias (una por entrada) --- */
    for (j = 1; j <= n_inp; j++) {
        for (k = 0; k <= s_ord[j]; k++) omega[j][k] = x[idx++];
        for (k = 1; k <= r_ord[j]; k++) delta[j][k] = x[idx++];
    }

    /* --- 2. ARMA de cada serie (factores del .pre, no expandidos) --- */
    for (i = 1; i <= n_ser; i++) {
        if (fix_arma[i]) continue;
        unpack_ar_factors(&Tm[i], x, &idx);
        unpack_ma_factors(&Tm[i], x, &idx);
    }

    /* --- 3. Deterministas: solo los coeficientes libres según el .pre --- */
    for (i = 1; i <= n_ser; i++)
        if (!fix_det[i]) unpack_det_params(&Tm[i], x, &idx);

    /* Rechazar deterministas con denominador inestable o factores de
       frecuencia fija inválidos (c2 >= 0) */
    for (i = 1; i <= n_ser; i++) {
        if (unstable_delta(&Tm[i]) || invalid_fixfreq(&Tm[i])) {
            *ifaultx = 1;
            return;
        }
    }

    /* --- 4. Medias (cada serie según su flag) --- */
    for (i = 1; i <= n_ser; i++)
        if (!fix_mu[i]) mu[i] = x[idx++];

    /* --- 5. Covarianza Q, diagonal.
             est() CONCENTRA un factor de escala sigma2 (Sigma = sigma2*Q), así
             que la escala global de Q NO está identificada: meter todas las
             varianzas libres deja una dirección exactamente plana y un hessiano
             SINGULAR. Solo las RAZONES son estimables: se normaliza Q[1][1] = 1
             y se estima log(var_i/var_1) para i = 2..m.                       */
    var[1] = 1.0;
    for (i = 2; i <= n_ser; i++) var[i] = exp(x[idx++]);

    /* --- 6. Expandir los factores ARMA a polinomios --- */
    for (i = 1; i <= n_ser; i++) {
        if (p_ord[i] > 0) expand_ar_factors(&Tm[i], phi[i],   p_ord[i]);
        if (q_ord[i] > 0) expand_ma_factors(&Tm[i], theta[i], q_ord[i]);
    }

    /* --- 7. Series estacionarias con los parámetros actuales --- */
    build_stationary_series();
    if (n_stat <= 0) { *ifaultx = 1; return; }

    /* --- 8. Transferencia total: suma de las de cada entrada --- */
    transfer = vector(1, n_stat);
    for (t = 1; t <= n_stat; t++) transfer[t] = 0.0;

    if (n_inp > 0) {
        nu = matrix(1, n_inp, 1, n_stat);

        for (j = 1; j <= n_inp; j++) {
            compute_irf(omega[j], s_ord[j], delta[j], r_ord[j], b_del[j],
                        nu[j], n_stat);

            /* transferencia_j[t] = sum_k nu_j[k] * w_{j+1}[t-k+1] */
            for (t = 1; t <= n_stat; t++) {
                real tr = 0.0;
                for (k = 1; k <= t; k++)
                    tr += nu[j][k] * w[j + 1][t - k + 1];
                transfer[t] += tr;
            }
        }
    }

    /* --- 9. Dimensiones del VARMA --- */
    p = 1;                       /* elf exige p >= 1 */
    q = 0;
    for (i = 1; i <= n_ser; i++) {
        if (p_ord[i] > p) p = p_ord[i];
        if (q_ord[i] > q) q = q_ord[i];
    }

    armax->m = m;
    armax->n = n_stat;
    armax->p = p;
    armax->q = q;

    /* --- 10. Alojar / reinicializar --- */
    if (firstx) {
        armax->mu    = vector(1, m);
        armax->phi   = tensor(0, p, 1, m, 1, m);
        armax->theta = tensor(0, q, 1, m, 1, m);
        armax->qq    = matrix(1, m, 1, m);
        armax->w     = matrix(1, n_stat, 1, m);
        armax->a     = matrix(1, n_stat, 1, m);

        for (i = 1; i <= m; i++) {
            armax->mu[i] = 0.0;
            for (j = 1; j <= m; j++) {
                for (k = 0; k <= p; k++) armax->phi[k][i][j]   = 0.0;
                for (k = 0; k <= q; k++) armax->theta[k][i][j] = 0.0;
                armax->qq[i][j] = 0.0;
            }
            for (t = 1; t <= n_stat; t++) {
                armax->w[t][i] = 0.0;
                armax->a[t][i] = 0.0;
            }
        }
        for (i = 1; i <= m; i++) {          /* phi[0] = theta[0] = I */
            armax->phi[0][i][i]   = 1.0;
            armax->theta[0][i][i] = 1.0;
        }
    } else {
        for (k = 1; k <= p; k++)
            for (i = 1; i <= m; i++)
                for (j = 1; j <= m; j++) armax->phi[k][i][j] = 0.0;
        for (k = 1; k <= q; k++)
            for (i = 1; i <= m; i++)
                for (j = 1; j <= m; j++) armax->theta[k][i][j] = 0.0;
    }

    /* --- 11. phi y theta DIAGONALES: cada serie con su propio ARMA --- */
    for (i = 1; i <= m; i++) {
        for (k = 1; k <= p_ord[i]; k++) armax->phi[k][i][i]   = phi[i][k];
        for (k = 1; k <= q_ord[i]; k++) armax->theta[k][i][i] = theta[i][k];
    }

    /* --- 12. Restricciones --- */
    {
        real wr[4 * MAX_SER], wi[4 * MAX_SER], wmod[4 * MAX_SER];
        int ifault_chk = 0;

        for (i = 1; i <= m; i++)
            if (p_ord[i] >= 1 && fabs(phi[i][1]) >= 0.999) {
                *ifaultx = 1;
                goto cleanup;
            }

        if (q > 0) {
            chekma(m, q, armax->theta, wr, wi, wmod, &ifault_chk);
            if (ifault_chk != 0) { *ifaultx = 1; goto cleanup; }
        }
    }

    /* --- 13. Q diagonal y medias --- */
    for (i = 1; i <= m; i++)
        for (j = 1; j <= m; j++)
            armax->qq[i][j] = (i == j) ? var[i] : 0.0;
    armax->sigma2 = 1.0;        /* la escala la concentra est() */

    for (i = 1; i <= m; i++) armax->mu[i] = mu[i];

    /* --- 14. Las series: la 1 es el ruido; las demás, las entradas --- */
    for (t = 1; t <= n_stat; t++) {
        armax->w[t][1] = w[1][t] - transfer[t];
        for (i = 2; i <= m; i++) armax->w[t][i] = w[i][t];
    }

cleanup:
    if (nu) free_matrix(nu, 1, n_inp, 1, n_stat);
    free_vector(transfer, 1, n_stat);

    if (lastx) {
        free_matrix(armax->a, 1, armax->n, 1, armax->m);
        free_matrix(armax->w, 1, armax->n, 1, armax->m);
        free_matrix(armax->qq, 1, armax->m, 1, armax->m);
        free_tensor(armax->theta, 0, armax->q, 1, armax->m, 1, armax->m);
        free_tensor(armax->phi, 0, armax->p, 1, armax->m, 1, armax->m);
        free_vector(armax->mu, 1, armax->m);
    }
}
