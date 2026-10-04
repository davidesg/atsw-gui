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

/**
 * @file seasonal_detection.c
 * @brief Implementación de detección estacional usando regresión armónica con base diferenciada
 *        (basado en la versión funcional de drvarma)
 */

#include "seasonal_detection.h"
#include "model_detection.h"   // para load_data, MAX_DATA_POINTS, etc.
#include <gsl/gsl_math.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_multifit.h>
#include <gsl/gsl_statistics.h>
#include <gsl/gsl_cdf.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_permutation.h>

/* ---------- Constantes ---------- */
#define SIGNIFICANCE_LEVEL 0.05

/* ---------- Funciones auxiliares ---------- */

void apply_log_transform_rescaled(double *data, int n_points) {
    for (int i = 0; i < n_points; i++) {
        if (data[i] > 0) {
            data[i] = 100.0 * log(data[i]);
        } else {
            printf("Warning: Non-positive value at position %d, using 1e-6\n", i);
            data[i] = 100.0 * log(1e-6);
        }
    }
}

void apply_regular_differences(double *data, int *n_points, int d) {
    for (int diff = 0; diff < d; diff++) {
        for (int i = 1; i < *n_points - diff; i++) {
            data[i - 1] = data[i] - data[i - 1];
        }
        (*n_points)--;
    }
}

/* ---------- Matriz A0 (transformación armónicos → dummies) ---------- */
gsl_matrix* generate_A0_matrix(int s) {
    if (s < 2 || s > 12) {
        printf("Error: Seasonal period s must be between 2 and 12\n");
        return NULL;
    }

    int matrix_size = s - 1;
    gsl_matrix *A0 = gsl_matrix_alloc(matrix_size, matrix_size);

    for (int i = 0; i < matrix_size; i++) {
        int col = 0;
        double t = i + 1;  // 1 .. s-1
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

/* ---------- Matriz de covarianza HAC (Newey-West) ---------- */
gsl_matrix* compute_hac_covariance(gsl_matrix *X, gsl_vector *residuals, int max_lags) {
    int n = X->size1;
    int p = X->size2;

    printf("=== HAC (Newey-West) ===\n");
    printf("n=%d, p=%d, max_lags=%d\n", n, p, max_lags);

    gsl_matrix *xtx = gsl_matrix_alloc(p, p);
    gsl_blas_dgemm(CblasTrans, CblasNoTrans, 1.0, X, X, 0.0, xtx);

    gsl_permutation *perm = gsl_permutation_alloc(p);
    int signum;
    gsl_matrix *xtx_inv = gsl_matrix_alloc(p, p);

    gsl_error_handler_t *old_handler = gsl_set_error_handler_off();
    int decomp_status = gsl_linalg_LU_decomp(xtx, perm, &signum);
    int invert_status = GSL_SUCCESS;
    if (decomp_status == GSL_SUCCESS)
        invert_status = gsl_linalg_LU_invert(xtx, perm, xtx_inv);
    gsl_set_error_handler(old_handler);

    if (decomp_status != GSL_SUCCESS || invert_status != GSL_SUCCESS) {
        printf("❌ No se puede invertir X'X, usando matriz identidad\n");
        gsl_matrix_set_identity(xtx_inv);
    }

    gsl_matrix *hac_cov = gsl_matrix_alloc(p, p);
    gsl_matrix_set_zero(hac_cov);
    gsl_matrix *S = gsl_matrix_alloc(p, p);
    gsl_matrix_set_zero(S);

    double *u = (double*)malloc(n * sizeof(double));
    double **x_outer = (double**)malloc(n * sizeof(double*));

    /* Newey-West meat as a SUM over t, as art-python (BUG-0206): dividing it
       by n made the covariance n times too small. And the lag term is
       w_l (Gamma_l + Gamma_l'), each product once: the old loop added
       x_t,j x_{t-l},k to both S[j][k] and S[k][j], counting Gamma_l twice and
       Gamma_l' never. */
    for (int i = 0; i < n; i++) {
        u[i] = gsl_vector_get(residuals, i);
        x_outer[i] = (double*)malloc(p * p * sizeof(double));
        for (int j = 0; j < p; j++) {
            for (int k = 0; k < p; k++) {
                double x_ij = gsl_matrix_get(X, i, j);
                double x_ik = gsl_matrix_get(X, i, k);
                x_outer[i][j * p + k] = x_ij * x_ik * u[i] * u[i];
            }
        }
    }

    // lag 0
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < p; j++) {
            for (int k = 0; k < p; k++) {
                double current = gsl_matrix_get(S, j, k);
                gsl_matrix_set(S, j, k, current + x_outer[i][j * p + k]);
            }
        }
    }

    // lags, Bartlett kernel: S += w (Gamma_l + Gamma_l')
    for (int lag = 1; lag <= max_lags; lag++) {
        double weight = 1.0 - (double)lag / (max_lags + 1.0);
        for (int i = lag; i < n; i++) {
            double g = u[i] * u[i - lag] * weight;
            for (int j = 0; j < p; j++) {
                double x_t_j = gsl_matrix_get(X, i, j);
                double x_l_j = gsl_matrix_get(X, i - lag, j);
                for (int k = 0; k < p; k++) {
                    double x_t_k = gsl_matrix_get(X, i, k);
                    double x_l_k = gsl_matrix_get(X, i - lag, k);
                    double cur = gsl_matrix_get(S, j, k);
                    gsl_matrix_set(S, j, k, cur + g * (x_t_j * x_l_k + x_l_j * x_t_k));
                }
            }
        }
    }

    double S_norm = 0.0;
    for (int i = 0; i < p; i++)
        for (int j = 0; j < p; j++)
            S_norm += fabs(gsl_matrix_get(S, i, j));
    printf("Norma de S: %.6e\n", S_norm);

    gsl_matrix *temp = gsl_matrix_alloc(p, p);
    gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1.0, xtx_inv, S, 0.0, temp);
    gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1.0, temp, xtx_inv, 0.0, hac_cov);

    double hac_norm = 0.0;
    for (int i = 0; i < p; i++)
        for (int j = 0; j < p; j++)
            hac_norm += fabs(gsl_matrix_get(hac_cov, i, j));
    printf("Norma de HAC: %.6e\n", hac_norm);

    free(u);
    for (int i = 0; i < n; i++) free(x_outer[i]);
    free(x_outer);

    gsl_matrix_free(xtx);
    gsl_matrix_free(xtx_inv);
    gsl_matrix_free(S);
    gsl_matrix_free(temp);
    gsl_permutation_free(perm);

    printf("✅ HAC calculada\n");
    return hac_cov;
}

