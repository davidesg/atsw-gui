/*****************************************************************************/
/*  volatility.c -- part of drvarma (multivariate VARMA modelling).
 *
 *  Copyright (C) 1995-2026 A.B. Treadway, J.A. Mauricio & D.E. Guerrero.
 *
 *  This program is free software: you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.  Distributed WITHOUT ANY WARRANTY; see the
 *  GNU General Public License (file COPYING) for details.
 *****************************************************************************/

/**
 * @file volatility.c
 * @brief Multivariate conditional volatility estimation for VARMA residuals.
 *
 * Implements two methods:
 *   - Exponential weighting with a common attention parameter φ (rational inattention).
 *   - Simple moving window (equal weights) over the last n observations.
 *
 * The functions rely on the linear algebra utilities in nlatools.c and require
 * the global structures defined in main.h.
 *
 * @date 2025
 * @author Alfredo Garcia-Hiernaux, Maria T. Gonzalez-Perez, David E. Guerrero
 */

#include "main.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

extern FILE *outputv;   /* global output file */
/* -------------------------------------------------------------------------
   Internal helper functions (static)
   ------------------------------------------------------------------------- */

/**
 * @brief Compute Mahalanobis distance for each observation.
 *
 * For each time t, calculates d_t = ε_t' Σ⁻¹ ε_t, where Σ is the
 * unconditional covariance matrix of the residuals.
 *
 * @param res   Residual matrix (nobs x m), 1‑based indexing.
 * @param nobs  Number of observations.
 * @param m     Number of series.
 * @param Sigma Unconditional covariance matrix (m x m), 1‑based.
 * @return      A vector of length nobs (1‑based) containing the distances.
 *              Caller must free with free_vector().
 */
static real *compute_mahalanobis(real **res, int nobs, int m, real **Sigma) {
    real **Sigma_inv = matrix(1, m, 1, m);
    matrix_inverse(Sigma, Sigma_inv, m);   /* from nlatools.c */

    real *d = vector(1, nobs);
    real *work = vector(1, m);

    for (int t = 1; t <= nobs; t++) {
        /* work = res[t] * Sigma_inv */
        for (int i = 1; i <= m; i++) {
            real sum = 0.0;
            for (int j = 1; j <= m; j++)
                sum += res[t][j] * Sigma_inv[j][i];
            work[i] = sum;
        }
        /* d[t] = work * res[t] */
        real sum = 0.0;
        for (int i = 1; i <= m; i++)
            sum += work[i] * res[t][i];
        d[t] = sum;
    }

    free_vector(work, 1, m);
    free_matrix(Sigma_inv, 1, m, 1, m);
    return d;
}

/* Comparison function for qsort (ascending) */
static int compare_real(const void *a, const void *b) {
    real aa = *(const real *)a;
    real bb = *(const real *)b;
    return (aa < bb) ? -1 : (aa > bb) ? 1 : 0;
}

/**
 * @brief Estimate φ as the proportion of distances exceeding the (1‑α) quantile.
 *
 * @param d      Vector of Mahalanobis distances (1‑based, length nobs).
 * @param nobs   Number of observations.
 * @param alpha  Significance level (e.g., 0.05). The threshold is the empirical
 *               100*(1‑α) percentile.
 * @param threshold [out] The computed threshold value.
 * @return       Estimated φ.
 */
static real estimate_phi(real *d, int nobs, real alpha, real *threshold) {
    /* Make a sorted copy */
    real *d_sorted = vector(1, nobs);
    for (int i = 1; i <= nobs; i++)
        d_sorted[i] = d[i];

    qsort(d_sorted + 1, nobs, sizeof(real), compare_real);

    int idx = (int)((1.0 - alpha) * nobs);
    if (idx < 1) idx = 1;
    if (idx > nobs) idx = nobs;
    *threshold = d_sorted[idx];

    int count = 0;
    for (int i = 1; i <= nobs; i++)
        if (d[i] > *threshold) count++;

    free_vector(d_sorted, 1, nobs);
    return (real)count / nobs;
}

