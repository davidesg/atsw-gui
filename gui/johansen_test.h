/**
 * @file johansen_test.h
 * @brief Johansen cointegration test – standalone module with explicit conclusions
 *
 * @date 2025
 * @author (refactored from drvarma_gui original code)
 */

#ifndef JOHANSEN_TEST_H
#define JOHANSEN_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <gsl/gsl_matrix.h>

/** Deterministic term specifications */
typedef enum {
    JOH_DET_NONE,           /**< No deterministic term */
    JOH_DET_CONST,          /**< Constant (restricted) */
    JOH_DET_CONST_TREND,    /**< Constant + Trend (restricted) */
    JOH_DET_CONST_DUMMY     /**< Constant + Step dummy (restricted) */
} JohansenDetType;

/** Structure holding the complete Johansen test result */
typedef struct {
    int n_var;                  /**< number of variables */
    int p;                      /**< lag order in differences */
    JohansenDetType det;        /**< deterministic specification */
    int use_logs;               /**< whether logs were applied */

    /* eigenvalues (sorted descending) */
    double *eigenvalues;        /**< array of n_var eigenvalues */
    double **eigenvectors;      /**< matrix [n_var][n_var], column i = eigenvector for eigenvalue i (real part) */

    /* trace test statistics for each rank r = 0 .. n_var-1 */
    double *trace_stat;         /**< trace statistic for H0: r <= k */
    double *trace_cv10;         /**< 10% critical value for each r */
    double *trace_cv5;          /**<  5% critical value */
    double *trace_cv1;          /**<  1% critical value */
    double *trace_pval;         /**<  interpolated p-value for each r */

    /* max eigenvalue test statistics for each rank */
    double *max_stat;           /**< max eigenvalue statistic for H0: r = k vs r = k+1 */
    double *max_cv10;           /**< 10% critical value */
    double *max_cv5;            /**<  5% critical value */
    double *max_cv1;            /**<  1% critical value */
    double *max_pval;           /**<  interpolated p-value for each r */

    /* cointegration rank according to sequential procedure */
    int r_trace_10;             /**< rank from trace test at 10% */
    int r_trace_5;              /**< rank from trace test at 5% */
    int r_max_10;               /**< rank from max-eigenvalue test at 10% */
    int r_max_5;                /**< rank from max-eigenvalue test at 5% */

    /* legacy fields (kept for backward compatibility) */
    int r_trace;                /**< @deprecated use r_trace_5 */
    int r_max;                  /**< @deprecated use r_max_5 */
    int coint_rank;             /**< @deprecated user should choose trace or max */

    /* a descriptive message summarising the conclusion */
    char *conclusion;           /**< human‑readable conclusion string */

    int error_code;             /**< 0 on success, negative on failure */
} JohansenResult;

/**
 * @brief Execute the Johansen cointegration test
 *
 * @param data      matrix [n_obs][n_var] of original data
 * @param n_obs     number of observations
 * @param n_var     number of variables (max 5 for critical values)
 * @param p         lag order of the VAR in differences (>=1)
 * @param det       type of deterministic term
 * @param use_logs  if non‑zero, apply natural logarithm
 * @return JohansenResult* pointer to filled result; must be freed with johansen_result_free()
 */
JohansenResult * johansen_test_run(const double * const * data,
                                   int n_obs, int n_var, int p,
                                   JohansenDetType det, int use_logs);

/**
 * @brief Free memory allocated for JohansenResult
 */
void johansen_result_free(JohansenResult *res);

/**
 * @brief Produce a human‑readable conclusion string (replaces res->conclusion)
 *
 * Can be called after johansen_test_run to rebuild the message if needed.
 */
char * johansen_conclusion_string(const JohansenResult *res);

/**
 * @brief Produce a full detailed report with eigenvalues, trace/max tables,
 *        and cointegration vectors.
 */
char * johansen_full_report_string(const JohansenResult *res);

#ifdef __cplusplus
}
#endif

#endif /* JOHANSEN_TEST_H */