/* ---------- Regresión armónica con base diferenciada (núcleo) ---------- */
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
    int total_params = num_harmonics + 1;  // intercept + armónicos

    if (n <= total_params) {
        printf("Error: Insufficient observations (%d) for %d parameters\n", n, total_params);
        return 0;
    }

    printf("Regresión armónica con base diferenciada: n=%d, s=%d, d=%d, armónicos=%d, params=%d\n",
           n, s, d, num_harmonics, total_params);

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

    // Matrices GSL
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

    for (int i = 0; i < n; i++)
        gsl_vector_set(y_vec, i, y[i]);

    // Construir matriz de diseño con base diferenciada
    for (int i = 0; i < n; i++) {
        double t = i + d + 1;  // tiempo original (1-based)
        gsl_matrix_set(X, i, 0, 1.0);  // intercepto

        int col = 1;
        for (int freq = 1; freq <= s/2; freq++) {
            double omega = 2.0 * M_PI * freq / s;
            double cos_vals[3], sin_vals[3];
            for (int k = 0; k <= d; k++) {
                double tk = t - k;
                cos_vals[k] = cos(omega * tk);
                sin_vals[k] = sin(omega * tk);
            }
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
                gsl_matrix_set(X, i, col++, diff_cos);
            }
        }
        if (col != total_params)
            printf("Warning: Column count mismatch at row %d\n", i);
    }

    // Regresión OLS
    gsl_matrix *cov_ols = gsl_matrix_alloc(total_params, total_params);
    double chisq;
    gsl_multifit_linear_workspace *work = gsl_multifit_linear_alloc(n, total_params);
    if (!cov_ols || !work) {
        printf("Error: Memory allocation for OLS\n");
        gsl_matrix_free(X); gsl_vector_free(y_vec); gsl_vector_free(c); gsl_vector_free(residuals);
        if (cov_ols) gsl_matrix_free(cov_ols);
        return 0;
    }

    gsl_error_handler_t *old_handler = gsl_set_error_handler_off();
    int status = gsl_multifit_linear(X, y_vec, c, cov_ols, &chisq, work);
    gsl_set_error_handler(old_handler);

    if (status != GSL_SUCCESS) {
        printf("Warning: OLS regression failed: %s\n", gsl_strerror(status));
        // Fallback: usar la media
        double y_mean = 0.0;
        for (int i = 0; i < n; i++) y_mean += y[i];
        y_mean /= n;
        *intercept = y_mean;
        *intercept_std_error = 0.0;
        for (int i = 0; i < num_harmonics; i++) {
            coefficients[i] = 0.0;
            std_errors[i] = 0.0;
        }
        *f_stat = 0.0; *p_value = 1.0; *r_squared = 0.0;
        *ols_cov_matrix_ptr = gsl_matrix_alloc(total_params, total_params);
        if (*ols_cov_matrix_ptr) gsl_matrix_set_identity(*ols_cov_matrix_ptr);
        gsl_matrix_free(X); gsl_vector_free(y_vec); gsl_vector_free(c); gsl_vector_free(residuals);
        gsl_matrix_free(cov_ols);
        gsl_multifit_linear_free(work);
        return 1; // éxito con fallback
    }

    // Extraer coeficientes y errores estándar
    *intercept = gsl_vector_get(c, 0);
    for (int i = 0; i < num_harmonics; i++)
        coefficients[i] = gsl_vector_get(c, i + 1);
    *intercept_std_error = sqrt(gsl_matrix_get(cov_ols, 0, 0));
    for (int i = 0; i < num_harmonics; i++)
        std_errors[i] = sqrt(gsl_matrix_get(cov_ols, i + 1, i + 1));

    // Residuos y estadísticas
    double y_mean = 0.0;
    for (int i = 0; i < n; i++) y_mean += y[i];
    y_mean /= n;
    double sst = 0.0, ssr = 0.0, sse = 0.0;
    int valid_residuals = 0;
    for (int i = 0; i < n; i++) {
        double y_pred = *intercept;
        for (int j = 0; j < num_harmonics; j++)
            y_pred += coefficients[j] * gsl_matrix_get(X, i, j + 1);
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

    // HAC opcional
    gsl_matrix *hac_cov = NULL;
    if (valid_residuals > total_params && sse / (valid_residuals - total_params) > 1e-12) {
        int max_lags = (n <= 100) ? 1 : (n <= 200) ? 2 : 3;
        printf("Intentando HAC con %d lags\n", max_lags);
        hac_cov = compute_hac_covariance(X, residuals, max_lags);
        if (hac_cov) {
            double hac_norm = 0.0;
            int hac_valid = 1;
            for (int i = 0; i < total_params && hac_valid; i++)
                for (int j = 0; j < total_params && hac_valid; j++) {
                    double val = gsl_matrix_get(hac_cov, i, j);
                    hac_norm += fabs(val);
                    if (isnan(val) || isinf(val)) hac_valid = 0;
                }
            if (!hac_valid || hac_norm < 1e-12 || hac_norm > 1e6) {
                gsl_matrix_free(hac_cov);
                hac_cov = NULL;
            }
        }
    }

    /* The decision F is the HAC Wald F of the harmonics, as art-python's
       identification test (BUG-0206: more power; art's study in
       research/seasonal_test): gamma' V_hac^-1 gamma / q against
       F(q, n - k). The OLS F above stays only as the fallback when the HAC
       covariance cannot be used. */
    if (hac_cov && num_harmonics > 0) {
        int q = num_harmonics;
        gsl_matrix *V = gsl_matrix_alloc(q, q);
        gsl_matrix *Vi = gsl_matrix_alloc(q, q);
        gsl_permutation *pq = gsl_permutation_alloc(q);
        int sg, ok = 1;
        for (int a1 = 0; a1 < q; a1++)
            for (int b1 = 0; b1 < q; b1++)
                gsl_matrix_set(V, a1, b1, gsl_matrix_get(hac_cov, a1 + 1, b1 + 1));
        gsl_error_handler_t *oh = gsl_set_error_handler_off();
        if (gsl_linalg_LU_decomp(V, pq, &sg) != GSL_SUCCESS ||
            gsl_linalg_LU_invert(V, pq, Vi) != GSL_SUCCESS)
            ok = 0;
        gsl_set_error_handler(oh);
        if (ok) {
            double w = 0.0;
            for (int a1 = 0; a1 < q; a1++)
                for (int b1 = 0; b1 < q; b1++)
                    w += coefficients[a1] * gsl_matrix_get(Vi, a1, b1) * coefficients[b1];
            int df2 = valid_residuals - total_params;
            if (df2 < 1) df2 = 1;
            *f_stat = w / q;
            *p_value = gsl_cdf_fdist_Q(*f_stat, q, df2);
            printf("HAC F=%.4f (p=%.4f)\n", *f_stat, *p_value);
        } else {
            printf("HAC covariance not invertible: the OLS F decides\n");
        }
        gsl_matrix_free(V); gsl_matrix_free(Vi); gsl_permutation_free(pq);
    } else {
        printf("No HAC covariance: the OLS F decides\n");
    }

    *hac_cov_matrix_ptr = hac_cov;
    *ols_cov_matrix_ptr = cov_ols;

    gsl_matrix_free(X);
    gsl_vector_free(y_vec);
    gsl_vector_free(c);
    gsl_vector_free(residuals);
    gsl_multifit_linear_free(work);

    printf("✅ Regresión completada: F=%.4f (p=%.4f), R²=%.4f\n",
           *f_stat, *p_value, *r_squared);
    return 1;
}

