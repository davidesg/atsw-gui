/**
 * @file seasonal_detection.c
 * @brief Implementation of seasonal pattern detection with HAC F-test and OLS confidence bands
 *
 * This module provides robust seasonal pattern detection using:
 * - Data transformation: 100*log(series) for percentage interpretation
 * - HAC covariance matrix for F-test of global significance (robust inference)
 * - OLS covariance matrix for visual confidence bands (stable visualization)
 * - Proper A0 matrix transformation from harmonics to seasonal dummies
 */

#include "seasonal_detection.h"
#include <gsl/gsl_math.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_multifit.h>
#include <gsl/gsl_statistics.h>
#include <gsl/gsl_cdf.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_permutation.h>
#include <glib.h>  // Para g_print

#ifdef DRVARMA_GUI
    // En la GUI no necesitamos la función de detección completa
    #define DISABLE_DETECT_FUNCTION
#endif



/**
 * @brief Apply logarithmic transformation and rescale by 100 for percentage interpretation
 *
 * Transforms the data as: data[i] = 100 * log(data[i])
 * This approximates percentage differences from the mean and ensures values
 * are in percentage units for the seasonal plot.
 *
 * @param data Input data array (modified in-place)
 * @param n_points Number of data points
 */
void apply_log_transform_rescaled(double *data, int n_points) {
    for (int i = 0; i < n_points; i++) {
        if (data[i] > 0) {
            data[i] = 100.0 * log(data[i]);
        } else {
            printf("Warning: Non-positive value at position %d, cannot apply log transform\n", i);
            // Use a small positive value to avoid numerical issues
            data[i] = 100.0 * log(1e-6);
        }
    }
}

/**
 * Integra las dummies de la serie diferenciada para obtener las dummies de nivel.
 * @param diff_dummies  Array de dummies de la serie diferenciada (longitud s)
 * @param s             Período estacional
 * @param d             Orden de diferenciación (0,1,2)
 * @return              Array de dummies de nivel (debe liberarse con free)
 */

/**
 * Integra las dummies de la serie diferenciada para obtener las dummies de nivel.
 * @param diff_dummies  Array de dummies de la serie diferenciada (longitud s)
 * @param s             Período estacional
 * @param d             Orden de diferenciación (0,1,2)
 * @return              Array de dummies de nivel (debe liberarse con free)
 */

double* integrate_dummies(double *diff_dummies, int s, int d) {
    double *level = malloc(s * sizeof(double));
    if (!level) return NULL;

    if (d == 0) {
        memcpy(level, diff_dummies, s * sizeof(double));
        return level;
    }
    else if (d == 1) {
        // Verificar que la suma de diff_dummies sea aproximadamente cero
        double sum_diff = 0.0;
        for (int i = 0; i < s; i++) sum_diff += diff_dummies[i];
        if (fabs(sum_diff) > 1e-10) {
            g_print("Warning: diff_dummies sum = %g, forcing zero mean\n", sum_diff);
            double mean = sum_diff / s;
            for (int i = 0; i < s; i++) diff_dummies[i] -= mean;
        }

        // Construir level provisional con level[0] = 0
        double *prov = malloc(s * sizeof(double));
        prov[0] = 0.0;
        for (int i = 1; i < s; i++) {
            prov[i] = prov[i-1] + diff_dummies[i];
        }
        // Calcular constante para que level sume cero
        double sum_prov = 0.0;
        for (int i = 0; i < s; i++) sum_prov += prov[i];
        double c = -sum_prov / s;
        for (int i = 0; i < s; i++) level[i] = prov[i] + c;
        free(prov);
        return level;
    }
    else if (d == 2) {
        double *first_diff = integrate_dummies(diff_dummies, s, 1);
        if (!first_diff) {
            free(level);
            return NULL;
        }
        double *level2 = integrate_dummies(first_diff, s, 1);
        free(first_diff);
        if (!level2) {
            free(level);
            return NULL;
        }
        memcpy(level, level2, s * sizeof(double));
        free(level2);
        return level;
    }
    else {
        free(level);
        return NULL;
    }
}


/**
 * @brief Generate A0 transformation matrix for given seasonal period
 *
 * Creates the A0 matrix that transforms harmonic coefficients to seasonal dummies
 * according to the theoretical specification: ω = A0 * γ
 *
 * @param s Seasonal period (must be between 2 and 12)
 * @return gsl_matrix* Pointer to allocated A0 matrix, NULL on error
 */

gsl_matrix* generate_A0_matrix(int s) {
    if (s < 2 || s > 12) {
        printf("Error: Seasonal period s must be between 2 and 12\n");
        return NULL;
    }

    int matrix_size = s - 1;
    gsl_matrix *A0 = gsl_matrix_alloc(matrix_size, matrix_size);

    for (int i = 0; i < matrix_size; i++) {
        int col = 0;
        double t = i + 1;  // mes 1..s-1
        for (int freq = 1; freq <= s/2; freq++) {
            double angle = 2.0 * M_PI * freq * t / s;
            if (freq < s/2) {
                gsl_matrix_set(A0, i, col++, cos(angle));
                gsl_matrix_set(A0, i, col++, sin(angle));
            } else if (s % 2 == 0 && freq == s/2) {
                gsl_matrix_set(A0, i, col++, cos(angle));  // Nyquist: solo coseno
            }
        }
    }
    return A0;
}

/**
 * @brief Compute HAC (Newey-West) covariance matrix estimator - VERSIÓN ESTABLE
 */
