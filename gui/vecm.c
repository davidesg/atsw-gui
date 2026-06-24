/*****************************************************************************/
/*  vecm.c -- part of drvarma (multivariate VARMA modelling).
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
 * @file vecm.c
 * @brief VECM estimation, weak exogeneity and Granger causality tests.
 *
 * Uses GSL for linear algebra. Reuses the Johansen module (johansen_test.h)
 * for cointegration vectors when not supplied by the caller.
 */

#include "vecm.h"
#include "johansen_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_cdf.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_statistics_double.h>

/* ---------- Helper: matrix inversion via LU (returns 0 on success) ---------- */
static int mat_inv(gsl_matrix *a, gsl_matrix *inv) {
    int n = a->size1;
    gsl_matrix *copy = gsl_matrix_alloc(n, n);
    gsl_matrix_memcpy(copy, a);
    gsl_permutation *p = gsl_permutation_alloc(n);
    int signum;
    gsl_error_handler_t *old = gsl_set_error_handler_off();
    int status = gsl_linalg_LU_decomp(copy, p, &signum);
    if (status) {
        /* Tikhonov regularisation */
        double diag = 0.0;
        for (int i=0;i<n;i++) diag += gsl_matrix_get(a,i,i);
        double lambda = 1e-3 * (diag / n);
        if (lambda < 1e-12) lambda = 1e-3;
        gsl_matrix_memcpy(copy, a);
        for (int i=0;i<n;i++) {
            gsl_matrix_set(copy, i, i, gsl_matrix_get(copy,i,i) + lambda);
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

/* ---------- OLS estimation for one equation ---------- */
static int ols_equation(const gsl_matrix *X, const gsl_vector *y,
                        gsl_vector *beta, gsl_vector *se,
                        gsl_vector *tstat, gsl_vector *pval,
                        double *r2, double *adj_r2,
                        double *fstat, double *fpval,
                        double *sigma, double *ssr) {
    int nobs = X->size1;
    int k    = X->size2;
    if (nobs <= k) return -1;

    /* X'X and X'y */
    gsl_matrix *XtX = gsl_matrix_alloc(k, k);
    gsl_blas_dgemm(CblasTrans, CblasNoTrans, 1.0, X, X, 0.0, XtX);
    gsl_vector *Xty = gsl_vector_alloc(k);
    gsl_blas_dgemv(CblasTrans, 1.0, X, y, 0.0, Xty);

    /* save a copy of X'X for covariance before LU destroys it */
    gsl_matrix *XtX_for_cov = gsl_matrix_alloc(k, k);
    gsl_matrix_memcpy(XtX_for_cov, XtX);

    /* solve X'X beta = X'y */
    gsl_vector_memcpy(beta, Xty);
    gsl_permutation *perm = gsl_permutation_alloc(k);
    int signum;
    int status = gsl_linalg_LU_decomp(XtX, perm, &signum);
    if (status) {
        gsl_matrix_free(XtX);
        gsl_vector_free(Xty);
        gsl_permutation_free(perm);
        return -2;
    }
    gsl_linalg_LU_solve(XtX, perm, Xty, beta);

    /* residuals */
    gsl_vector *res = gsl_vector_alloc(nobs);
    gsl_vector_memcpy(res, y);
    gsl_blas_dgemv(CblasNoTrans, -1.0, X, beta, 1.0, res);
    double rss = 0.0;
    gsl_blas_ddot(res, res, &rss);

    /* TSS = sum (y_i - ybar)^2 */
    double ymean = gsl_stats_mean(y->data, 1, nobs);
    double tss = 0.0;
    for (int i=0;i<nobs;i++) {
        double d = gsl_vector_get(y,i) - ymean;
        tss += d*d;
    }
    *ssr = rss;
    double r2val = (tss > 1e-15) ? 1.0 - rss/tss : 0.0;
    *r2 = r2val;
    *adj_r2 = 1.0 - (1.0 - r2val) * (nobs - 1.0) / (nobs - k);
    *sigma = sqrt(rss / (nobs - k));
    double fval = (tss > 1e-15 && r2val < 1.0 - 1e-15)
                  ? (r2val / (k - 1)) / ((1.0 - r2val) / (nobs - k)) : 0.0;
    *fstat = fval;
    *fpval = gsl_cdf_fdist_Q(fval, k - 1, nobs - k);

    /* standard errors: Cov(beta) = sigma^2 * inv(X'X) — use saved copy */
    double s2 = rss / (nobs - k);
    gsl_matrix *invXtX = gsl_matrix_alloc(k, k);
    if (mat_inv(XtX_for_cov, invXtX) != 0) {
        /* fallback: set SE to NaN */
        for (int j=0;j<k;j++) gsl_vector_set(se, j, NAN);
    } else {
        for (int j=0;j<k;j++) {
            double v = gsl_matrix_get(invXtX, j, j) * s2;
            if (v < 0) v = 0;
            double se_j = sqrt(v);
            gsl_vector_set(se, j, se_j);
        }
    }
    gsl_matrix_free(invXtX);
    gsl_matrix_free(XtX_for_cov);

    /* t-statistics and p-values */
    for (int j=0;j<k;j++) {
        double t = (gsl_vector_get(se,j) > 1e-15)
                   ? gsl_vector_get(beta,j) / gsl_vector_get(se,j) : 0.0;
        gsl_vector_set(tstat, j, t);
        gsl_vector_set(pval, j, 2.0 * gsl_cdf_tdist_Q(fabs(t), nobs - k));
    }

    gsl_matrix_free(XtX);
    gsl_vector_free(Xty);
    gsl_vector_free(res);
    gsl_permutation_free(perm);
    return 0;
}

/* ---------- Build deterministic regressors for one observation ---------- */
static int ndet(VecmDetType det) {
    switch(det) {
        case VECM_DET_NONE:          return 0;
        case VECM_DET_CONST:         return 1;
        case VECM_DET_CONST_TREND:   return 2;
        case VECM_DET_CONST_DUMMY:   return 1;  /* step handled separately */
        default: return 0;
    }
}

/* ---------- Main VECM estimation ---------- */
VecmResult * vecm_estimate(const double * const * data,
                           int n_obs, int n_var, int p, int r,
                           VecmDetType det, int use_logs,
                           const double * const * beta_joh) {
    VecmResult *res = calloc(1, sizeof(VecmResult));
    if (!res) return NULL;
    res->n_var = n_var;
    res->p = p;
    res->r = r;
    res->det = det;
    res->use_logs = use_logs;
    res->error_code = 0;

    if (n_obs < p + 2 || n_var < 2 || r < 1 || r >= n_var) {
        res->error_code = -1; return res;
    }

    /* ---- 1. Apply log transform if requested ---- */
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

    /* ---- 2. Run Johansen if beta not supplied ---- */
    res->eigenvalues = calloc(n_var, sizeof(double));
    res->beta = calloc(n_var, sizeof(double*));
    for (int i=0;i<n_var;i++) res->beta[i] = calloc(n_var, sizeof(double));

    if (beta_joh) {
        for (int i=0;i<n_var;i++)
            for (int j=0;j<n_var;j++)
                res->beta[i][j] = beta_joh[i][j];
    } else {
        JohansenDetType jdet;
        switch(det) {
            case VECM_DET_NONE:          jdet = JOH_DET_NONE; break;
            case VECM_DET_CONST:         jdet = JOH_DET_CONST; break;
            case VECM_DET_CONST_TREND:   jdet = JOH_DET_CONST_TREND; break;
            case VECM_DET_CONST_DUMMY:   jdet = JOH_DET_CONST_DUMMY; break;
            default: jdet = JOH_DET_CONST;
        }
        JohansenResult *jres = johansen_test_run(data, n_obs, n_var, p, jdet, use_logs);
        if (!jres || jres->error_code != 0) {
            res->error_code = -3;
            goto free_y;
        }
        for (int i=0;i<n_var;i++) {
            res->eigenvalues[i] = jres->eigenvalues[i];
            if (jres->eigenvectors) {
                for (int j=0;j<n_var;j++)
                    res->beta[i][j] = jres->eigenvectors[i][j];
            }
        }
        johansen_result_free(jres);
    }

    /* ---- 3. Form error correction terms: ecm[t-1][c] = sum_j beta[j][c] * y[t-1][j] ---- */
    int nobs_eff = n_obs - p;
    res->nobs = nobs_eff;
    double **ecm = malloc((n_obs+1) * sizeof(double*));
    for (int t=0;t<=n_obs;t++) {
        ecm[t] = calloc(r, sizeof(double));
        if (t >= 1) {
            for (int c=0;c<r;c++) {
                double s = 0.0;
                for (int j=0;j<n_var;j++)
                    s += res->beta[j][c] * y[t-1][j];
                ecm[t][c] = s;
            }
        }
    }

    /* ---- 4. Number of deterministic terms ---- */
    int n_det = ndet(det);

    /* ---- 5. Build design matrix common to all equations ---- */
    /* Regressors per observation (for equation j):
         ecm_{1..r} (t-1)              → r
         Δy_{1..n_var}(t-1 .. t-(p-1)) → n_var * (p-1)
         deterministic terms           → n_det
       Total npar = r + n_var*(p-1) + n_det
    */
    int npar = r + n_var * (p-1) + n_det;
    res->npar = npar;
    gsl_matrix *X = gsl_matrix_alloc(nobs_eff, npar);

    for (int t=0;t<nobs_eff;t++) {
        int t_abs = t + p;  /* absolute index in y (0-based) */
        int col = 0;
        /* ECTs */
        for (int c=0;c<r;c++)
            gsl_matrix_set(X, t, col++, ecm[t_abs][c]);
        /* Δy lags */
        for (int lag=1;lag<=p-1;lag++) {
            for (int v=0;v<n_var;v++) {
                double dy = y[t_abs - lag][v] - y[t_abs - lag - 1][v];
                gsl_matrix_set(X, t, col++, dy);
            }
        }
        /* deterministic */
        if (det == VECM_DET_CONST) {
            gsl_matrix_set(X, t, col++, 1.0);
        } else if (det == VECM_DET_CONST_TREND) {
            gsl_matrix_set(X, t, col++, 1.0);
            gsl_matrix_set(X, t, col++, (double)(t_abs + 1));
        } else if (det == VECM_DET_CONST_DUMMY) {
            double step = (t_abs >= n_obs/2) ? 1.0 : 0.0;
            gsl_matrix_set(X, t, col++, step);
        }
    }

    /* ---- 6. Allocate result arrays ---- */
    res->alpha = calloc(n_var, sizeof(double*));
    res->alpha_se = calloc(n_var, sizeof(double*));
    res->alpha_tstat = calloc(n_var, sizeof(double*));
    res->alpha_pval = calloc(n_var, sizeof(double*));
    res->det_coef = calloc(n_var, sizeof(double*));
    res->det_se = calloc(n_var, sizeof(double*));
    res->det_tstat = calloc(n_var, sizeof(double*));
    res->det_pval = calloc(n_var, sizeof(double*));
    for (int j=0;j<n_var;j++) {
        res->alpha[j] = calloc(r, sizeof(double));
        res->alpha_se[j] = calloc(r, sizeof(double));
        res->alpha_tstat[j] = calloc(r, sizeof(double));
        res->alpha_pval[j] = calloc(r, sizeof(double));
        res->det_coef[j] = calloc(n_det, sizeof(double));
        res->det_se[j] = calloc(n_det, sizeof(double));
        res->det_tstat[j] = calloc(n_det, sizeof(double));
        res->det_pval[j] = calloc(n_det, sizeof(double));
    }

    res->gamma = calloc(p-1, sizeof(double**));
    res->gamma_se = calloc(p-1, sizeof(double**));
    res->gamma_tstat = calloc(p-1, sizeof(double**));
    res->gamma_pval = calloc(p-1, sizeof(double**));
    for (int l=0;l<p-1;l++) {
        res->gamma[l] = calloc(n_var, sizeof(double*));
        res->gamma_se[l] = calloc(n_var, sizeof(double*));
        res->gamma_tstat[l] = calloc(n_var, sizeof(double*));
        res->gamma_pval[l] = calloc(n_var, sizeof(double*));
        for (int j=0;j<n_var;j++) {
            res->gamma[l][j] = calloc(n_var, sizeof(double));
            res->gamma_se[l][j] = calloc(n_var, sizeof(double));
            res->gamma_tstat[l][j] = calloc(n_var, sizeof(double));
            res->gamma_pval[l][j] = calloc(n_var, sizeof(double));
        }
    }

    res->r_squared = calloc(n_var, sizeof(double));
    res->adj_r_squared = calloc(n_var, sizeof(double));
    res->f_stat = calloc(n_var, sizeof(double));
    res->f_pval = calloc(n_var, sizeof(double));
    res->sigma = calloc(n_var, sizeof(double));
    res->ssr = calloc(n_var, sizeof(double));

    res->resid_cov = calloc(n_var, sizeof(double*));
    for (int j=0;j<n_var;j++) res->resid_cov[j] = calloc(n_var, sizeof(double));

    res->weak_exog_f = calloc(n_var, sizeof(double));
    res->weak_exog_pval = calloc(n_var, sizeof(double));

    res->granger_f = calloc(n_var, sizeof(double*));
    res->granger_pval = calloc(n_var, sizeof(double*));
    for (int j=0;j<n_var;j++) {
        res->granger_f[j] = calloc(n_var, sizeof(double));
        res->granger_pval[j] = calloc(n_var, sizeof(double));
    }

    /* ---- 7. Estimate each equation ---- */
    gsl_vector *yvec = gsl_vector_alloc(nobs_eff);
    gsl_vector *beta_v = gsl_vector_alloc(npar);
    gsl_vector *se_v = gsl_vector_alloc(npar);
    gsl_vector *t_v = gsl_vector_alloc(npar);
    gsl_vector *p_v = gsl_vector_alloc(npar);

    /* Store residuals for covariance */
    gsl_matrix *all_res = gsl_matrix_alloc(nobs_eff, n_var);

    for (int eq=0;eq<n_var;eq++) {
        /* dependent variable: Δy_{eq,t} */
        for (int t=0;t<nobs_eff;t++) {
            int t_abs = t + p;
            double dy = y[t_abs][eq] - y[t_abs-1][eq];
            gsl_vector_set(yvec, t, dy);
        }

        if (ols_equation(X, yvec, beta_v, se_v, t_v, p_v,
                         &res->r_squared[eq], &res->adj_r_squared[eq],
                         &res->f_stat[eq], &res->f_pval[eq],
                         &res->sigma[eq], &res->ssr[eq]) != 0) {
            res->error_code = -4;
            goto cleanup;
        }

        /* Extract coefficients */
        int pos = 0;
        for (int c=0;c<r;c++) {
            res->alpha[eq][c] = gsl_vector_get(beta_v, pos);
            res->alpha_se[eq][c] = gsl_vector_get(se_v, pos);
            res->alpha_tstat[eq][c] = gsl_vector_get(t_v, pos);
            res->alpha_pval[eq][c] = gsl_vector_get(p_v, pos);
            pos++;
        }
        for (int lag=0;lag<p-1;lag++) {
            for (int v=0;v<n_var;v++) {
                res->gamma[lag][eq][v] = gsl_vector_get(beta_v, pos);
                res->gamma_se[lag][eq][v] = gsl_vector_get(se_v, pos);
                res->gamma_tstat[lag][eq][v] = gsl_vector_get(t_v, pos);
                res->gamma_pval[lag][eq][v] = gsl_vector_get(p_v, pos);
                pos++;
            }
        }
        for (int d=0;d<n_det;d++) {
            res->det_coef[eq][d] = gsl_vector_get(beta_v, pos);
            res->det_se[eq][d] = gsl_vector_get(se_v, pos);
            res->det_tstat[eq][d] = gsl_vector_get(t_v, pos);
            res->det_pval[eq][d] = gsl_vector_get(p_v, pos);
            pos++;
        }

        /* Store residuals for covariance */
        for (int t=0;t<nobs_eff;t++) {
            double yhat = 0.0;
            for (int k=0;k<npar;k++)
                yhat += gsl_matrix_get(X, t, k) * gsl_vector_get(beta_v, k);
            gsl_matrix_set(all_res, t, eq,
                           gsl_vector_get(yvec, t) - yhat);
        }
    }

    /* ---- 8. Residual covariance ---- */
    for (int i=0;i<n_var;i++)
        for (int j=0;j<n_var;j++) {
            double s = 0.0;
            for (int t=0;t<nobs_eff;t++)
                s += gsl_matrix_get(all_res, t, i) * gsl_matrix_get(all_res, t, j);
            res->resid_cov[i][j] = s / nobs_eff;
        }

    /* ---- 9. Weak exogeneity tests (joint α_j = 0 for each var j) ---- */
    /* Restricted model for eq j: drop ECT columns (first r columns of X) */
    int npar_restricted = npar - r;
    gsl_matrix *X_rest = NULL;
    gsl_vector *beta_r = NULL, *se_r = NULL, *t_r = NULL, *p_r = NULL;
    
    X_rest = gsl_matrix_alloc(nobs_eff, npar_restricted);
    for (int t=0;t<nobs_eff;t++) {
        int col = 0;
        for (int k=r;k<npar;k++)
            gsl_matrix_set(X_rest, t, col++, gsl_matrix_get(X, t, k));
    }
    beta_r = gsl_vector_alloc(npar_restricted);
    se_r = gsl_vector_alloc(npar_restricted);
    t_r = gsl_vector_alloc(npar_restricted);
    p_r = gsl_vector_alloc(npar_restricted);

    for (int eq=0;eq<n_var;eq++) {
        for (int t=0;t<nobs_eff;t++) {
            int t_abs = t + p;
            gsl_vector_set(yvec, t, y[t_abs][eq] - y[t_abs-1][eq]);
        }
        double r2_r, adj_r2_r, f_r, fp_r, sig_r, ssr_r;
        if (ols_equation(X_rest, yvec, beta_r, se_r, t_r, p_r,
                         &r2_r, &adj_r2_r, &f_r, &fp_r, &sig_r, &ssr_r) == 0) {
            res->weak_exog_f[eq] = ((ssr_r - res->ssr[eq]) / r) /
                                    (res->ssr[eq] / (nobs_eff - npar));
            if (res->weak_exog_f[eq] < 0) res->weak_exog_f[eq] = 0;
            res->weak_exog_pval[eq] = gsl_cdf_fdist_Q(
                res->weak_exog_f[eq], r, nobs_eff - npar);
        } else {
            res->weak_exog_f[eq] = 0;
            res->weak_exog_pval[eq] = 1.0;
        }
    }

    /* ---- 10. Granger causality tests ---- */
    /* For each target eq, test if all lags+pairs of variable source are zero.
       Restricted model: drop all Δy_{source, t-lag} columns (source fixed).
       Number of restrictions q = p-1 (one per lag, per source). */
    int q_gc = p - 1;
    if (q_gc > 0 && p > 1) {
        for (int target=0;target<n_var;target++) {
            for (int source=0;source<n_var;source++) {
                if (source == target) {
                    res->granger_f[target][source] = 0;
                    res->granger_pval[target][source] = 1.0;
                    continue;
                }
                /* Build restricted X: same as X but drop columns for source */
                int npar_gc = npar - q_gc;
                gsl_matrix *X_gc = gsl_matrix_alloc(nobs_eff, npar_gc);
                for (int t=0;t<nobs_eff;t++) {
                    int col = 0;
                    for (int k=0;k<npar;k++) {
                        /* Is this column a Δy_{source,·}? */
                        int is_source_col = 0;
                        if (k >= r) {
                            int kk = k - r;
                            int lag = kk / n_var;
                            int v   = kk % n_var;
                            if (lag < p-1 && v == source)
                                is_source_col = 1;
                        }
                        if (!is_source_col)
                            gsl_matrix_set(X_gc, t, col++,
                                           gsl_matrix_get(X, t, k));
                    }
                }
                for (int t=0;t<nobs_eff;t++) {
                    int t_abs = t + p;
                    gsl_vector_set(yvec, t, y[t_abs][target] - y[t_abs-1][target]);
                }
                gsl_vector *b_gc = gsl_vector_alloc(npar_gc);
                gsl_vector *s_gc = gsl_vector_alloc(npar_gc);
                gsl_vector *t_gc = gsl_vector_alloc(npar_gc);
                gsl_vector *p_gc = gsl_vector_alloc(npar_gc);
                double r2_gc, adj_r2_gc, f_gc, fp_gc, sig_gc, ssr_gc;
                if (ols_equation(X_gc, yvec, b_gc, s_gc, t_gc, p_gc,
                                 &r2_gc, &adj_r2_gc, &f_gc, &fp_gc, &sig_gc, &ssr_gc) == 0) {
                    double F = ((ssr_gc - res->ssr[target]) / q_gc) /
                                (res->ssr[target] / (nobs_eff - npar));
                    if (F < 0) F = 0;
                    res->granger_f[target][source] = F;
                    res->granger_pval[target][source] =
                        gsl_cdf_fdist_Q(F, q_gc, nobs_eff - npar);
                } else {
                    res->granger_f[target][source] = 0;
                    res->granger_pval[target][source] = 1.0;
                }
                gsl_vector_free(b_gc); gsl_vector_free(s_gc);
                gsl_vector_free(t_gc); gsl_vector_free(p_gc);
                gsl_matrix_free(X_gc);
            }
        }
    }

    /* ---- 11. Build report ---- */
    res->report = vecm_report_string(res);

cleanup:
    gsl_vector_free(yvec);
    gsl_vector_free(beta_v);
    gsl_vector_free(se_v);
    gsl_vector_free(t_v);
    gsl_vector_free(p_v);
    if (beta_r) gsl_vector_free(beta_r);
    if (se_r)   gsl_vector_free(se_r);
    if (t_r)    gsl_vector_free(t_r);
    if (p_r)    gsl_vector_free(p_r);
    gsl_matrix_free(X);
    if (X_rest) gsl_matrix_free(X_rest);
    gsl_matrix_free(all_res);

    for (int t=0;t<=n_obs;t++) free(ecm[t]);
    free(ecm);

free_y:
    for (int i=0;i<n_obs;i++) free(y[i]);
    free(y);

    return res;
}

/* ---------- Report generation ---------- */
char * vecm_report_string(const VecmResult *res) {
    if (!res) return strdup("Invalid result.");
    if (res->error_code != 0) {
        char buf[256];
        snprintf(buf, sizeof(buf), "VECM estimation failed (error code %d).", res->error_code);
        return strdup(buf);
    }

    char *msg = malloc(32768);
    msg[0] = '\0';
    int n_det = ndet(res->det);

    strcat(msg, "=======================================================\n");
    strcat(msg, "  VECTOR ERROR CORRECTION MODEL — ESTIMATION RESULTS\n");
    strcat(msg, "=======================================================\n\n");

    snprintf(msg+strlen(msg), 32768-strlen(msg),
             "Variables: %d  Lag order (levels): %d  Cointegration rank: %d\n"
             "Observations: %d  Parameters per equation: %d\n\n",
             res->n_var, res->p, res->r, res->nobs, res->npar);

    /* Adjustment coefficients α */
    strcat(msg, "-------------------------------------------------------\n");
    strcat(msg, "  Adjustment coefficients (alpha) — speed of adjustment\n");
    strcat(msg, "-------------------------------------------------------\n");
    for (int j=0;j<res->n_var;j++) {
        snprintf(msg+strlen(msg), 32768-strlen(msg),
                 "  Equation %d:\n", j+1);
        for (int c=0;c<res->r;c++) {
            char sig = (res->alpha_pval[j][c] < 0.01) ? '*' :
                       (res->alpha_pval[j][c] < 0.05) ? '+' : ' ';
            snprintf(msg+strlen(msg), 32768-strlen(msg),
                     "    ECT_%d  %10.6f  (s.e. %10.6f  t=%8.3f  p=%7.4f) %c\n",
                     c+1, res->alpha[j][c], res->alpha_se[j][c],
                     res->alpha_tstat[j][c], res->alpha_pval[j][c], sig);
        }
    }
    strcat(msg, "    (* p<0.01, + p<0.05)\n\n");

    /* Short-run coefficients Γ */
    if (res->p > 1) {
        strcat(msg, "-------------------------------------------------------\n");
        strcat(msg, "  Short-run coefficients (Gamma_i) with t-statistics\n");
        strcat(msg, "-------------------------------------------------------\n");
        for (int lag=0;lag<res->p-1;lag++) {
            snprintf(msg+strlen(msg), 32768-strlen(msg),
                     "  Lag %d:\n", lag+1);
            for (int j=0;j<res->n_var;j++) {
                snprintf(msg+strlen(msg), 32768-strlen(msg),
                         "    Eq %d:\n", j+1);
                for (int v=0;v<res->n_var;v++) {
                    char sig = (res->gamma_pval[lag][j][v] < 0.01) ? '*' :
                               (res->gamma_pval[lag][j][v] < 0.05) ? '+' : ' ';
                    snprintf(msg+strlen(msg), 32768-strlen(msg),
                             "      Var %d  %10.6f  (s.e. %10.6f  t=%8.3f  p=%7.4f) %c\n",
                             v+1, res->gamma[lag][j][v], res->gamma_se[lag][j][v],
                             res->gamma_tstat[lag][j][v], res->gamma_pval[lag][j][v], sig);
                }
            }
        }
        strcat(msg, "    (* p<0.01, + p<0.05)\n\n");
    }

    /* Deterministic terms */
    if (n_det > 0) {
        strcat(msg, "-------------------------------------------------------\n");
        strcat(msg, "  Deterministic terms\n");
        strcat(msg, "-------------------------------------------------------\n");
        for (int j=0;j<res->n_var;j++) {
            snprintf(msg+strlen(msg), 32768-strlen(msg),
                     "  Eq %d:\n", j+1);
            for (int d=0;d<n_det;d++) {
                char sig = (res->det_pval[j][d] < 0.01) ? '*' :
                           (res->det_pval[j][d] < 0.05) ? '+' : ' ';
                snprintf(msg+strlen(msg), 32768-strlen(msg),
                         "    Det %d  %10.6f  (s.e. %10.6f  t=%8.3f  p=%7.4f) %c\n",
                         d+1, res->det_coef[j][d], res->det_se[j][d],
                         res->det_tstat[j][d], res->det_pval[j][d], sig);
            }
        }
        strcat(msg, "\n");
    }

    /* Equation statistics */
    strcat(msg, "-------------------------------------------------------\n");
    strcat(msg, "  Equation statistics\n");
    strcat(msg, "-------------------------------------------------------\n");
    strcat(msg, "  Eq    R²      Adj R²     F-stat    F-pval      σ\n");
    strcat(msg, "  --  -------  -------  ---------  ---------  -------\n");
    for (int j=0;j<res->n_var;j++) {
        snprintf(msg+strlen(msg), 32768-strlen(msg),
                 "  %2d  %.4f   %.4f   %7.3f   %7.4f   %7.4f\n",
                 j+1, res->r_squared[j], res->adj_r_squared[j],
                 res->f_stat[j], res->f_pval[j], res->sigma[j]);
    }
    strcat(msg, "\n");

    /* Residual covariance */
    strcat(msg, "-------------------------------------------------------\n");
    strcat(msg, "  Residual covariance matrix\n");
    strcat(msg, "-------------------------------------------------------\n");
    for (int i=0;i<res->n_var;i++) {
        snprintf(msg+strlen(msg), 32768-strlen(msg), "  ");
        for (int j=0;j<res->n_var;j++) {
            snprintf(msg+strlen(msg), 32768-strlen(msg),
                     "  %10.6f", res->resid_cov[i][j]);
        }
        strcat(msg, "\n");
    }
    strcat(msg, "\n");

    /* Weak exogeneity tests */
    strcat(msg, "=======================================================\n");
    strcat(msg, "  WEAK EXOGENEITY TESTS (H0: alpha_j = 0)\n");
    strcat(msg, "=======================================================\n");
    strcat(msg, "  Variable      F-stat     p-value    Exogenous?\n");
    strcat(msg, "  ----------  ---------  ---------  -----------\n");
    for (int j=0;j<res->n_var;j++) {
        const char *verdict = (res->weak_exog_pval[j] > 0.05) ? "Yes" : "No ";
        snprintf(msg+strlen(msg), 32768-strlen(msg),
                 "     %2d       %7.3f    %7.4f       %s\n",
                 j+1, res->weak_exog_f[j], res->weak_exog_pval[j], verdict);
    }
    strcat(msg, "  (Exogenous at 5% if p > 0.05)\n\n");

    /* Granger causality */
    strcat(msg, "=======================================================\n");
    strcat(msg, "  GRANGER CAUSALITY TESTS (H0: source does NOT cause target)\n");
    strcat(msg, "=======================================================\n");
    strcat(msg, "  Target \\ Source");
    for (int s=0;s<res->n_var;s++)
        snprintf(msg+strlen(msg), 32768-strlen(msg), "    Var %d  ", s+1);
    strcat(msg, "\n");
    strcat(msg, "  ---------------");
    for (int s=0;s<res->n_var;s++)
        strcat(msg, "  --------");
    strcat(msg, "\n");

    for (int t=0;t<res->n_var;t++) {
        snprintf(msg+strlen(msg), 32768-strlen(msg), "     Var %d      ", t+1);
        for (int s=0;s<res->n_var;s++) {
            if (s == t) {
                strcat(msg, "     --    ");
            } else {
                const char *causes = (res->granger_pval[t][s] < 0.05) ? "Yes" : "No ";
                snprintf(msg+strlen(msg), 32768-strlen(msg),
                         "  %s(%5.3f)", causes, res->granger_pval[t][s]);
            }
        }
        strcat(msg, "\n");
    }
    strcat(msg, "  (Cell: 'Yes' = Granger-causes at 5%, with p-value in parentheses)\n");

    return msg;
}

/* ---------- Free ---------- */
void vecm_result_free(VecmResult *res) {
    if (!res) return;
    free(res->eigenvalues);
    if (res->beta) {
        for (int i=0;i<res->n_var;i++) free(res->beta[i]);
        free(res->beta);
    }
    if (res->alpha) {
        for (int i=0;i<res->n_var;i++) {
            free(res->alpha[i]); free(res->alpha_se[i]);
            free(res->alpha_tstat[i]); free(res->alpha_pval[i]);
        }
        free(res->alpha); free(res->alpha_se);
        free(res->alpha_tstat); free(res->alpha_pval);
    }
    if (res->gamma) {
        for (int l=0;l<res->p-1;l++) {
            if (res->gamma[l]) {
                for (int i=0;i<res->n_var;i++) {
                    free(res->gamma[l][i]);
                    free(res->gamma_se[l][i]);
                    free(res->gamma_tstat[l][i]);
                    free(res->gamma_pval[l][i]);
                }
                free(res->gamma[l]); free(res->gamma_se[l]);
                free(res->gamma_tstat[l]); free(res->gamma_pval[l]);
            }
        }
        free(res->gamma); free(res->gamma_se);
        free(res->gamma_tstat); free(res->gamma_pval);
    }
    if (res->det_coef) {
        for (int i=0;i<res->n_var;i++) {
            free(res->det_coef[i]); free(res->det_se[i]);
            free(res->det_tstat[i]); free(res->det_pval[i]);
        }
        free(res->det_coef); free(res->det_se);
        free(res->det_tstat); free(res->det_pval);
    }
    free(res->r_squared); free(res->adj_r_squared);
    free(res->f_stat); free(res->f_pval);
    free(res->sigma); free(res->ssr);
    if (res->resid_cov) {
        for (int i=0;i<res->n_var;i++) free(res->resid_cov[i]);
        free(res->resid_cov);
    }
    free(res->weak_exog_f); free(res->weak_exog_pval);
    if (res->granger_f) {
        for (int i=0;i<res->n_var;i++) free(res->granger_f[i]);
        free(res->granger_f);
    }
    if (res->granger_pval) {
        for (int i=0;i<res->n_var;i++) free(res->granger_pval[i]);
        free(res->granger_pval);
    }
    free(res->report);
    free(res);
}