/* ---------- Transformación de coeficientes armónicos a dummies estacionales ---------- */
double* transform_harmonics_to_dummies_general(double *harmonic_coeffs, gsl_matrix *harmonic_cov, int s) {
    int num_harmonics = s - 1;
    double *dummies = malloc(s * sizeof(double));
    if (!dummies) return NULL;

    gsl_matrix *A0 = generate_A0_matrix(s);
    if (!A0) {
        for (int i = 0; i < s; i++) dummies[i] = 0.0;
        return dummies;
    }

    gsl_vector *coeff_vec = gsl_vector_alloc(num_harmonics);
    for (int i = 0; i < num_harmonics; i++)
        gsl_vector_set(coeff_vec, i, harmonic_coeffs[i]);

    gsl_vector *dummies_vec = gsl_vector_alloc(s - 1);
    gsl_blas_dgemv(CblasNoTrans, 1.0, A0, coeff_vec, 0.0, dummies_vec);

    for (int i = 0; i < s - 1; i++)
        dummies[i] = gsl_vector_get(dummies_vec, i);

    // Última dummy por restricción de suma cero
    dummies[s - 1] = 0.0;
    for (int i = 0; i < s - 1; i++)
        dummies[s - 1] -= dummies[i];

    gsl_matrix_free(A0);
    gsl_vector_free(coeff_vec);
    gsl_vector_free(dummies_vec);

    return dummies;
}