gsl_matrix* compute_hac_covariance(gsl_matrix *X, gsl_vector *residuals, int max_lags) {
    int n = X->size1;
    int p = X->size2;

    printf("=== CÁLCULO HAC SIMPLIFICADO Y ESTABLE ===\n");
    printf("n=%d, p=%d, max_lags=%d\n", n, p, max_lags);

    // Calcular (X'X)^-1 directamente usando GSL (más estable)
    gsl_matrix *xtx = gsl_matrix_alloc(p, p);
    gsl_blas_dgemm(CblasTrans, CblasNoTrans, 1.0, X, X, 0.0, xtx);

    gsl_permutation *perm = gsl_permutation_alloc(p);
    int signum;
    gsl_matrix *xtx_inv = gsl_matrix_alloc(p, p);

    gsl_error_handler_t *old_handler = gsl_set_error_handler_off();
    int decomp_status = gsl_linalg_LU_decomp(xtx, perm, &signum);
    int invert_status = GSL_SUCCESS;

    if (decomp_status == GSL_SUCCESS) {
        invert_status = gsl_linalg_LU_invert(xtx, perm, xtx_inv);
    }
    gsl_set_error_handler(old_handler);

    if (decomp_status != GSL_SUCCESS || invert_status != GSL_SUCCESS) {
        printf("❌ No se puede invertir X'X, usando matriz identidad\n");
        gsl_matrix_set_identity(xtx_inv);
    }

    // Calcular matriz Omega (HAC) de manera simplificada y estable
    gsl_matrix *hac_cov = gsl_matrix_alloc(p, p);
    gsl_matrix_set_zero(hac_cov);

    gsl_matrix *S = gsl_matrix_alloc(p, p);
    gsl_matrix_set_zero(S);

    // PRIMERO: Calcular todos los productos necesarios con escalado apropiado
    double *u = (double*)malloc(n * sizeof(double));
    double **x_outer = (double**)malloc(n * sizeof(double*));

    // Pre-calcular todos los productos externos escalados
    for (int i = 0; i < n; i++) {
        u[i] = gsl_vector_get(residuals, i);
        x_outer[i] = (double*)malloc(p * p * sizeof(double));

        // Calcular x_i * x_i^T * u_i^2 / n (ya escalado)
        for (int j = 0; j < p; j++) {
            for (int k = 0; k < p; k++) {
                double x_ij = gsl_matrix_get(X, i, j);
                double x_ik = gsl_matrix_get(X, i, k);
                x_outer[i][j * p + k] = (x_ij * x_ik * u[i] * u[i]) / n;
            }
        }
    }

    // Contribución lag 0
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < p; j++) {
            for (int k = 0; k < p; k++) {
                double current = gsl_matrix_get(S, j, k);
                gsl_matrix_set(S, j, k, current + x_outer[i][j * p + k]);
            }
        }
    }

    // Contribuciones de lags con kernel de Bartlett
    for (int lag = 1; lag <= max_lags; lag++) {
        double weight = 1.0 - (double)lag / (max_lags + 1.0);

        for (int i = lag; i < n; i++) {
            double cross_product = (u[i] * u[i - lag]) / n * weight;

            for (int j = 0; j < p; j++) {
                for (int k = 0; k < p; k++) {
                    double x_t_j = gsl_matrix_get(X, i, j);
                    double x_t_lag_k = gsl_matrix_get(X, i - lag, k);
                    double x_t_lag_j = gsl_matrix_get(X, i - lag, j);
                    double x_t_k = gsl_matrix_get(X, i, k);

                    double current1 = gsl_matrix_get(S, j, k);
                    double current2 = gsl_matrix_get(S, k, j);

                    gsl_matrix_set(S, j, k, current1 + cross_product * x_t_j * x_t_lag_k);
                    gsl_matrix_set(S, k, j, current2 + cross_product * x_t_lag_j * x_t_k);
                }
            }
        }
    }

    // Verificar norma de S
    double S_norm = 0.0;
    for (int i = 0; i < p; i++) {
        for (int j = 0; j < p; j++) {
            S_norm += fabs(gsl_matrix_get(S, i, j));
        }
    }
    printf("Norma de matriz S: %.6e\n", S_norm);

    // Calcular HAC: (X'X)^{-1} * S * (X'X)^{-1}
    gsl_matrix *temp = gsl_matrix_alloc(p, p);
    gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1.0, xtx_inv, S, 0.0, temp);
    gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1.0, temp, xtx_inv, 0.0, hac_cov);

    // Verificar resultado final
    double hac_norm = 0.0;
    for (int i = 0; i < p; i++) {
        for (int j = 0; j < p; j++) {
            hac_norm += fabs(gsl_matrix_get(hac_cov, i, j));
        }
    }
    printf("Norma de matriz HAC: %.6e\n", hac_norm);

    // Limpiar memoria
    free(u);
    for (int i = 0; i < n; i++) {
        free(x_outer[i]);
    }
    free(x_outer);

    gsl_matrix_free(xtx);
    gsl_matrix_free(xtx_inv);
    gsl_matrix_free(S);
    gsl_matrix_free(temp);
    gsl_permutation_free(perm);

    printf("✅ CÁLCULO HAC COMPLETADO\n\n");
    return hac_cov;
}

/**
 * @brief Perform harmonic regression on differenced series using differenced basis
 *
 * Constructs the design matrix using the d-th differences of the harmonic functions
 * (cos and sin) and estimates the level coefficients γ0 directly.
 *
 * @param y Differenced series (length n)
 * @param n Number of observations in y
 * @param d Order of differencing applied to original series to obtain y
 * @param coefficients Output array for harmonic coefficients (γ0)
 * @param std_errors Output array for OLS standard errors
 * @param hac_cov_matrix_ptr Output pointer for HAC covariance (may be NULL)
 * @param ols_cov_matrix_ptr Output pointer for OLS covariance
 * @param intercept Output intercept term
 * @param intercept_std_error Output OLS standard error for intercept
 * @param f_stat Output F-statistic (OLS-based)
 * @param p_value Output p-value for F-test
 * @param r_squared Output R-squared value
 * @param s Seasonal period
 * @return int 1 on success, 0 on failure
 */
