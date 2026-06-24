/*****************************************************************************/
/*  vecm.h -- part of drvarma (multivariate VARMA modelling).
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
 * @file vecm.h
 * @brief VECM (Vector Error Correction Model) estimation with weak exogeneity
 *        and Granger causality tests for each equation.
 *
 * Uses cointegration vectors from Johansen and estimates the VECM
 * by OLS equation-by-equation. Reports adjustment coefficients (alpha),
 * short-run dynamics (Gamma), weak exogeneity F-tests, and Granger causality.
 *
 * @date 2025-2026
 * @author David E. Guerrero
 */

#ifndef VECM_H
#define VECM_H

#ifdef __cplusplus
extern "C" {
#endif

/** Deterministic term specification (matches JohansenDetType) */
typedef enum {
    VECM_DET_NONE,
    VECM_DET_CONST,
    VECM_DET_CONST_TREND,
    VECM_DET_CONST_DUMMY
} VecmDetType;

/** Structure holding the complete VECM estimation result */
typedef struct {
    int n_var;                  /**< number of variables */
    int p;                      /**< VAR lag order in levels (p >= 1) */
    int r;                      /**< cointegration rank */
    VecmDetType det;            /**< deterministic specification */
    int use_logs;
    int nobs;                   /**< effective observations (n_obs - p) */
    int npar;                   /**< parameters per equation */

    double *eigenvalues;        /**< [n_var] from Johansen */
    double **beta;              /**< cointegration vectors [n_var][n_var], first r cols active */

    double **alpha;             /**< [n_var][r] adjustment coefficients */
    double **alpha_se;          /**< standard errors */
    double **alpha_tstat;       /**< t-statistics */
    double **alpha_pval;        /**< individual p-values */

    double ***gamma;            /**< [p-1][n_var][n_var] short-run coeffs */
    double ***gamma_se;
    double ***gamma_tstat;
    double ***gamma_pval;

    double **det_coef;          /**< [n_var][n_det] deterministic coeffs */
    double **det_se;
    double **det_tstat;
    double **det_pval;

    double *r_squared;          /**< per-equation R² */
    double *adj_r_squared;
    double *f_stat;             /**< overall F per equation */
    double *f_pval;
    double *sigma;              /**< residual standard error */
    double *ssr;                /**< sum of squared residuals */

    double **resid_cov;         /**< residual covariance matrix [n_var][n_var] */

    double *weak_exog_f;        /**< joint weak exogeneity F per variable */
    double *weak_exog_pval;

    double **granger_f;         /**< Granger causality F: [target][source] */
    double **granger_pval;

    char *report;
    int error_code;
} VecmResult;

/**
 * @brief Estimate a VECM model.
 *
 * @param data        [n_obs][n_var] original data
 * @param n_obs       number of observations
 * @param n_var       number of variables
 * @param p           VAR lag order in levels (p >= 1)
 * @param r           cointegration rank (0 < r < n_var)
 * @param det         deterministic specification
 * @param use_logs    apply natural logarithm if non-zero
 * @param beta_joh    cointegration vectors from Johansen [n_var][n_var];
 *                    can be NULL to run Johansen internally
 * @return VecmResult* must be freed with vecm_result_free()
 */
VecmResult * vecm_estimate(const double * const * data,
                           int n_obs, int n_var, int p, int r,
                           VecmDetType det, int use_logs,
                           const double * const * beta_joh);

void vecm_result_free(VecmResult *res);
char * vecm_report_string(const VecmResult *res);

#ifdef __cplusplus
}
#endif

#endif /* VECM_H */
