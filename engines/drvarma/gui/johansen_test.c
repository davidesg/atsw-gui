/*****************************************************************************/
/*  johansen_test.c -- part of drvarma (multivariate VARMA modelling).
 *
 *  Copyright (C) 1995-2026 A.B. Treadway, J.A. Mauricio & D.E. Guerrero.
 *
 *  This program is free software: you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.  Distributed WITHOUT ANY WARRANTY; see the
 *  GNU General Public License (file COPYING) for details.
 *****************************************************************************/

#include "johansen_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_eigen.h>
#include <gsl/gsl_complex.h>
#include <gsl/gsl_cdf.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_blas.h>

/* ---------- Critical values for up to 5 variables ----------
   Format: [n-1][det_trend][0=trace,1=max][10%,5%,1%]
   where det_trend = 0 for constant (restricted), 1 for constant+trend (restricted)
   Index: n = number of common trends (= n_var - r under H0)
*/
static const double johansen_cv[5][2][2][3] = {
    /* n=1 */ { /* const */ { {2.71,3.84,6.64},{2.71,3.84,6.64} },
                /* trend */  { {5.42,6.79,10.04},{5.42,6.79,10.04} } },
    /* n=2 */ { /* const */ { {13.31,15.41,20.04},{12.07,14.07,18.63} },
                /* trend */  { {18.90,21.07,26.15},{16.18,18.11,23.46} } },
    /* n=3 */ { /* const */ { {27.07,29.80,35.46},{18.90,20.97,25.78} },
                /* trend */  { {34.40,37.22,44.31},{22.89,24.99,30.57} } },
    /* n=4 */ { /* const */ { {44.49,47.86,54.68},{25.12,27.34,32.62} },
                /* trend */  { {53.35,56.74,65.17},{28.86,31.24,37.22} } },
    /* n=5 */ { /* const */ { {65.82,69.82,78.12},{30.58,33.26,38.86} },
                /* trend */  { {76.07,80.11,90.07},{34.55,37.15,43.45} } }
};

/* ---------- Helper: matrix inversion with Tikhonov regularisation ---------- */
static int matrix_invert(gsl_matrix *a, gsl_matrix *inv) {
    int n = a->size1;
    gsl_matrix *copy = gsl_matrix_alloc(n, n);
    gsl_matrix_memcpy(copy, a);
    gsl_permutation *p = gsl_permutation_alloc(n);
    int signum;
    gsl_error_handler_t *old = gsl_set_error_handler_off();
    int status = gsl_linalg_LU_decomp(copy, p, &signum);
    if (status) {
        /* regularise */
        double diag = 0.0;
        for (int i=0;i<n;i++) diag += gsl_matrix_get(a,i,i);
        double lambda = 1e-3 * (diag / n);
        if (lambda < 1e-12) lambda = 1e-3;
        gsl_matrix_memcpy(copy, a);
        for (int i=0;i<n;i++) {
            double val = gsl_matrix_get(copy,i,i);
            gsl_matrix_set(copy,i,i,val + lambda);
        }
        gsl_permutation_free(p);
        p = gsl_permutation_alloc(n);
        status = gsl_linalg_LU_decomp(copy, p, &signum);
    }
    if (status) {
        gsl_set_error_handler(old);
        gsl_matrix_free(copy);
        gsl_permutation_free(p);
        return status;
    }
    int inv_status = gsl_linalg_LU_invert(copy, p, inv);
    gsl_set_error_handler(old);
    gsl_matrix_free(copy);
    gsl_permutation_free(p);
    return inv_status;
}