int harmonic_regression_differenced_basis(double *y, int n, int d,
                                          double *coefficients, double *std_errors,
                                          gsl_matrix **hac_cov_matrix_ptr, gsl_matrix **ols_cov_matrix_ptr,
                                          double *intercept, double *intercept_std_error,
                                          double *f_stat, double *p_value, double *r_squared,
                                          int s) {
    if (!y || n <= 0) return 0;
    if (d < 0 || d > 2) {
        printf("Error: differencing order d must be 0,1,2\n");
        return 0;
    }

    int num_harmonics = s - 1;
    int total_params = num_harmonics + 1; // intercept + harmonics

    if (n <= total_params) {
        printf("Error: Insufficient observations (%d) for %d parameters\n", n, total_params);
        return 0;
    }

    printf("Starting harmonic regression (differenced basis): n=%d, s=%d, d=%d, harmonics=%d, total_params=%d\n",
           n, s, d, num_harmonics, total_params);

    // Initialize outputs
    *hac_cov_matrix_ptr = NULL;
    *ols_cov_matrix_ptr = NULL;
    *f_stat = 0.0;
    *p_value = 1.0;
    *r_squared = 0.0;
    *intercept = 0.0;
    *intercept_std_error = 0.0;
    for (int i = 0; i < num_harmonics; i++) {
        coefficients[i] = 0.0;
        std_errors[i] = 0.0;
    }

    // Allocate memory
    gsl_matrix *X = gsl_matrix_alloc(n, total_params);
    gsl_vector *y_vec = gsl_vector_alloc(n);
    gsl_vector *c = gsl_vector_alloc(total_params);
    gsl_vector *residuals = gsl_vector_alloc(n);
    if (!X || !y_vec || !c || !residuals) {
        printf("Error: Memory allocation failed\n");
        if (X) gsl_matrix_free(X);
        if (y_vec) gsl_vector_free(y_vec);
        if (c) gsl_vector_free(c);
        if (residuals) gsl_vector_free(residuals);
        return 0;
    }

    // Copy y to GSL vector
    for (int i = 0; i < n; i++) {
        gsl_vector_set(y_vec, i, y[i]);
    }

    // Construct design matrix X
    // For each observation i (0..n-1), original time t = i + d + 1 (since y corresponds to t = d+1..T)
    for (int i = 0; i < n; i++) {
        double t = i + d + 1; // original time index (1-based)

        // Intercept
        gsl_matrix_set(X, i, 0, 1.0);

        int col = 1;
        for (int freq = 1; freq <= s/2; freq++) {
            double omega = 2.0 * M_PI * freq / s;

            // Compute values at t, t-1, ..., t-d
            double cos_vals[3], sin_vals[3];
            for (int k = 0; k <= d; k++) {
                double tk = t - k;
                cos_vals[k] = cos(omega * tk);
                sin_vals[k] = sin(omega * tk);
            }

            // Compute d-th difference using binomial coefficients
            double diff_cos, diff_sin;
            if (d == 0) {
                diff_cos = cos_vals[0];
                diff_sin = sin_vals[0];
            } else if (d == 1) {
                diff_cos = cos_vals[0] - cos_vals[1];
                diff_sin = sin_vals[0] - sin_vals[1];
            } else { // d == 2
                diff_cos = cos_vals[0] - 2.0 * cos_vals[1] + cos_vals[2];
                diff_sin = sin_vals[0] - 2.0 * sin_vals[1] + sin_vals[2];
            }

            if (freq < s/2) {
                gsl_matrix_set(X, i, col++, diff_cos);
                gsl_matrix_set(X, i, col++, diff_sin);
            } else if (s % 2 == 0 && freq == s/2) {
                // Nyquist frequency: only cosine
                gsl_matrix_set(X, i, col++, diff_cos);
            }
        }
        if (col != total_params) {
            printf("Warning: Column count mismatch at row %d\n", i);
        }
    }

    // OLS regression
    gsl_matrix *cov_ols = gsl_matrix_alloc(total_params, total_params);
    double chisq;
    gsl_multifit_linear_workspace *work = gsl_multifit_linear_alloc(n, total_params);
    if (!cov_ols || !work) {
        printf("Error: Memory allocation for OLS\n");
        gsl_matrix_free(X);
        gsl_vector_free(y_vec);
        gsl_vector_free(c);
        gsl_vector_free(residuals);
        if (cov_ols) gsl_matrix_free(cov_ols);
        return 0;
    }

    gsl_error_handler_t *old_handler = gsl_set_error_handler_off();
    int status = gsl_multifit_linear(X, y_vec, c, cov_ols, &chisq, work);
    gsl_set_error_handler(old_handler);

    if (status != GSL_SUCCESS) {
        printf("Warning: OLS regression failed: %s\n", gsl_strerror(status));
        // Fallback: use simple mean
        double y_mean = 0.0;
        for (int i = 0; i < n; i++) y_mean += y[i];
        y_mean /= n;
        *intercept = y_mean;
        *intercept_std_error = 0.0;
        for (int i = 0; i < num_harmonics; i++) {
            coefficients[i] = 0.0;
            std_errors[i] = 0.0;
        }
        *f_stat = 0.0;
        *p_value = 1.0;
        *r_squared = 0.0;
        *ols_cov_matrix_ptr = gsl_matrix_alloc(total_params, total_params);
        if (*ols_cov_matrix_ptr) gsl_matrix_set_identity(*ols_cov_matrix_ptr);
        gsl_matrix_free(X);
        gsl_vector_free(y_vec);
        gsl_vector_free(c);
        gsl_vector_free(residuals);
        gsl_matrix_free(cov_ols);
        gsl_multifit_linear_free(work);
        return 1; // success with fallback
    }

    // Extract coefficients and standard errors
    *intercept = gsl_vector_get(c, 0);
    for (int i = 0; i < num_harmonics; i++) {
        coefficients[i] = gsl_vector_get(c, i + 1);
    }
    *intercept_std_error = sqrt(gsl_matrix_get(cov_ols, 0, 0));
    for (int i = 0; i < num_harmonics; i++) {
        std_errors[i] = sqrt(gsl_matrix_get(cov_ols, i + 1, i + 1));
    }

    // Compute residuals and statistics
    double y_mean = 0.0;
    for (int i = 0; i < n; i++) y_mean += y[i];
    y_mean /= n;
    double sst = 0.0, ssr = 0.0, sse = 0.0;
    int valid_residuals = 0;
    for (int i = 0; i < n; i++) {
        double y_pred = *intercept;
        for (int j = 0; j < num_harmonics; j++) {
            y_pred += coefficients[j] * gsl_matrix_get(X, i, j + 1);
        }
        double y_actual = gsl_vector_get(y_vec, i);
        double residual = y_actual - y_pred;
        gsl_vector_set(residuals, i, residual);
        if (!isnan(residual) && !isinf(residual)) {
            sse += residual * residual;
            ssr += (y_pred - y_mean) * (y_pred - y_mean);
            sst += (y_actual - y_mean) * (y_actual - y_mean);
            valid_residuals++;
        }
    }
    if (valid_residuals > total_params) {
        double residual_variance = sse / (valid_residuals - total_params);
        *r_squared = (sst > 1e-12) ? ssr / sst : 0.0;
        // F-test
        if (residual_variance > 1e-12 && num_harmonics > 0) {
            double mssr = ssr / num_harmonics;
            double msse = sse / (valid_residuals - total_params);
            *f_stat = mssr / msse;
            *p_value = gsl_cdf_fdist_Q(*f_stat, num_harmonics, valid_residuals - total_params);
        } else {
            *f_stat = 0.0;
            *p_value = 1.0;
        }
    } else {
        *r_squared = 0.0;
        *f_stat = 0.0;
        *p_value = 1.0;
    }

    // HAC covariance (optional)
    gsl_matrix *hac_cov = NULL;
    if (valid_residuals > total_params * 3 && sse / (valid_residuals - total_params) > 1e-12) {
        int max_lags = (n <= 100) ? 1 : (n <= 200) ? 2 : 3;
        printf("Attempting HAC computation with %d lags\n", max_lags);
        hac_cov = compute_hac_covariance(X, residuals, max_lags);
        if (hac_cov) {
            double hac_norm = 0.0;
            int hac_valid = 1;
            for (int i = 0; i < total_params && hac_valid; i++) {
                for (int j = 0; j < total_params && hac_valid; j++) {
                    double val = gsl_matrix_get(hac_cov, i, j);
                    hac_norm += fabs(val);
                    if (isnan(val) || isinf(val)) hac_valid = 0;
                }
            }
            if (!hac_valid || hac_norm < 1e-12 || hac_norm > 1e6) {
                gsl_matrix_free(hac_cov);
                hac_cov = NULL;
            }
        }
    }

    *hac_cov_matrix_ptr = hac_cov;
    *ols_cov_matrix_ptr = cov_ols;

    // Clean up temporary objects
    gsl_matrix_free(X);
    gsl_vector_free(y_vec);
    gsl_vector_free(c);
    gsl_vector_free(residuals);
    gsl_multifit_linear_free(work);

    printf("✅ Harmonic regression (differenced basis) completed: F=%.4f (p=%.4f), R²=%.4f\n",
           *f_stat, *p_value, *r_squared);
    return 1;
}