/* -------------------------------------------------------------------------
   Public functions (called from drvarma.c)
   ------------------------------------------------------------------------- */

/**
 * @brief Compute conditional covariance matrices using exponential weighting.
 *
 * Model: H_t = sum_{k=0}^{window-1} φ(1-φ)^k ε_{t-k} ε_{t-k}'.
 * The parameter φ is estimated from the Mahalanobis distances of the residuals
 * using a given significance level α. The sum is truncated to a finite window
 * length; weights are renormalised to sum to 1.
 *
 * The output file (basename + ".volexp") contains one line per observation:
 *   t  var1 var2 ... varm  cov12 cov13 ... cov(m-1)m
 * where var_i = H_t[i][i] and cov_ij = H_t[i][j] for i<j.
 *
 * @param varma    Pointer to the estimated VARMA model (provides residuals,
 *                 σ², and normalized Q matrix).
 * @param alpha    Significance level for the Mahalanobis threshold (0 < alpha < 1).
 * @param window   Number of lagged residuals to include in the sum (>=1).
 * @param basename Base name of the input file (used to construct output name).
 */
void compute_exponential_volatility(struct Tvarma *varma, real alpha,
                                    int window, const char *basename) {
    int m = varma->m;
    int nobs = varma->n;
    real **res = varma->a;          /* residuals (nobs x m) */
    real sigma2 = varma->sigma2;
    real **Q = varma->qq;            /* normalized covariance (m x m) */

    /* 1. Build unconditional covariance matrix Σ = σ² Q */
    real **Sigma = matrix(1, m, 1, m);
    for (int i = 1; i <= m; i++)
        for (int j = 1; j <= m; j++)
            Sigma[i][j] = sigma2 * Q[i][j];

    /* 2. Compute Mahalanobis distances */
    real *d = compute_mahalanobis(res, nobs, m, Sigma);

    /* 3. Estimate φ */
    real thresh;
    real phi = estimate_phi(d, nobs, alpha, &thresh);

    /* Print info to the main output file */
    fprintf(outputv, "\nExponential volatility (rational inattention):\n");
    fprintf(outputv, "  α = %.3f, threshold = %g, φ = %.4f\n", alpha, thresh, phi);

    /* 4. Compute weights (finite window) and normalise */
    real *w = vector(0, window - 1);
    real sumw = 0.0;
    for (int k = 0; k < window; k++) {
        w[k] = phi * pow(1.0 - phi, k);
        sumw += w[k];
    }
    /* Guard: if phi = 0 (no exceedances detected) all weights are zero and
       sumw = 0, which would yield NaN after normalisation. Fall back to
       equal weights (1/window) so H_t is a simple average over the window. */
    if (!(sumw > 0.0)) {   /* also catches NaN */
        fprintf(outputv, "  (warning: phi = 0, using equal weights 1/%d)\n",
                window);
        for (int k = 0; k < window; k++)
            w[k] = 1.0 / window;
    } else {
        for (int k = 0; k < window; k++)
            w[k] /= sumw;   /* so that sum = 1 */
    }

    /* 5. Open output file */
    char outname[256];
    sprintf(outname, "%s.volexp", basename);
    FILE *fout = fopen(outname, "w");
    if (!fout) {
        fprintf(stderr, "ERROR: cannot create %s\n", outname);
        goto cleanup;
    }

    /* Header */
    fprintf(fout, "t");
    for (int i = 1; i <= m; i++) fprintf(fout, " var%d", i);
    for (int i = 1; i <= m; i++)
        for (int j = i+1; j <= m; j++)
            fprintf(fout, " cov%d%d", i, j);
    fprintf(fout, "\n");

    /* 6. Compute H_t for each t */
    for (int t = 1; t <= nobs; t++) {
        real **H = matrix(1, m, 1, m);
        for (int i = 1; i <= m; i++)
            for (int j = 1; j <= m; j++)
                H[i][j] = 0.0;

        int maxlag = (t - 1 < window - 1) ? t - 1 : window - 1;
        for (int k = 0; k <= maxlag; k++) {
            int idx = t - k;            /* observation index */
            real weight = w[k];
            for (int i = 1; i <= m; i++) {
                for (int j = 1; j <= m; j++) {
                    H[i][j] += weight * res[idx][i] * res[idx][j];
                }
            }
        }

        /* Write row */
        fprintf(fout, "%d", t);
        for (int i = 1; i <= m; i++) fprintf(fout, " %g", H[i][i]);
        for (int i = 1; i <= m; i++)
            for (int j = i+1; j <= m; j++)
                fprintf(fout, " %g", H[i][j]);
        fprintf(fout, "\n");

        free_matrix(H, 1, m, 1, m);
    }

    fclose(fout);
    printf("Exponential volatility series written to %s\n", outname);

cleanup:
    free_vector(w, 0, window - 1);
    free_vector(d, 1, nobs);
    free_matrix(Sigma, 1, m, 1, m);
}