/* ---------- Cálculo de varianzas de las dummies (basado en OLS) ---------- */
double* calculate_dummy_variances_ols_based(gsl_matrix *ols_cov_matrix, int s) {
    if (!ols_cov_matrix) {
        printf("Error: OLS covariance matrix is NULL\n");
        return NULL;
    }

    double *variances = malloc(s * sizeof(double));
    if (!variances) return NULL;

    for (int i = 0; i < s; i++) variances[i] = 0.0;

    gsl_matrix *A0 = generate_A0_matrix(s);
    if (!A0) {
        free(variances);
        return NULL;
    }

    int num_harmonics = s - 1;
    if (ols_cov_matrix->size1 != num_harmonics + 1 || ols_cov_matrix->size2 != num_harmonics + 1) {
        printf("Error: Dimensiones de la matriz de covarianza incorrectas\n");
        gsl_matrix_free(A0);
        free(variances);
        return NULL;
    }

    // Submatriz de covarianza de los coeficientes armónicos (sin intercepto)
    gsl_matrix *harmonic_cov_sub = gsl_matrix_alloc(num_harmonics, num_harmonics);
    for (int i = 0; i < num_harmonics; i++)
        for (int j = 0; j < num_harmonics; j++)
            gsl_matrix_set(harmonic_cov_sub, i, j, gsl_matrix_get(ols_cov_matrix, i + 1, j + 1));

    // Calcular A0 * Cov(γ) * A0^T
    gsl_matrix *temp = gsl_matrix_alloc(num_harmonics, num_harmonics);
    gsl_matrix *A0T = gsl_matrix_alloc(num_harmonics, num_harmonics);
    gsl_matrix_transpose_memcpy(A0T, A0);

    gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1.0, A0, harmonic_cov_sub, 0.0, temp);
    gsl_matrix *variances_matrix = gsl_matrix_alloc(num_harmonics, num_harmonics);
    gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1.0, temp, A0T, 0.0, variances_matrix);

    // Varianzas de las primeras s-1 dummies
    for (int i = 0; i < num_harmonics; i++) {
        double v = gsl_matrix_get(variances_matrix, i, i);
        variances[i] = (v < 1e-12) ? 1e-12 : v;
    }

    // Varianza de la última dummy por suma de todas las covarianzas
    variances[s - 1] = 0.0;
    for (int i = 0; i < num_harmonics; i++)
        for (int j = 0; j < num_harmonics; j++)
            variances[s - 1] += gsl_matrix_get(variances_matrix, i, j);
    if (variances[s - 1] < 1e-12) variances[s - 1] = 1e-12;

    gsl_matrix_free(A0);
    gsl_matrix_free(harmonic_cov_sub);
    gsl_matrix_free(temp);
    gsl_matrix_free(A0T);
    gsl_matrix_free(variances_matrix);

    return variances;
}