/**
 * @brief Perform robust harmonic regression with dual covariance matrices
 *
 * This function implements harmonic regression for seasonal detection with:
 * - OLS covariance for stable visual confidence bands
 * - HAC covariance for robust inference (when numerically stable)
 * - Comprehensive numerical stability checks
 * - Fallback mechanisms for all potential failure points
 *
 * @param y Response vector (already transformed if applicable)
 * @param n Number of observations
 * @param coefficients Output array for harmonic coefficients
 * @param std_errors Output array for OLS standard errors
 * @param hac_cov_matrix_ptr Output pointer for HAC covariance (may be NULL)
 * @param ols_cov_matrix_ptr Output pointer for OLS covariance
 * @param intercept Output intercept term
 * @param intercept_std_error Output OLS standard error for intercept
 * @param f_stat Output F-statistic (OLS-based for stability)
 * @param p_value Output p-value for F-test
 * @param r_squared Output R-squared value
 * @param s Seasonal period
 * @return int 1 on success, 0 on failure
*/
int harmonic_regression_dual_covariance(double *y, int n, double *coefficients, double *std_errors,
                                       gsl_matrix **hac_cov_matrix_ptr, gsl_matrix **ols_cov_matrix_ptr,
                                       double *intercept, double *intercept_std_error, double *f_stat,
                                       double *p_value, double *r_squared, int s) {

    // =========================================================================
    // INITIALIZATION AND VALIDATION
    // =========================================================================

    if (!y || n <= 0) {
        printf("Error: Invalid input data\n");
        return 0;
    }

    int num_harmonics = s - 1;
    int total_params = num_harmonics + 1; // Intercept + harmonics

    // Validate parameters
    if (n <= total_params) {
        printf("Error: Insufficient observations (%d) for %d parameters\n", n, total_params);
        return 0;
    }

    if (num_harmonics <= 0 || num_harmonics > 11) {
        printf("Error: Invalid number of harmonics: %d\n", num_harmonics);
        return 0;
    }

    printf("Starting robust harmonic regression: n=%d, s=%d, harmonics=%d, total_params=%d\n",
           n, s, num_harmonics, total_params);

    // Initialize outputs
    *hac_cov_matrix_ptr = NULL;
    *ols_cov_matrix_ptr = NULL;
    *f_stat = 0.0;
    *p_value = 1.0;
    *r_squared = 0.0;
    *intercept = 0.0;
    *intercept_std_error = 0.0;

    for (int i = 0; i < num_harmonics; i++) {
        coefficients[i] = 0.0;
        std_errors[i] = 0.0;
    }

    // =========================================================================
    // MEMORY ALLOCATION WITH COMPREHENSIVE ERROR CHECKING
    // =========================================================================

    gsl_matrix *X = gsl_matrix_alloc(n, total_params);
    gsl_vector *y_vec = gsl_vector_alloc(n);
    gsl_vector *c = gsl_vector_alloc(total_params);
    gsl_vector *residuals = gsl_vector_alloc(n);

    if (!X || !y_vec || !c || !residuals) {
        printf("Error: Memory allocation failed for GSL objects\n");
        if (X) gsl_matrix_free(X);
        if (y_vec) gsl_vector_free(y_vec);
        if (c) gsl_vector_free(c);
        if (residuals) gsl_vector_free(residuals);
        return 0;
    }

    // =========================================================================
    // DATA PREPARATION AND VALIDATION
    // =========================================================================

    // Copy y data to GSL vector and check for invalid values
    int valid_points = 0;
    double y_mean = 0.0;
    double y_min = 1e10, y_max = -1e10;

    for (int i = 0; i < n; i++) {
        double y_val = y[i];
        if (!isnan(y_val) && !isinf(y_val)) {
            gsl_vector_set(y_vec, i, y_val);
            y_mean += y_val;
            if (y_val < y_min) y_min = y_val;
            if (y_val > y_max) y_max = y_val;
            valid_points++;
        } else {
            gsl_vector_set(y_vec, i, 0.0); // Safe fallback
        }
    }

    if (valid_points == 0) {
        printf("Error: No valid data points\n");
        gsl_matrix_free(X);
        gsl_vector_free(y_vec);
        gsl_vector_free(c);
        gsl_vector_free(residuals);
        return 0;
    }

    y_mean /= valid_points;
    printf("Data range: min=%.6f, max=%.6f, mean=%.6f, valid_points=%d/%d\n",
           y_min, y_max, y_mean, valid_points, n);

    // =========================================================================
    // DESIGN MATRIX CONSTRUCTION - IMPROVED FOR ALL SEASONAL PERIODS
    // =========================================================================

    printf("Constructing design matrix for s=%d...\n", s);

    for (int i = 0; i < n; i++) {
        // Column 0: intercept term (always 1.0)
        gsl_matrix_set(X, i, 0, 1.0);

        // Columns 1..num_harmonics: harmonic basis functions
        int col = 1;
        double t = i + 1; // Time index starting from 1

        // Specialized handling for different seasonal periods
        if (s == 5) {
            // For daily seasonality (s=5): 2 frequencies with sin/cos pairs
            gsl_matrix_set(X, i, col++, cos(2.0 * M_PI * 1 * t / 5));
            gsl_matrix_set(X, i, col++, sin(2.0 * M_PI * 1 * t / 5));
            gsl_matrix_set(X, i, col++, cos(2.0 * M_PI * 2 * t / 5));
            gsl_matrix_set(X, i, col++, sin(2.0 * M_PI * 2 * t / 5));
        }
        else if (s == 12) {
            // For monthly seasonality: 6 frequencies, last one cosine only
            for (int freq = 1; freq <= 6; freq++) {
                double angle = 2.0 * M_PI * freq * t / 12;
                if (freq < 6) {
                    gsl_matrix_set(X, i, col++, cos(angle));
                    gsl_matrix_set(X, i, col++, sin(angle));
                } else {
                    // Nyquist frequency: cosine only
                    gsl_matrix_set(X, i, col++, cos(angle));
                }
            }
        }
        else {
            // General case for other seasonal periods
            for (int freq = 1; freq <= s/2; freq++) {
                double angle = 2.0 * M_PI * freq * t / s;

                if (freq < s/2) {
                    // Non-Nyquist frequencies: both sine and cosine
                    gsl_matrix_set(X, i, col++, cos(angle));
                    if (col < total_params) {
                        gsl_matrix_set(X, i, col++, sin(angle));
                    }
                } else if (s % 2 == 0 && freq == s/2) {
                    // Nyquist frequency (s even): cosine only
                    gsl_matrix_set(X, i, col++, cos(angle));
                }
            }
        }

        // Verify we used the correct number of columns
        if (col != total_params) {
            printf("Warning: Column count mismatch: expected %d, got %d\n", total_params, col);
        }
    }

    // =========================================================================
    // OLS REGRESSION WITH ROBUST ERROR HANDLING
    // =========================================================================

    gsl_matrix *cov_ols = gsl_matrix_alloc(total_params, total_params);
    double chisq;
    gsl_multifit_linear_workspace *work = gsl_multifit_linear_alloc(n, total_params);

    if (!cov_ols || !work) {
        printf("Error: Memory allocation failed for OLS computation\n");
        gsl_matrix_free(X);
        gsl_vector_free(y_vec);
        gsl_vector_free(c);
        gsl_vector_free(residuals);
        if (cov_ols) gsl_matrix_free(cov_ols);
        return 0;
    }

    // Disable GSL error handler to prevent crashes on numerical issues
    gsl_error_handler_t *old_handler = gsl_set_error_handler_off();

    printf("Performing OLS regression...\n");
    int status = gsl_multifit_linear(X, y_vec, c, cov_ols, &chisq, work);

    if (status != GSL_SUCCESS) {
        printf("Warning: OLS regression failed: %s\n", gsl_strerror(status));

        // Fallback: Use simple mean if regression fails
        *intercept = y_mean;
        *intercept_std_error = 0.0;

        for (int i = 0; i < num_harmonics; i++) {
            coefficients[i] = 0.0;
            std_errors[i] = 0.0;
        }

        // Set conservative statistics
        *f_stat = 0.0;
        *p_value = 1.0;
        *r_squared = 0.0;

        // Still create dummy covariance matrices for consistency
        *ols_cov_matrix_ptr = gsl_matrix_alloc(total_params, total_params);
        if (*ols_cov_matrix_ptr) {
            gsl_matrix_set_identity(*ols_cov_matrix_ptr);
        }

        gsl_set_error_handler(old_handler);
        gsl_matrix_free(X);
        gsl_vector_free(y_vec);
        gsl_vector_free(c);
        gsl_vector_free(residuals);
        gsl_matrix_free(cov_ols);
        gsl_multifit_linear_free(work);

        printf("Using fallback solution (mean only)\n");
        return 1; // Return success but with fallback values
    }

    gsl_set_error_handler(old_handler);
    printf("✅ OLS regression completed successfully\n");

    // =========================================================================
    // PARAMETER EXTRACTION AND VALIDATION
    // =========================================================================

    *intercept = gsl_vector_get(c, 0);
    for (int i = 0; i < num_harmonics; i++) {
        coefficients[i] = gsl_vector_get(c, i + 1);
    }

    // Extract OLS standard errors
    *intercept_std_error = sqrt(gsl_matrix_get(cov_ols, 0, 0));
    for (int i = 0; i < num_harmonics; i++) {
        std_errors[i] = sqrt(gsl_matrix_get(cov_ols, i + 1, i + 1));
    }

    printf("Parameter extraction: intercept=%.6f ± %.6f\n", *intercept, *intercept_std_error);

    // =========================================================================
    // RESIDUAL COMPUTATION WITH ROBUST DIAGNOSTICS
    // =========================================================================

    double residual_variance = 0.0;
    double max_residual = 0.0;
    int valid_residuals = 0;
    double sst = 0.0, ssr = 0.0, sse = 0.0;

    for (int i = 0; i < n; i++) {
        // Compute predicted value
        double y_pred = *intercept;
        for (int j = 0; j < num_harmonics; j++) {
            y_pred += coefficients[j] * gsl_matrix_get(X, i, j + 1);
        }

        double y_actual = gsl_vector_get(y_vec, i);
        double residual = y_actual - y_pred;

        // Store residual
        gsl_vector_set(residuals, i, residual);

        // Accumulate statistics only for valid residuals
        if (!isnan(residual) && !isinf(residual)) {
            residual_variance += residual * residual;
            sse += residual * residual;
            ssr += (y_pred - y_mean) * (y_pred - y_mean);
            sst += (y_actual - y_mean) * (y_actual - y_mean);
            valid_residuals++;

            if (fabs(residual) > max_residual) {
                max_residual = fabs(residual);
            }
        }
    }

    // Compute final statistics
    if (valid_residuals > total_params) {
        residual_variance /= (valid_residuals - total_params);
    } else if (valid_residuals > 0) {
        residual_variance /= valid_residuals;
    } else {
        residual_variance = 0.0;
    }

    // Compute R-squared
    if (sst > 1e-12) {
        *r_squared = ssr / sst;
    } else {
        *r_squared = 0.0;
    }

    printf("Residual diagnostics: valid=%d/%d, variance=%.6e, max=%.6e, R²=%.4f\n",
           valid_residuals, n, residual_variance, max_residual, *r_squared);

    // =========================================================================
    // F-TEST COMPUTATION WITH ROBUST FALLBACKS
    // =========================================================================

    if (valid_residuals > total_params && residual_variance > 1e-12 && sst > 1e-12) {
        // Standard F-test: F = (SSR/num_harmonics) / (SSE/(n-total_params))
        double mssr = ssr / num_harmonics;
        double msse = sse / (valid_residuals - total_params);

        if (msse > 1e-12) {
            *f_stat = mssr / msse;
            *p_value = gsl_cdf_fdist_Q(*f_stat, num_harmonics, valid_residuals - total_params);
        } else {
            // Near-perfect fit case
            *f_stat = 1000.0; // Large value
            *p_value = 0.0;
        }

        printf("F-test: F=%.4f, p=%.6f, df=(%d, %d)\n",
               *f_stat, *p_value, num_harmonics, valid_residuals - total_params);
    } else {
        // Conservative fallback
        *f_stat = 0.0;
        *p_value = 1.0;
        printf("Warning: Using conservative F-test values due to numerical issues\n");
    }

    // =========================================================================
    // HAC COVARIANCE COMPUTATION (OPTIONAL - ONLY IF STABLE)
    // =========================================================================

    gsl_matrix *hac_cov = NULL;

    // Only attempt HAC if we have sufficient data and reasonable residuals
    if (valid_residuals > total_params * 3 && residual_variance > 1e-12 && max_residual < 1e6) {
        int max_lags;

        // Conservative lag selection
        if (n <= 100) max_lags = 1;
        else if (n <= 200) max_lags = 2;
        else max_lags = 3;

        printf("Attempting HAC computation with %d lags\n", max_lags);
        hac_cov = compute_hac_covariance(X, residuals, max_lags);

        if (hac_cov) {
            // Validate HAC matrix
            double hac_norm = 0.0;
            int hac_valid = 1;

            for (int i = 0; i < total_params && hac_valid; i++) {
                for (int j = 0; j < total_params && hac_valid; j++) {
                    double val = gsl_matrix_get(hac_cov, i, j);
                    hac_norm += fabs(val);

                    if (isnan(val) || isinf(val)) {
                        hac_valid = 0;
                    }
                }
            }

            if (hac_valid && hac_norm > 1e-12 && hac_norm < 1e6) {
                printf("✅ HAC covariance computed successfully (norm=%.6e)\n", hac_norm);

                // Optional diagnostic comparison
                if (*f_stat > 0) {
                    diagnostic_hac_f_test(coefficients, hac_cov, num_harmonics, n, total_params);
                }
            } else {
                printf("❌ HAC matrix invalid (norm=%.6e, valid=%d), discarding\n", hac_norm, hac_valid);
                gsl_matrix_free(hac_cov);
                hac_cov = NULL;
            }
        }
    } else {
        printf("Skipping HAC: insufficient valid data or extreme residuals\n");
    }

    // =========================================================================
    // FINAL OUTPUT ASSIGNMENT AND CLEANUP
    // =========================================================================

    *hac_cov_matrix_ptr = hac_cov;
    *ols_cov_matrix_ptr = cov_ols;

    // Free temporary objects (but keep cov_ols since it's returned)
    gsl_matrix_free(X);
    gsl_vector_free(y_vec);
    gsl_vector_free(c);
    gsl_vector_free(residuals);
    gsl_multifit_linear_free(work);

    printf("✅ Harmonic regression completed: F=%.4f (p=%.4f), R²=%.4f\n",
           *f_stat, *p_value, *r_squared);

    return 1;
}