/* ---------- Main Johansen routine ---------- */
JohansenResult * johansen_test_run(const double * const * data,
                                   int n_obs, int n_var, int p,
                                   JohansenDetType det, int use_logs) {
    JohansenResult *res = calloc(1, sizeof(JohansenResult));
    if (!res) return NULL;
    res->n_var = n_var;
    res->p = p;
    res->det = det;
    res->use_logs = use_logs;
    res->error_code = 0;

    /* check minimum observations */
    if (n_obs < p+2) { res->error_code = -1; goto err; }
    if (n_var < 1 || n_var > 5) { res->error_code = -1; goto err; }  /* critical values only up to 5 */

    /* build data matrix (maybe with logs) */
    double **y = malloc(n_obs * sizeof(double*));
    for (int i=0;i<n_obs;i++) {
        y[i] = malloc(n_var * sizeof(double));
        for (int j=0;j<n_var;j++) {
            double val = data[i][j];
            if (use_logs) {
                if (val <= 0) { res->error_code = -2; goto free_y; }
                val = log(val);
            }
            y[i][j] = val;
        }
    }

    int T = n_obs;
    int nobs = T - p;
    int d = (det == JOH_DET_CONST) ? 1 :
            (det == JOH_DET_CONST_TREND || det == JOH_DET_CONST_DUMMY) ? 2 : 0;
    int nz = n_var * p + d;

    /* allocate workspace */
    gsl_matrix *Z0 = gsl_matrix_alloc(n_var, nobs);
    gsl_matrix *Z1 = gsl_matrix_alloc(n_var, nobs);
    gsl_matrix *Z2 = gsl_matrix_alloc(nz, nobs);

    /* fill Z0, Z1, Z2 */
    for (int t=p; t<T; t++) {
        int col = t-p;
        for (int i=0;i<n_var;i++) {
            gsl_matrix_set(Z0, i, col, y[t][i] - y[t-1][i]);
            gsl_matrix_set(Z1, i, col, y[t-1][i]);
        }
        int row = 0;
        for (int lag=1; lag<=p; lag++) {
            for (int i=0;i<n_var;i++) {
                double diff = (t-lag-1 >= 0) ? (y[t-lag][i] - y[t-lag-1][i]) : 0.0;
                gsl_matrix_set(Z2, row++, col, diff);
            }
        }
        if (det == JOH_DET_CONST) {
            gsl_matrix_set(Z2, row++, col, 1.0);
        } else if (det == JOH_DET_CONST_TREND) {
            gsl_matrix_set(Z2, row++, col, 1.0);
            gsl_matrix_set(Z2, row++, col, (double)(t+1));
        } else if (det == JOH_DET_CONST_DUMMY) {
            int break_obs = T/2;
            double step = (t >= break_obs) ? 1.0 : 0.0;
            gsl_matrix_set(Z2, row++, col, 1.0);
            gsl_matrix_set(Z2, row++, col, step);
        }
    }

    /* free raw data copies */
    for (int i=0;i<T;i++) free(y[i]);
    free(y); y=NULL;

    /* moment matrices */
    double scale = 1.0/nobs;
    gsl_matrix *R00 = gsl_matrix_alloc(n_var, n_var);
    gsl_matrix *R01 = gsl_matrix_alloc(n_var, n_var);
    gsl_matrix *R11 = gsl_matrix_alloc(n_var, n_var);
    gsl_matrix *R0Z = gsl_matrix_alloc(n_var, nz);
    gsl_matrix *R1Z = gsl_matrix_alloc(n_var, nz);
    gsl_matrix *RZZ = gsl_matrix_alloc(nz, nz);
    gsl_matrix_set_zero(R00); gsl_matrix_set_zero(R01); gsl_matrix_set_zero(R11);
    gsl_matrix_set_zero(R0Z); gsl_matrix_set_zero(R1Z); gsl_matrix_set_zero(RZZ);

    for (int col=0; col<nobs; col++) {
        gsl_vector_const_view z0 = gsl_matrix_const_column(Z0, col);
        gsl_vector_const_view z1 = gsl_matrix_const_column(Z1, col);
        gsl_vector_const_view z2 = gsl_matrix_const_column(Z2, col);
        gsl_blas_dger(scale, &z0.vector, &z0.vector, R00);
        gsl_blas_dger(scale, &z0.vector, &z1.vector, R01);
        gsl_blas_dger(scale, &z1.vector, &z1.vector, R11);
        gsl_blas_dger(scale, &z0.vector, &z2.vector, R0Z);
        gsl_blas_dger(scale, &z1.vector, &z2.vector, R1Z);
        gsl_blas_dger(scale, &z2.vector, &z2.vector, RZZ);
    }

    gsl_matrix_free(Z0); gsl_matrix_free(Z1); gsl_matrix_free(Z2);

    /* RZZ^{-1} */
    gsl_matrix *invRZZ = gsl_matrix_alloc(nz, nz);
    if (matrix_invert(RZZ, invRZZ) != GSL_SUCCESS) {
        res->error_code = -4; goto free_matrices;
    }

    /* S00, S01, S11 */
    gsl_matrix *tmp = gsl_matrix_alloc(n_var, n_var);
    gsl_matrix *R0Z_inv = gsl_matrix_alloc(n_var, nz);
    gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1.0, R0Z, invRZZ, 0.0, R0Z_inv);
    gsl_matrix *R1Z_inv = gsl_matrix_alloc(n_var, nz);
    gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1.0, R1Z, invRZZ, 0.0, R1Z_inv);
    gsl_matrix_free(invRZZ);

    gsl_matrix *S00 = gsl_matrix_alloc(n_var, n_var);
    gsl_matrix_memcpy(S00, R00);
    gsl_blas_dgemm(CblasNoTrans, CblasTrans, 1.0, R0Z_inv, R0Z, 0.0, tmp);
    gsl_matrix_sub(S00, tmp);

    gsl_matrix *S01 = gsl_matrix_alloc(n_var, n_var);
    gsl_matrix_memcpy(S01, R01);
    gsl_blas_dgemm(CblasNoTrans, CblasTrans, 1.0, R0Z_inv, R1Z, 0.0, tmp);
    gsl_matrix_sub(S01, tmp);

    gsl_matrix *S11 = gsl_matrix_alloc(n_var, n_var);
    gsl_matrix_memcpy(S11, R11);
    gsl_blas_dgemm(CblasNoTrans, CblasTrans, 1.0, R1Z_inv, R1Z, 0.0, tmp);
    gsl_matrix_sub(S11, tmp);

    gsl_matrix_free(R0Z_inv);
    gsl_matrix_free(R1Z_inv);
    gsl_matrix_free(tmp);

    /* free remaining moment matrices */
    gsl_matrix_free(R00); gsl_matrix_free(R01); gsl_matrix_free(R11);
    gsl_matrix_free(R0Z); gsl_matrix_free(R1Z); gsl_matrix_free(RZZ);

    /* generalised eigenvalue problem: A = S10 * inv(S00) * S01, B = S11 */
    gsl_matrix *invS00 = gsl_matrix_alloc(n_var, n_var);
    if (matrix_invert(S00, invS00) != GSL_SUCCESS) { res->error_code = -5; goto free_s; }

    gsl_matrix *S10 = gsl_matrix_alloc(n_var, n_var);
    gsl_matrix_transpose_memcpy(S10, S01);
    gsl_matrix *A = gsl_matrix_alloc(n_var, n_var);
    gsl_matrix *temp = gsl_matrix_alloc(n_var, n_var);
    gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1.0, S10, invS00, 0.0, temp);
    gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1.0, temp, S01, 0.0, A);
    gsl_matrix_free(invS00); gsl_matrix_free(S10); gsl_matrix_free(S01);
    gsl_matrix_free(temp);

    /* Solve generalised eigenproblem A x = lambda B x using gsl_eigen_genv
       to obtain both eigenvalues and eigenvectors */
    gsl_vector_complex *alpha = gsl_vector_complex_alloc(n_var);
    gsl_vector *beta = gsl_vector_alloc(n_var);
    gsl_matrix_complex *evec = gsl_matrix_complex_alloc(n_var, n_var);
    gsl_matrix *Acopy = gsl_matrix_alloc(n_var, n_var);
    gsl_matrix_memcpy(Acopy, A);
    gsl_matrix *Bcopy = gsl_matrix_alloc(n_var, n_var);
    gsl_matrix_memcpy(Bcopy, S11);
    gsl_matrix_free(A); gsl_matrix_free(S11);

    gsl_eigen_genv_workspace *w = gsl_eigen_genv_alloc(n_var);
    gsl_error_handler_t *old = gsl_set_error_handler_off();
    int eig_status = gsl_eigen_genv(Acopy, Bcopy, alpha, beta, evec, w);
    gsl_set_error_handler(old);
    if (eig_status) {
        res->error_code = -3;
        gsl_eigen_genv_free(w);
        gsl_vector_complex_free(alpha); gsl_vector_free(beta);
        gsl_matrix_complex_free(evec);
        gsl_matrix_free(Acopy); gsl_matrix_free(Bcopy);
        goto free_s;
    }
    gsl_eigen_genv_free(w);

    /* compute eigenvalues and allocate eigenvector storage */
    res->eigenvalues = calloc(n_var, sizeof(double));
    res->eigenvectors = calloc(n_var, sizeof(double*));
    for (int i=0;i<n_var;i++)
        res->eigenvectors[i] = calloc(n_var, sizeof(double));

    for (int i=0;i<n_var;i++) {
        gsl_complex a = gsl_vector_complex_get(alpha, i);
        double b = gsl_vector_get(beta, i);
        res->eigenvalues[i] = (fabs(b) > 1e-12) ? GSL_REAL(a) / b : 0.0;
    }
    gsl_vector_complex_free(alpha); gsl_vector_free(beta);

    /* sort eigenvalues and eigenvectors together (descending by eigenvalue) */
    for (int i=0;i<n_var-1;i++) {
        for (int j=i+1;j<n_var;j++) {
            if (res->eigenvalues[j] > res->eigenvalues[i]) {
                double tmp = res->eigenvalues[i];
                res->eigenvalues[i] = res->eigenvalues[j];
                res->eigenvalues[j] = tmp;
                /* swap columns i and j in evec */
                for (int k=0;k<n_var;k++) {
                    gsl_complex tmp_c = gsl_matrix_complex_get(evec, k, i);
                    gsl_matrix_complex_set(evec, k, i,
                        gsl_matrix_complex_get(evec, k, j));
                    gsl_matrix_complex_set(evec, k, j, tmp_c);
                }
            }
        }
    }

    /* store real parts of eigenvectors (columns in evec -> rows in eigenvectors) */
    for (int i=0;i<n_var;i++)
        for (int j=0;j<n_var;j++)
            res->eigenvectors[j][i] = GSL_REAL(gsl_matrix_complex_get(evec, j, i));
    gsl_matrix_complex_free(evec);
    gsl_matrix_free(Acopy); gsl_matrix_free(Bcopy);

    /* trace and max statistics */
    res->trace_stat = calloc(n_var, sizeof(double));
    res->max_stat   = calloc(n_var, sizeof(double));
    for (int r=0; r<n_var; r++) {
        double trace = 0.0;
        for (int i=r; i<n_var; i++) {
            double lam = res->eigenvalues[i];
            if (lam > 0 && lam < 1) trace += -nobs * log(1.0 - lam);
        }
        res->trace_stat[r] = trace;
        res->max_stat[r] = (r < n_var-1 && res->eigenvalues[r] > 0 && res->eigenvalues[r] < 1)
                           ? -nobs * log(1.0 - res->eigenvalues[r]) : 0.0;
    }

    /* critical values — per-rank: table indexed by dimension (n - r),
       where n - r is the number of common trends under H0: rank <= r */
    int idx_det = (det == JOH_DET_CONST_TREND) ? 1 : 0;
    res->trace_cv10 = calloc(n_var, sizeof(double));
    res->trace_cv5  = calloc(n_var, sizeof(double));
    res->trace_cv1  = calloc(n_var, sizeof(double));
    res->max_cv10   = calloc(n_var, sizeof(double));
    res->max_cv5    = calloc(n_var, sizeof(double));
    res->max_cv1    = calloc(n_var, sizeof(double));

    for (int r=0; r<n_var; r++) {
        int dim = n_var - r;  /* number of common trends under H0 */
        int idx_n = (dim - 1 < 5) ? (dim - 1) : 4;
        if (idx_n < 0) idx_n = 0;
        int i = r;
        res->trace_cv10[i] = johansen_cv[idx_n][idx_det][0][0];
        res->trace_cv5[i]  = johansen_cv[idx_n][idx_det][0][1];
        res->trace_cv1[i]  = johansen_cv[idx_n][idx_det][0][2];
        res->max_cv10[i]   = johansen_cv[idx_n][idx_det][1][0];
        res->max_cv5[i]    = johansen_cv[idx_n][idx_det][1][1];
        res->max_cv1[i]    = johansen_cv[idx_n][idx_det][1][2];
    }

    /* --- compute interpolated p-values --- */
    res->trace_pval = calloc(n_var, sizeof(double));
    res->max_pval   = calloc(n_var, sizeof(double));
    for (int r=0; r<n_var; r++) {
        double Tt = res->trace_stat[r];
        double c10 = res->trace_cv10[r], c5 = res->trace_cv5[r], c1 = res->trace_cv1[r];
        if (Tt <= c10)
            res->trace_pval[r] = 0.10 + 0.90 * (c10 - Tt) / (c10 > 1e-12 ? c10 : 1.0);
        else if (Tt <= c5)
            res->trace_pval[r] = 0.10 * pow(0.50, (Tt - c10) / (c5 - c10 + 1e-12));
        else if (Tt <= c1)
            res->trace_pval[r] = 0.05 * pow(0.20, (Tt - c5)  / (c1 - c5  + 1e-12));
        else
            res->trace_pval[r] = 0.01 * c1 / (Tt + 1e-12);
        if (res->trace_pval[r] < 1e-6) res->trace_pval[r] = 1e-6;
        if (res->trace_pval[r] > 1.0)  res->trace_pval[r] = 1.0;
    }
    for (int r=0; r<n_var; r++) {
        if (r == n_var-1) { res->max_pval[r] = 1.0; continue; }
        double Tm = res->max_stat[r];
        double c10 = res->max_cv10[r], c5 = res->max_cv5[r], c1 = res->max_cv1[r];
        if (Tm <= c10)
            res->max_pval[r] = 0.10 + 0.90 * (c10 - Tm) / (c10 > 1e-12 ? c10 : 1.0);
        else if (Tm <= c5)
            res->max_pval[r] = 0.10 * pow(0.50, (Tm - c10) / (c5 - c10 + 1e-12));
        else if (Tm <= c1)
            res->max_pval[r] = 0.05 * pow(0.20, (Tm - c5)  / (c1 - c5  + 1e-12));
        else
            res->max_pval[r] = 0.01 * c1 / (Tm + 1e-12);
        if (res->max_pval[r] < 1e-6) res->max_pval[r] = 1e-6;
        if (res->max_pval[r] > 1.0)  res->max_pval[r] = 1.0;
    }

    /* determine ranks at both 5% and 10% levels */
    res->r_trace_5 = n_var;
    res->r_trace_10 = n_var;
    res->r_max_5 = n_var;
    res->r_max_10 = n_var;
    for (int r=0; r<n_var; r++) {
        if (res->trace_stat[r] <= res->trace_cv5[r])  { res->r_trace_5 = r;  break; }
    }
    for (int r=0; r<n_var; r++) {
        if (res->trace_stat[r] <= res->trace_cv10[r]) { res->r_trace_10 = r; break; }
    }
    for (int r=0; r<n_var-1; r++) {
        if (res->max_stat[r] <= res->max_cv5[r])  { res->r_max_5 = r;  break; }
    }
    for (int r=0; r<n_var-1; r++) {
        if (res->max_stat[r] <= res->max_cv10[r]) { res->r_max_10 = r; break; }
    }
    /* backward-compat fields */
    res->r_trace = res->r_trace_5;
    res->r_max   = res->r_max_5;
    res->coint_rank = res->r_trace_5;  /* no longer a forced consensus */

    /* build conclusion string */
    res->conclusion = johansen_conclusion_string(res);

    return res;