/* ---------- Valor crítico de la distribución F ---------- */
double f_distribution_critical_value(int df1, int df2, double alpha) {
    return gsl_cdf_fdist_Qinv(alpha, df1, df2);
}

/* ---------- Función principal de detección estacional (punto de entrada para ART) ---------- */
/* The file entry point (the old GUI's): load, then the array one. */
SeasonalDetectionResult* detect_seasonality_harmonic_regression(const char *filename,
                                                               int d,
                                                               int apply_log,
                                                               int s) {
    double *data;
    int n;
    if (!load_data(filename, &data, &n)) return NULL;
    printf("Cargados %d puntos\n", n);
    SeasonalDetectionResult *r = detect_seasonality_from_array(data, n, d, apply_log, s);
    free(data);
    return r;
}

/* 18.2.1: the test works on an array, so the engine reads its data once.
 * `data` is not modified (a copy is transformed). */
SeasonalDetectionResult* detect_seasonality_from_array(const double *data, int n_original,
                                                       int d, int apply_log, int s) {
    SeasonalDetectionResult *result = malloc(sizeof(SeasonalDetectionResult));
    if (!result) {
        printf("Error: Memory allocation failed for result structure\n");
        return NULL;
    }
    memset(result, 0, sizeof(SeasonalDetectionResult));
    result->seasonal_detected = 0;
    result->num_harmonics = s - 1;
    result->seasonal_period = s;

    if (s < 2 || s > 12) {
        result->message = strdup("Error: Seasonal period must be between 2 and 12");
        free(result);
        return NULL;
    }

    printf("Iniciando detección estacional (base diferenciada): s=%d, d=%d, log=%d\n", s, d, apply_log);

    double *original_data = malloc(n_original * sizeof(double));
    if (!original_data) { free(result); return NULL; }
    memcpy(original_data, data, n_original * sizeof(double));

    // Transformación logarítmica (reescalada a porcentaje)
    if (apply_log) {
        apply_log_transform_rescaled(original_data, n_original);
        printf("Aplicada transformación 100*log\n");
    }

    // Aplicar diferencias regulares (en el lugar)
    int n_diff = n_original;
    double *differenced_data = malloc(n_original * sizeof(double));
    if (!differenced_data) {
        free(original_data);
        free(result);
        return NULL;
    }
    memcpy(differenced_data, original_data, n_original * sizeof(double));
    apply_regular_differences(differenced_data, &n_diff, d);
    printf("Después de %d diferencias: %d observaciones\n", d, n_diff);

    if (n_diff <= 2 * s) {
        free(original_data);
        free(differenced_data);
        result->message = strdup("Error: Insufficient data after differencing");
        free(result);
        return NULL;
    }

    // Regresión armónica con base diferenciada
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
        result->message = strdup("Error: Memory allocation for regression arrays");
        free(result);
        return NULL;
    }

    int regression_ok = harmonic_regression_differenced_basis(
        differenced_data, n_diff, d,
        coefficients, std_errors,
        &hac_cov_matrix, &ols_cov_matrix,
        &intercept, &intercept_std_error,
        &f_stat, &p_value, &r_squared,
        s);

    if (!regression_ok) {
        free(original_data);
        free(differenced_data);
        free(coefficients);
        free(std_errors);
        result->message = strdup("Error: Harmonic regression failed");
        free(result);
        return NULL;
    }

    printf("Regresión completada: F=%.3f, p=%.4f, R²=%.3f\n", f_stat, p_value, r_squared);

    // Prueba de significación global (test F con nivel de significancia)
    double f_critical = f_distribution_critical_value(num_harmonics,
                                                     n_diff - num_harmonics - 1,
                                                     SIGNIFICANCE_LEVEL);
    result->seasonal_detected = (f_stat > f_critical);
    result->f_statistic = f_stat;
    result->p_value = p_value;
    result->harmonic_coeffs = coefficients;
    result->harmonic_std_errors = std_errors;
    result->harmonic_cov_matrix = hac_cov_matrix;  // puede ser NULL
    result->intercept = intercept;
    result->intercept_std_error = intercept_std_error;

    // Transformar a dummies estacionales de nivel (los coeficientes ya son de nivel)
    result->seasonal_dummies = transform_harmonics_to_dummies_general(coefficients, ols_cov_matrix, s);

    // Calcular varianzas y errores estándar de las dummies (basados en OLS)
    double *dummy_variances = calculate_dummy_variances_ols_based(ols_cov_matrix, s);
    result->dummy_confidence_lower = malloc(s * sizeof(double));
    result->dummy_confidence_upper = malloc(s * sizeof(double));
    result->dummy_std_errors = malloc(s * sizeof(double));

    if (dummy_variances && result->dummy_std_errors) {
        for (int i = 0; i < s; i++) {
            double se = sqrt(dummy_variances[i]);
            result->dummy_std_errors[i] = se;
            result->dummy_confidence_lower[i] = -1.96 * se;
            result->dummy_confidence_upper[i] = 1.96 * se;
        }
        free(dummy_variances);
    } else {
        // Fallback: errores estándar aproximados
        for (int i = 0; i < s; i++) {
            result->dummy_std_errors[i] = fabs(result->seasonal_dummies[i]) * 0.1;
            if (result->dummy_std_errors[i] < 1e-6) result->dummy_std_errors[i] = 0.01;
            result->dummy_confidence_lower[i] = -1.96 * result->dummy_std_errors[i];
            result->dummy_confidence_upper[i] = 1.96 * result->dummy_std_errors[i];
        }
    }

    // Mensaje final
    char msg[256];
    if (result->seasonal_detected) {
        snprintf(msg, sizeof(msg),
                "Seasonality detected (s=%d): HAC F=%.3f (p=%.4f), OLS confidence bands",
                s, f_stat, p_value);
    } else {
        snprintf(msg, sizeof(msg),
                "No significant seasonality (s=%d): HAC F=%.3f (p=%.4f)",
                s, f_stat, p_value);
    }
    result->message = strdup(msg);
    printf("%s\n", msg);

    // Liberar matrices temporales (OLS covariance ya no se necesita)
    if (ols_cov_matrix) gsl_matrix_free(ols_cov_matrix);
    free(original_data);
    free(differenced_data);

    return result;
}

/* ---------- Liberación de memoria ---------- */
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

/* ---------- Impresión de resultados (opcional) ---------- */
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
    if (result->seasonal_dummies) {
        printf("\nSEASONAL DUMMY COEFFICIENTS:\n");
        for (int i = 0; i < result->seasonal_period; i++) {
            printf("  Period %2d: %8.4f [%8.4f, %8.4f] ± %.4f\n",
                   i + 1,
                   result->seasonal_dummies[i],
                   result->dummy_confidence_lower[i],
                   result->dummy_confidence_upper[i],
                   result->dummy_std_errors[i]);
        }
    }
    printf("==================================\n\n");
}