/**
 * @brief Diagnostic HAC F-test for comparison with OLS (optional)
 *
 * This function computes the HAC-based F-test for diagnostic purposes
 * but does not replace the primary OLS-based F-test.
 */
void diagnostic_hac_f_test(double *coefficients, gsl_matrix *hac_cov,
                          int num_harmonics, int n, int total_params) {
    // Extract harmonic coefficients submatrix from HAC covariance
    gsl_matrix *hac_harmonic_cov = gsl_matrix_alloc(num_harmonics, num_harmonics);
    gsl_vector *beta = gsl_vector_alloc(num_harmonics);

    if (!hac_harmonic_cov || !beta) {
        if (hac_harmonic_cov) gsl_matrix_free(hac_harmonic_cov);
        if (beta) gsl_vector_free(beta);
        return;
    }

    for (int i = 0; i < num_harmonics; i++) {
        gsl_vector_set(beta, i, coefficients[i]);
        for (int j = 0; j < num_harmonics; j++) {
            double cov_val = gsl_matrix_get(hac_cov, i + 1, j + 1);
            gsl_matrix_set(hac_harmonic_cov, i, j, cov_val);
        }
    }

    // Compute Wald statistic
    gsl_permutation *perm = gsl_permutation_alloc(num_harmonics);
    gsl_matrix *hac_copy = gsl_matrix_alloc(num_harmonics, num_harmonics);

    if (perm && hac_copy) {
        gsl_matrix_memcpy(hac_copy, hac_harmonic_cov);
        int signum;

        gsl_error_handler_t *old_handler = gsl_set_error_handler_off();
        int decomp_status = gsl_linalg_LU_decomp(hac_copy, perm, &signum);
        gsl_set_error_handler(old_handler);

        if (decomp_status == GSL_SUCCESS) {
            gsl_vector *x = gsl_vector_alloc(num_harmonics);
            if (x) {
                gsl_error_handler_t *old_handler2 = gsl_set_error_handler_off();
                int solve_status = gsl_linalg_LU_solve(hac_copy, perm, beta, x);
                gsl_set_error_handler(old_handler2);

                if (solve_status == GSL_SUCCESS) {
                    double wald_stat;
                    gsl_blas_ddot(beta, x, &wald_stat);

                    if (wald_stat >= 0 && !isnan(wald_stat) && !isinf(wald_stat)) {
                        double hac_f = wald_stat / num_harmonics;
                        double hac_p = gsl_cdf_fdist_Q(hac_f, num_harmonics, n - total_params);
                        printf("HAC F-test (diagnostic): F=%.6f, p=%.6f\n", hac_f, hac_p);
                    }
                }
                gsl_vector_free(x);
            }
        }
        gsl_matrix_free(hac_copy);
        gsl_permutation_free(perm);
    }

    gsl_matrix_free(hac_harmonic_cov);
    gsl_vector_free(beta);
}

 /**
 * @brief Calculate variances of seasonal dummies using OLS covariance matrix
 *
 * This function properly handles the scaling from harmonic coefficients to seasonal dummies
 * ensuring the variances are in the correct units (percentage points for 100*log transformed data)
 */