free_s:
    if (S00) gsl_matrix_free(S00);
    if (S01) gsl_matrix_free(S01);
    if (S11) gsl_matrix_free(S11);
    goto err;

free_matrices:
    gsl_matrix_free(R00); gsl_matrix_free(R01); gsl_matrix_free(R11);
    gsl_matrix_free(R0Z); gsl_matrix_free(R1Z); gsl_matrix_free(RZZ);

free_y:
    for (int k=0; k < n_obs; k++) {
        if (y[k]) free(y[k]);
    }
    free(y);

err:
    res->conclusion = strdup("Johansen test failed due to numerical problems.");
    res->r_trace = res->r_max = res->coint_rank = 0;
    res->r_trace_5 = res->r_trace_10 = res->r_max_5 = res->r_max_10 = 0;
    return res;
}

char * johansen_conclusion_string(const JohansenResult *res) {
    if (!res) return strdup("Invalid result.");
    if (res->error_code != 0) {
        char buf[256];
        snprintf(buf, sizeof(buf), "Johansen test failed (error code %d).", res->error_code);
        return strdup(buf);
    }
    char *msg = malloc(2048);
    msg[0] = '\0';
    strcat(msg, "Johansen Cointegration Test Results\n");
    strcat(msg, "-----------------------------------\n");
    strcat(msg, "Trace test:\n");
    for (int r=0; r<res->n_var; r++) {
        char line[160];
        snprintf(line, sizeof(line),
                 "  H0: r <= %d   trace = %.3f  p-val = %.4f  (10%%:%.2f, 5%%:%.2f, 1%%:%.2f)\n",
                 r, res->trace_stat[r], res->trace_pval[r],
                 res->trace_cv10[r], res->trace_cv5[r], res->trace_cv1[r]);
        strcat(msg, line);
    }
    snprintf(msg+strlen(msg), 2048-strlen(msg),
             " => Trace test: %d coint. rel. at 5%%, %d coint. rel. at 10%%\n\n",
             res->r_trace_5, res->r_trace_10);

    strcat(msg, "Max eigenvalue test:\n");
    for (int r=0; r<res->n_var-1; r++) {
        char line[160];
        snprintf(line, sizeof(line),
                 "  H0: r = %d vs r = %d   max = %.3f  p-val = %.4f  (10%%:%.2f, 5%%:%.2f, 1%%:%.2f)\n",
                 r, r+1, res->max_stat[r], res->max_pval[r],
                 res->max_cv10[r], res->max_cv5[r], res->max_cv1[r]);
        strcat(msg, line);
    }
    snprintf(msg+strlen(msg), 2048-strlen(msg),
             " => Max-eigenvalue test: %d coint. rel. at 5%%, %d coint. rel. at 10%%\n\n",
             res->r_max_5, res->r_max_10);

    strcat(msg, "User should choose trace or max-eigenvalue result "
                "according to preference.\n");
    return msg;
}

