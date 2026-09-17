/*****************************************************************************/
/*  forecast.c -- part of drvarma (multivariate VARMA modelling).
 *
 *  Copyright (C) 1995-2026 A.B. Treadway, J.A. Mauricio & D.E. Guerrero.
 *
 *  This program is free software: you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.  Distributed WITHOUT ANY WARRANTY; see the
 *  GNU General Public License (file COPYING) for details.
 *****************************************************************************/

#include "forecast.h"
#include <math.h>

static void compute_psi_weights(int m, int p, int q, real ***phi, real ***theta,
                                int L, real ***psi)
{
    int l, i, j, k, i1, j1;
    for (l = 0; l <= L; l++)
        for (i = 1; i <= m; i++)
            for (j = 1; j <= m; j++)
                psi[l][i][j] = 0.0;
    for (i = 1; i <= m; i++)
        psi[0][i][i] = 1.0;

    for (l = 1; l <= L; l++) {
        for (i = 1; i <= l; i++) {
            if (i <= p) {
                for (i1 = 1; i1 <= m; i1++) {
                    for (j1 = 1; j1 <= m; j1++) {
                        real s = 0.0;
                        for (k = 1; k <= m; k++)
                            s += phi[i][i1][k] * psi[l-i][k][j1];
                        psi[l][i1][j1] += s;
                    }
                }
            }
        }
        if (l <= q) {
            for (i1 = 1; i1 <= m; i1++)
                for (j1 = 1; j1 <= m; j1++)
                    psi[l][i1][j1] -= theta[l][i1][j1];
        }
    }
}

static void forecast_variance(int m, int L, real ***psi, real **sigma,
                              real ***var)
{
    int l, j, i1, j1, k;
    real **mtmp1 = matrix(1, m, 1, m);
    real **mtmp2 = matrix(1, m, 1, m);

    for (l = 1; l <= L; l++) {
        for (i1 = 1; i1 <= m; i1++)
            for (j1 = 1; j1 <= m; j1++)
                var[l][i1][j1] = 0.0;
    }

    for (l = 1; l <= L; l++) {
        for (j = 0; j <= l-1; j++) {
            // mtmp1 = psi[j] * sigma
            for (i1 = 1; i1 <= m; i1++) {
                for (j1 = 1; j1 <= m; j1++) {
                    real s = 0.0;
                    for (k = 1; k <= m; k++)
                        s += psi[j][i1][k] * sigma[k][j1];
                    mtmp1[i1][j1] = s;
                }
            }
            // mtmp2 = mtmp1 * psi[j]'
            for (i1 = 1; i1 <= m; i1++) {
                for (j1 = 1; j1 <= m; j1++) {
                    real s = 0.0;
                    for (k = 1; k <= m; k++)
                        s += mtmp1[i1][k] * psi[j][j1][k];
                    mtmp2[i1][j1] = s;
                }
            }
            for (i1 = 1; i1 <= m; i1++)
                for (j1 = 1; j1 <= m; j1++)
                    var[l][i1][j1] += mtmp2[i1][j1];
        }
    }
    free_matrix(mtmp2, 1, m, 1, m);
    free_matrix(mtmp1, 1, m, 1, m);
}

static void forecast_mean(int m, int n, int p, int q, real *mu,
                          real ***phi, real ***theta,
                          real **w, real **a,
                          int b, int L, real **f1)
{
    int l, i, i1, j, k;
    real *vtmp1 = vector(1, m);
    real *vtmp2 = vector(1, m);
    real s1;

    for (l = 1; l <= L; l++) {
        for (i = 1; i <= m; i++) vtmp1[i] = 0.0;
        // parte AR
        for (i = 1; i <= p; i++) {
            if (l > i) {
                for (i1 = 1; i1 <= m; i1++) {
                    s1 = 0.0;
                    for (k = 1; k <= m; k++)
                        s1 += phi[i][i1][k] * (f1[k][l-i] - mu[k]);
                    vtmp1[i1] += s1;
                }
            } else {
                for (i1 = 1; i1 <= m; i1++) {
                    s1 = 0.0;
                    for (k = 1; k <= m; k++)
                        s1 += phi[i][i1][k] * (w[n-b-i+l][k] - mu[k]);
                    vtmp1[i1] += s1;
                }
            }
        }
        // parte MA
        for (j = 1; j <= m; j++) vtmp2[j] = 0.0;
        for (j = 1; j <= q; j++) {
            if (l <= j) {
                for (i1 = 1; i1 <= m; i1++) {
                    s1 = 0.0;
                    for (k = 1; k <= m; k++)
                        s1 += theta[j][i1][k] * a[n-b-j+l][k];
                    vtmp2[i1] += s1;
                }
            }
        }
        for (i1 = 1; i1 <= m; i1++)
            f1[i1][l] = mu[i1] + vtmp1[i1] - vtmp2[i1];
    }
    free_vector(vtmp2, 1, m);
    free_vector(vtmp1, 1, m);
}