/**
 * @brief Compute conditional covariance matrices using a simple moving window.
 *
 * For each time t >= window, H_t is the sample covariance of the residuals
 * from t-window+1 to t (equal weights). For t < window, no output is produced.
 * The output file (basename + ".volmov") has the same format as above.
 *
 * @param varma    Pointer to the estimated VARMA model.
 * @param window   Window length (number of observations) – must be >= 2.
 * @param basename Base name of the input file.
 */
void compute_moving_window_volatility(struct Tvarma *varma, int window,
                                      const char *basename) {
    int m = varma->m;
    int nobs = varma->n;
    real **res = varma->a;

    if (window > nobs) {
        fprintf(stderr, "ERROR: moving window (%d) > number of observations (%d)\n",
                window, nobs);
        return;
    }
    if (window < 2) {
        fprintf(stderr, "ERROR: moving window must be at least 2\n");
        return;
    }

    /* Open output file */
    char outname[256];
    sprintf(outname, "%s.volmov", basename);
    FILE *fout = fopen(outname, "w");
    if (!fout) {
        fprintf(stderr, "ERROR: cannot create %s\n", outname);
        return;
    }

    /* Header */
    fprintf(fout, "t");
    for (int i = 1; i <= m; i++) fprintf(fout, " var%d", i);
    for (int i = 1; i <= m; i++)
        for (int j = i+1; j <= m; j++)
            fprintf(fout, " cov%d%d", i, j);
    fprintf(fout, "\n");

    /* For each t from window to nobs */
    for (int t = window; t <= nobs; t++) {
        /* Compute means over the window */
        real *mean = vector(1, m);
        for (int i = 1; i <= m; i++) {
            real sum = 0.0;
            for (int k = 0; k < window; k++)
                sum += res[t - k][i];
            mean[i] = sum / window;
        }

        /* Compute covariance matrix */
        real **H = matrix(1, m, 1, m);
        for (int i = 1; i <= m; i++) {
            for (int j = i; j <= m; j++) {
                real sum = 0.0;
                for (int k = 0; k < window; k++) {
                    sum += (res[t - k][i] - mean[i]) * (res[t - k][j] - mean[j]);
                }
                H[i][j] = sum / (window - 1);   /* unbiased */
                if (i != j) H[j][i] = H[i][j];
            }
        }

        /* Write row */
        fprintf(fout, "%d", t);
        for (int i = 1; i <= m; i++) fprintf(fout, " %g", H[i][i]);
        for (int i = 1; i <= m; i++)
            for (int j = i+1; j <= m; j++)
                fprintf(fout, " %g", H[i][j]);
        fprintf(fout, "\n");

        free_matrix(H, 1, m, 1, m);
        free_vector(mean, 1, m);
    }

    fclose(fout);
    printf("Moving‑window volatility series written to %s\n", outname);
}