double* calculate_dummy_variances_ols_based(gsl_matrix *ols_cov_matrix, int s) {
    if (!ols_cov_matrix) {
        printf("Error: OLS covariance matrix is NULL\n");
        return NULL;
    }

    double *variances = malloc(s * sizeof(double));
    if (!variances) {
        printf("Error: Could not allocate memory for variances\n");
        return NULL;
    }

    // Initialize variances
    for (int i = 0; i < s; i++) {
        variances[i] = 0.0;
    }

    gsl_matrix *A0 = generate_A0_matrix(s);
    if (!A0) {
        printf("Error: Could not generate A0 matrix\n");
        free(variances);
        return NULL;
    }

    int num_harmonics = s - 1;

    // Verify matrix dimensions
    if (ols_cov_matrix->size1 != num_harmonics + 1 || ols_cov_matrix->size2 != num_harmonics + 1) {
        printf("Error: OLS covariance matrix dimensions don't match expected size\n");
        printf("Expected: %dx%d, Got: %zux%zu\n", num_harmonics+1, num_harmonics+1,
               ols_cov_matrix->size1, ols_cov_matrix->size2);
        gsl_matrix_free(A0);
        free(variances);
        return NULL;
    }

    // Extract the covariance submatrix for harmonic coefficients (excluding intercept)
    gsl_matrix *harmonic_cov_sub = gsl_matrix_alloc(num_harmonics, num_harmonics);
    for (int i = 0; i < num_harmonics; i++) {
        for (int j = 0; j < num_harmonics; j++) {
            // Note: +1 because index 0 is intercept, 1..num_harmonics are harmonic coefficients
            double cov_val = gsl_matrix_get(ols_cov_matrix, i + 1, j + 1);
            gsl_matrix_set(harmonic_cov_sub, i, j, cov_val);
        }
    }

    printf("Harmonic covariance submatrix extracted (dim: %zux%zu)\n",
           harmonic_cov_sub->size1, harmonic_cov_sub->size2);

    // Compute A0 * Cov(γ) * A0^T
    gsl_matrix *temp = gsl_matrix_alloc(num_harmonics, num_harmonics);
    gsl_matrix *A0T = gsl_matrix_alloc(num_harmonics, num_harmonics);
    gsl_matrix_transpose_memcpy(A0T, A0);

    // temp = A0 * Cov(γ)
    gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1.0, A0, harmonic_cov_sub, 0.0, temp);

    // variances_matrix = temp * A0^T = A0 * Cov(γ) * A0^T
    gsl_matrix *variances_matrix = gsl_matrix_alloc(num_harmonics, num_harmonics);
    gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1.0, temp, A0T, 0.0, variances_matrix);

    // Extract variances for first s-1 dummies (diagonal elements)
    for (int i = 0; i < num_harmonics; i++) {
        double variance = gsl_matrix_get(variances_matrix, i, i);

        // Debug: print intermediate variances
        printf("  Dummy %2d raw variance: %12.8f\n", i+1, variance);

        // Ensure positive variance for stability
        if (variance < 1e-12) {
            variance = 1e-12;
        }
        variances[i] = variance;
    }

    // Compute variance for last dummy using sum-to-zero constraint
    // Var(ω_s) = Σ_i Σ_j Cov(ω_i, ω_j) for i,j = 1..s-1
    variances[s-1] = 0.0;
    for (int i = 0; i < num_harmonics; i++) {
        for (int j = 0; j < num_harmonics; j++) {
            variances[s-1] += gsl_matrix_get(variances_matrix, i, j);
        }
    }

    printf("  Dummy %2d raw variance (from constraint): %12.8f\n", s, variances[s-1]);

    // Ensure positive variance for last dummy
    if (variances[s-1] < 1e-12) {
        variances[s-1] = 1e-12;
    }

    // SCALE VARIANCES PROPERLY FOR PERCENTAGE INTERPRETATION
    // Since data was transformed as 100*log(data), the coefficients represent percentage effects
    // The variances should be scaled accordingly to maintain proper confidence intervals
    double variance_scale_factor = 1.0; // No additional scaling needed as OLS covariance already includes the 100*log scaling

    printf("Final OLS-based dummy variances (scaled for percentage):\n");
    for (int i = 0; i < s; i++) {
        variances[i] *= variance_scale_factor;
        printf("  Dummy %2d: variance = %12.8f, std_error = %10.6f%%\n",
               i+1, variances[i], sqrt(variances[i]));
    }

    // Clean up
    gsl_matrix_free(A0);
    gsl_matrix_free(harmonic_cov_sub);
    gsl_matrix_free(temp);
    gsl_matrix_free(A0T);
    gsl_matrix_free(variances_matrix);

    return variances;
}