void forecast_level_variances(int m, int p, int q, real ***phi, real ***theta,
                              real **sigma, int L, int d, int D, int s,
                              real ***v_level, real ***v_mon, real ***v_ann)
{
    real ***psi  = tensor(0, L, 1, m, 1, m);   /* model MA(inf) weights      */
    real ***psis = tensor(0, L, 1, m, 1, m);   /* integrated (level) weights */
    real ***op   = tensor(0, L, 1, m, 1, m);   /* weights of an operator * y */

    compute_psi_weights(m, p, q, phi, theta, L, psi);

    /* Coefficients of the differencing operator
       delta(B) = (1-B)^d (1-B^s)^D = sum_k delta[k] B^k (delta[0] = 1). */
    int deg = d + D * s;
    real *delta = vector(0, deg);
    for (int k = 0; k <= deg; k++) delta[k] = 0.0;
    delta[0] = 1.0;
    for (int r = 0; r < d; r++)                 /* multiply by (1-B)   */
        for (int k = deg; k >= 1; k--) delta[k] -= delta[k-1];
    for (int r = 0; r < D; r++)                 /* multiply by (1-B^s) */
        for (int k = deg; k >= s; k--) delta[k] -= delta[k-s];

    /* psi*(B) = psi(B) / delta(B):  psi*_l = psi_l - sum_{k>=1} delta_k psi*_{l-k}. */
    for (int l = 0; l <= L; l++)
        for (int i = 1; i <= m; i++)
            for (int j = 1; j <= m; j++) {
                real val = psi[l][i][j];
                int kmax = (l < deg) ? l : deg;
                for (int k = 1; k <= kmax; k++)
                    val -= delta[k] * psis[l-k][i][j];
                psis[l][i][j] = val;
            }

    /* [v1] level variance: weights psi*. */
    forecast_variance(m, L, psis, sigma, v_level);

    /* [v2] monthly variation (1-B)y: weights psi*_l - psi*_{l-1}. */
    for (int l = 0; l <= L; l++)
        for (int i = 1; i <= m; i++)
            for (int j = 1; j <= m; j++)
                op[l][i][j] = psis[l][i][j] - (l >= 1 ? psis[l-1][i][j] : 0.0);
    forecast_variance(m, L, op, sigma, v_mon);

    /* [v3] annual variation (1-B^s)y: weights psi*_l - psi*_{l-s}. */
    for (int l = 0; l <= L; l++)
        for (int i = 1; i <= m; i++)
            for (int j = 1; j <= m; j++)
                op[l][i][j] = psis[l][i][j] - (l >= s ? psis[l-s][i][j] : 0.0);
    forecast_variance(m, L, op, sigma, v_ann);

    free_vector(delta, 0, deg);
    free_tensor(op,   0, L, 1, m, 1, m);
    free_tensor(psis, 0, L, 1, m, 1, m);
    free_tensor(psi,  0, L, 1, m, 1, m);
}

void forecast_model(int m, int n, int p, int q, real *mu,
                    real ***phi, real ***theta, real **sigma,
                    real **w, real **a,
                    real **f1, real ***v1, real ***v2, real ***v3,
                    int b, int L, int s, real **datamat)
{
    real ***psi1, ***psi2, ***psi3;

    // Asignar memoria para los pesos psi (hasta L)
    psi1 = tensor(0, L, 1, m, 1, m);
    psi2 = tensor(0, L, 1, m, 1, m);
    psi3 = tensor(0, L, 1, m, 1, m);

    // 1. Previsiones puntuales
    forecast_mean(m, n, p, q, mu, phi, theta, w, a, b, L, f1);

    // 2. Pesos psi para el nivel
    compute_psi_weights(m, p, q, phi, theta, L, psi1);

    // 3. Varianzas para el nivel
    forecast_variance(m, L, psi1, sigma, v1);

    // 4. Pesos y varianzas para diferencias simples
    for (int l = 0; l <= L; l++)
        for (int i = 1; i <= m; i++)
            for (int j = 1; j <= m; j++)
                psi2[l][i][j] = (l == 0) ? psi1[0][i][j] : psi1[l][i][j] - psi1[l-1][i][j];
    forecast_variance(m, L, psi2, sigma, v2);

    // 5. Pesos y varianzas para diferencias estacionales (período s)
    if (s > 1) {
        for (int l = 0; l <= L; l++) {
            if (l < s)
                for (int i = 1; i <= m; i++)
                    for (int j = 1; j <= m; j++)
                        psi3[l][i][j] = psi1[l][i][j];
            else
                for (int i = 1; i <= m; i++)
                    for (int j = 1; j <= m; j++)
                        psi3[l][i][j] = psi1[l][i][j] - psi1[l-s][i][j];
        }
        forecast_variance(m, L, psi3, sigma, v3);
    } else {
        // Si no hay estacionalidad, v3 se deja a cero (no usado)
        for (int l = 1; l <= L; l++)
            for (int i = 1; i <= m; i++)
                for (int j = 1; j <= m; j++)
                    v3[l][i][j] = 0.0;
    }

    // Liberar memoria
    free_tensor(psi3, 0, L, 1, m, 1, m);
    free_tensor(psi2, 0, L, 1, m, 1, m);
    free_tensor(psi1, 0, L, 1, m, 1, m);
}