char * johansen_full_report_string(const JohansenResult *res) {
    if (!res) return strdup("Invalid result.");
    if (res->error_code != 0) {
        char buf[256];
        snprintf(buf, sizeof(buf), "Johansen test failed (error code %d).", res->error_code);
        return strdup(buf);
    }

    char *msg = malloc(16384);
    msg[0] = '\0';

    strcat(msg, "======================================================\n");
    strcat(msg, "  JOHANSEN COINTEGRATION TEST — FULL ESTIMATION OUTPUT\n");
    strcat(msg, "======================================================\n\n");

    /* Eigenvalues section */
    strcat(msg, "Estimated eigenvalues (sorted descending):\n");
    strcat(msg, "  i      eigenvalue       lambda_i\n");
    strcat(msg, " ---   ------------   ------------\n");
    for (int i=0; i<res->n_var; i++) {
        char line[128];
        snprintf(line, sizeof(line), "  %2d   %10.6f    %10.6f\n",
                 i+1, res->eigenvalues[i], 1.0 - res->eigenvalues[i]);
        strcat(msg, line);
    }
    strcat(msg, "\n");

    /* Trace test table */
    strcat(msg, "Trace test (H0: rank <= r vs H1: rank > r):\n");
    strcat(msg, "  r      trace stat    p-val     10% cv     5% cv     1% cv\n");
    strcat(msg, " ---   ------------   -------   --------   --------   --------\n");
    for (int r=0; r<res->n_var; r++) {
        char line[160];
        snprintf(line, sizeof(line), "  %2d    %10.3f   %7.4f    %7.2f     %7.2f     %7.2f\n",
                 r, res->trace_stat[r], res->trace_pval[r],
                 res->trace_cv10[r], res->trace_cv5[r], res->trace_cv1[r]);
        strcat(msg, line);
    }
    snprintf(msg+strlen(msg), 16384-strlen(msg),
             "  => Trace test: %d coint. rel. at 5%%, %d coint. rel. at 10%%\n\n",
             res->r_trace_5, res->r_trace_10);

    /* Max eigenvalue test table */
    strcat(msg, "Max eigenvalue test (H0: r vs H1: r+1):\n");
    strcat(msg, "  r      max stat      p-val     10% cv     5% cv     1% cv\n");
    strcat(msg, " ---   ------------   -------   --------   --------   --------\n");
    for (int r=0; r<res->n_var-1; r++) {
        char line[160];
        snprintf(line, sizeof(line), "  %2d    %10.3f   %7.4f    %7.2f     %7.2f     %7.2f\n",
                 r, res->max_stat[r], res->max_pval[r],
                 res->max_cv10[r], res->max_cv5[r], res->max_cv1[r]);
        strcat(msg, line);
    }
    snprintf(msg+strlen(msg), 16384-strlen(msg),
             "  => Max-eigenvalue test: %d coint. rel. at 5%%, %d coint. rel. at 10%%\n\n",
             res->r_max_5, res->r_max_10);

    /* Cointegration vectors (eigenvectors) */
    if (res->r_trace_5 > 0 && res->eigenvectors) {
        strcat(msg, "Cointegration vectors (beta', first r eigenvectors by trace test):\n");
        for (int i=0; i<res->r_trace_5; i++) {
            char line[256];
            snprintf(line, sizeof(line), "  Vector %d: [", i+1);
            strcat(msg, line);
            for (int j=0; j<res->n_var; j++) {
                char num[32];
                snprintf(num, sizeof(num), " %10.6f", res->eigenvectors[j][i]);
                strcat(msg, num);
            }
            strcat(msg, " ]\n");
        }
    }

    snprintf(msg+strlen(msg), 16384-strlen(msg),
             "\nRank (trace, 5%%): %d    Rank (trace, 10%%): %d\n"
             "Rank (max,   5%%): %d    Rank (max,   10%%): %d\n",
             res->r_trace_5, res->r_trace_10, res->r_max_5, res->r_max_10);
    return msg;
}

void johansen_result_free(JohansenResult *res) {
    if (!res) return;
    free(res->eigenvalues);
    if (res->eigenvectors) {
        for (int i=0;i<res->n_var;i++)
            free(res->eigenvectors[i]);
        free(res->eigenvectors);
    }
    free(res->trace_stat); free(res->trace_cv10); free(res->trace_cv5); free(res->trace_cv1);
    free(res->trace_pval);
    free(res->max_stat);   free(res->max_cv10);   free(res->max_cv5);   free(res->max_cv1);
    free(res->max_pval);
    free(res->conclusion);
    free(res);
}