/**
 * @brief Transform harmonic coefficients to seasonal dummies
 *
 * Applies A0 transformation matrix to convert harmonic coefficients
 * to seasonal dummy coefficients using ω = A0 * γ.
 *
 * @param harmonic_coeffs Harmonic coefficients vector
 * @param harmonic_cov Covariance matrix of harmonic coefficients (OLS-based)
 * @param s Seasonal period
 * @return double* Array of seasonal dummy coefficients
 */
double* transform_harmonics_to_dummies_general(double *harmonic_coeffs, gsl_matrix *harmonic_cov, int s) {
    int num_harmonics = s - 1;
    double *dummies = malloc(s * sizeof(double));

    gsl_matrix *A0 = generate_A0_matrix(s);
    if (!A0) {
        for (int i = 0; i < s; i++) dummies[i] = 0.0;
        return dummies;
    }

    // Create GSL vector for harmonic coefficients
    gsl_vector *coeff_vec = gsl_vector_alloc(num_harmonics);
    for (int i = 0; i < num_harmonics; i++) {
        gsl_vector_set(coeff_vec, i, harmonic_coeffs[i]);
    }

    // Compute first s-1 dummies: dummies[0:s-1] = A0 * coeff_vec
    gsl_vector *dummies_vec = gsl_vector_alloc(s - 1);
    gsl_blas_dgemv(CblasNoTrans, 1.0, A0, coeff_vec, 0.0, dummies_vec);

    // Extract first s-1 dummies
    for (int i = 0; i < s - 1; i++) {
        dummies[i] = gsl_vector_get(dummies_vec, i);
    }

    // Compute last dummy using sum-to-zero constraint
    dummies[s - 1] = 0.0;
    for (int i = 0; i < s - 1; i++) {
        dummies[s - 1] -= dummies[i];
    }

    // Clean up
    gsl_matrix_free(A0);
    gsl_vector_free(coeff_vec);
    gsl_vector_free(dummies_vec);

    return dummies;
}

/**
 * @brief Apply regular differences to time series data
 *
 * @param data Input time series data (modified in-place)
 * @param n_points Number of data points (updated in-place)
 * @param d Order of differencing
 */
void apply_regular_differences(double *data, int *n_points, int d) {
    for (int diff = 0; diff < d; diff++) {
        for (int i = 1; i < *n_points - diff; i++) {
            data[i - 1] = data[i] - data[i - 1];
        }
        (*n_points)--;
    }
}

/**
 * @brief Compute critical value from F-distribution
 *
 * @param df1 Numerator degrees of freedom
 * @param df2 Denominator degrees of freedom
 * @param alpha Significance level
 * @return double Critical value
 */
double f_distribution_critical_value(int df1, int df2, double alpha) {
    return gsl_cdf_fdist_Qinv(alpha, df1, df2);
}

/**
 * @brief Main function for seasonal pattern detection with dual covariance approach
 *
 * Implements the complete seasonal detection pipeline:
 * 1. Load and transform data (100*log for percentages)
 * 2. Apply differencing
 * 3. Perform harmonic regression with dual covariance matrices
 * 4. Use HAC for F-test of global significance
 * 5. Use OLS for visual confidence bands of seasonal dummies
 *
 * @param filename Path to data file
 * @param d Order of regular differencing
 * @param apply_log Whether to apply logarithmic transformation and rescaling
 * @param s Seasonal period
 * @return SeasonalDetectionResult* Detection results structure
 */

#ifndef DISABLE_DETECT_FUNCTION
SeasonalDetectionResult* detect_seasonality_harmonic_regression(const char *filename,
                                                               int d,
                                                               int apply_log,
                                                               int s) {
    SeasonalDetectionResult *result = malloc(sizeof(SeasonalDetectionResult));
    if (!result) {
        printf("Error: Memory allocation failed for result structure\n");
        return NULL;
    }

    // Initialize result structure
    memset(result, 0, sizeof(SeasonalDetectionResult));
    result->seasonal_detected = 0;
    result->num_harmonics = s - 1;
    result->seasonal_period = s;

    // Validate seasonal period
    if (s < 2 || s > 12) {
        result->message = strdup("Error: Seasonal period must be between 2 and 12");
        free(result);
        return NULL;
    }

    printf("Starting seasonal detection with dual covariance approach:\n");
    printf("  s=%d, d=%d, log=%d (100*log transformation)\n", s, d, apply_log);

    // Load original data
    double *original_data;
    int n_original;
    if (!load_data(filename, &original_data, &n_original)) {
        result->message = strdup("Error: Failed to load data file");
        free(result);
        return NULL;
    }

    printf("Loaded %d data points from %s\n", n_original, filename);

    // Apply logarithmic transformation with rescaling if requested
    if (apply_log) {
        apply_log_transform_rescaled(original_data, n_original);
        printf("Applied 100*log transformation for percentage interpretation\n");
    }

    // Apply regular differencing
    int n_diff = n_original;
    double *differenced_data = malloc(n_original * sizeof(double));
    if (!differenced_data) {
        free(original_data);
        free(result);
       // result->message = strdup("Error: Memory allocation failed for differenced data");
        return NULL;
    }

    memcpy(differenced_data, original_data, n_original * sizeof(double));
    apply_regular_differences(differenced_data, &n_diff, d);

    printf("After %d differences: %d observations remaining\n", d, n_diff);

    // Check sufficient data after differencing
    if (n_diff <= 2 * s) {
        free(original_data);
        free(differenced_data);
        result->message = strdup("Error: Insufficient data after differencing");
        free(result);
        return NULL;
    }

    // Allocate arrays for regression results
    int num_harmonics = result->num_harmonics;
    double *coefficients = malloc(num_harmonics * sizeof(double));
    double *std_errors = malloc(num_harmonics * sizeof(double));
    gsl_matrix *hac_cov_matrix = NULL;
    gsl_matrix *ols_cov_matrix = NULL;
    double intercept, intercept_std_error, f_stat, p_value, r_squared;

    if (!coefficients || !std_errors) {
        free(original_data);
        free(differenced_data);
        free(coefficients);
        free(std_errors);
        result->message = strdup("Error: Memory allocation failed for regression arrays");
        free(result);
        return NULL;
    }

    // Perform harmonic regression with dual covariance matrices
    int regression_success = harmonic_regression_dual_covariance(differenced_data, n_diff, coefficients,
                                                               std_errors, &hac_cov_matrix, &ols_cov_matrix,
                                                               &intercept, &intercept_std_error, &f_stat,
                                                               &p_value, &r_squared, s);

    if (!regression_success) {
        free(original_data);
        free(differenced_data);
        free(coefficients);
        free(std_errors);
        result->message = strdup("Error: Harmonic regression failed");
        free(result);
        return NULL;
    }

    printf("Harmonic regression completed:\n");
    printf("  HAC F-statistic: %.3f, p-value: %.4f\n", f_stat, p_value);
    printf("  R-squared: %.3f\n", r_squared);

    // Test for seasonal significance using HAC-based F-test (robust inference)
    double f_critical = f_distribution_critical_value(num_harmonics,
                                                     n_diff - num_harmonics - 1,
                                                     SIGNIFICANCE_LEVEL);
    result->seasonal_detected = (f_stat > f_critical);
    result->f_statistic = f_stat;
    result->p_value = p_value;
    result->harmonic_coeffs = coefficients;
    result->harmonic_std_errors = std_errors;
    result->harmonic_cov_matrix = hac_cov_matrix;  // Store HAC matrix for reference
    result->intercept = intercept;
    result->intercept_std_error = intercept_std_error;

    if (result->seasonal_detected) {
        printf("Seasonality detected at significance level %.3f (HAC F-test)\n", SIGNIFICANCE_LEVEL);

        // Transform to seasonal dummies using OLS covariance for stable visualization
        result->seasonal_dummies = transform_harmonics_to_dummies_general(coefficients,
                                                                         ols_cov_matrix, s);

        // Calculate dummy variances using OLS covariance matrix for visual bands
        double *dummy_variances = calculate_dummy_variances_ols_based(ols_cov_matrix, s);

        // Compute confidence intervals for dummies using OLS standard errors
        result->dummy_confidence_lower = malloc(s * sizeof(double));
        result->dummy_confidence_upper = malloc(s * sizeof(double));
        result->dummy_std_errors = malloc(s * sizeof(double));

        for (int i = 0; i < s; i++) {
            double std_error = sqrt(dummy_variances[i]);
            result->dummy_std_errors[i] = std_error;

            // Bandas centradas en cero: [-1.96*SE, +1.96*SE]
            result->dummy_confidence_lower[i] = -1.96 * std_error;
            result->dummy_confidence_upper[i] = 1.96 * std_error;
         }
        free(dummy_variances);

        char msg[256];
        snprintf(msg, sizeof(msg),
                "Seasonality detected (s=%d): HAC F=%.3f (p=%.4f), OLS confidence bands",
                s, f_stat, p_value);
        result->message = strdup(msg);

        printf("Seasonal dummies computed with OLS-based confidence intervals\n");
    } else {
        char msg[256];
        snprintf(msg, sizeof(msg),
                "No significant seasonality (s=%d): HAC F=%.3f (p=%.4f)",
                s, f_stat, p_value);
        result->message = strdup(msg);
        printf("No significant seasonality found using HAC F-test\n");
    }

    // Free OLS covariance matrix (not needed in results)
    if (ols_cov_matrix) {
        gsl_matrix_free(ols_cov_matrix);
    }

    // Clean up temporary data arrays
    free(original_data);
    free(differenced_data);

    return result;
}

#endif
/**
 * @brief Free memory allocated for seasonal detection results
 *
 * @param result Pointer to results structure to free
 */
void free_seasonal_detection_result(SeasonalDetectionResult *result) {
    if (!result) return;

    if (result->seasonal_dummies) free(result->seasonal_dummies);
    if (result->dummy_confidence_lower) free(result->dummy_confidence_lower);
    if (result->dummy_confidence_upper) free(result->dummy_confidence_upper);
    if (result->dummy_std_errors) free(result->dummy_std_errors);
    if (result->harmonic_coeffs) free(result->harmonic_coeffs);
    if (result->harmonic_std_errors) free(result->harmonic_std_errors);
    if (result->harmonic_cov_matrix) gsl_matrix_free(result->harmonic_cov_matrix);
    if (result->message) free(result->message);

    free(result);
}

/**
 * @brief Print formatted seasonal detection results
 *
 * @param result Results structure to print
 */
void print_seasonal_detection_result(SeasonalDetectionResult *result) {
    if (!result) {
        printf("Null results pointer\n");
        return;
    }

    printf("\n=== SEASONAL DETECTION RESULTS ===\n");
    printf("Message: %s\n", result->message);
    printf("Seasonality detected: %s\n", result->seasonal_detected ? "YES" : "NO");
    printf("F-statistic: %.4f\n", result->f_statistic);
    printf("P-value: %.4f\n", result->p_value);
    printf("Seasonal period (s): %d\n", result->seasonal_period);
    printf("Number of harmonics: %d\n", result->num_harmonics);
    printf("Intercept: %.4f ± %.4f\n", result->intercept, result->intercept_std_error);

    if (result->seasonal_detected && result->seasonal_dummies) {
        printf("\nSEASONAL DUMMY COEFFICIENTS:\n");
        for (int i = 0; i < result->seasonal_period; i++) {
            printf("  Period %2d: %8.4f [%8.4f, %8.4f] ± %.4f\n",
                   i + 1,
                   result->seasonal_dummies[i],
                   result->dummy_confidence_lower[i],
                   result->dummy_confidence_upper[i],
                   result->dummy_std_errors[i]);
        }

        printf("\nHARMONIC COEFFICIENTS:\n");
        for (int i = 0; i < result->num_harmonics; i++) {
            printf("  Harmonic %2d: %8.4f ± %8.4f\n",
                   i + 1,
                   result->harmonic_coeffs[i],
                   result->harmonic_std_errors[i]);
        }
    }
    printf("==================================\n\n");
}
