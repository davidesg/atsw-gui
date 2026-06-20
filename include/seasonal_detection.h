/**
 * @file seasonal_detection.h
 * @brief Detección de patrones estacionales usando regresión armónica con test F HAC y bandas OLS
 *
 * Este módulo implementa detección robusta de patrones estacionales en series temporales usando
 * regresión armónica con el siguiente enfoque:
 * - Matriz de covarianza corregida por HAC para test F de significancia global
 * - Matriz de covarianza OLS para bandas de confianza visual de dummies estacionales
 * - Datos reescalados por 100*log() para interpretación porcentual
 */

#ifndef SEASONAL_DETECTION_H
#define SEASONAL_DETECTION_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <gsl/gsl_matrix.h>
#include "unit_root_tests.h"
// Usar definiciones de model_detection.h
#include "model_detection.h"

#define SIGNIFICANCE_LEVEL 0.05
#define DEFAULT_MAX_LAGS 4

/**
 * @brief Resultados de detección estacional usando regresión armónica
 *
 * Esta estructura almacena todos los resultados de la detección de patrones estacionales,
 * incluyendo coeficientes, estadísticas e intervalos de confianza.
 * Importante: El test F usa covarianza HAC, mientras que las bandas de confianza usan OLS.
 */
typedef struct {
    int seasonal_detected;           /**< Flag indicando estacionalidad detectada (test F HAC) */
    double *seasonal_dummies;        /**< Vector de coeficientes de dummies estacionales (ω_i) */
    double *dummy_confidence_lower;  /**< Límites inferiores de confianza (basados en OLS) */
    double *dummy_confidence_upper;  /**< Límites superiores de confianza (basados en OLS) */
    double *dummy_std_errors;        /**< Errores estándar de dummies estacionales (basados en OLS) */
    double f_statistic;              /**< Estadístico F de regresión armónica (basado en HAC) */
    double p_value;                  /**< Valor p del test F (basado en HAC) */
    double *harmonic_coeffs;         /**< Coeficientes de regresión armónica (γ) */
    double *harmonic_std_errors;     /**< Errores estándar de coeficientes armónicos (basados en OLS) */
    gsl_matrix *harmonic_cov_matrix; /**< Matriz de covarianza HAC de coeficientes armónicos */
    double intercept;                /**< Término intercepto de la regresión */
    double intercept_std_error;      /**< Error estándar del intercepto (basado en OLS) */
    int num_harmonics;               /**< Número de componentes armónicos usados */
    int seasonal_period;             /**< Período estacional (s) */
    char *message;                   /**< Mensaje descriptivo sobre resultados de detección */
} SeasonalDetectionResult;

// Declaraciones de funciones externas
extern int load_data(const char *filename, double **data, int *n_points);

// Funciones principales de detección
SeasonalDetectionResult* detect_seasonality_harmonic_regression(const char *filename,
                                                               int d,
                                                               int apply_log,
                                                               int s);
void free_seasonal_detection_result(SeasonalDetectionResult *result);
void plot_seasonal_dummies(SeasonalDetectionResult *result, int apply_log);
void print_seasonal_detection_result(SeasonalDetectionResult *result);

// Funciones de transformación de matrices
gsl_matrix* generate_A0_matrix(int s);
int verify_A0_matrix();
void test_A0_matrices();

// Funciones de cálculo estadístico
void apply_regular_differences(double *data, int *n_points, int d);
int harmonic_regression_dual_covariance(double *y, int n, double *coefficients, double *std_errors,
                                       gsl_matrix **hac_cov_matrix_ptr, gsl_matrix **ols_cov_matrix_ptr,
                                       double *intercept, double *intercept_std_error, double *f_stat,
                                       double *p_value, double *r_squared, int s);
double* calculate_dummy_variances_ols_based(gsl_matrix *ols_cov_matrix, int s);
gsl_matrix* compute_hac_covariance(gsl_matrix *X, gsl_vector *residuals, int max_lags);
double f_distribution_critical_value(int df1, int df2, double alpha);
double f_distribution_p_value(double f_stat, int df1, int df2);
void apply_log_transform_rescaled(double *data, int n_points);
double* transform_harmonics_to_dummies_general(double *harmonic_coeffs, gsl_matrix *harmonic_cov, int s);
void diagnostic_hac_f_test(double *coefficients, gsl_matrix *hac_cov,
                          int num_harmonics, int n, int total_params);
// Nueva función para obtener residuos de regresión armónica
double* get_harmonic_regression_residuals(double *y, int n, double *coefficients,
                                        double intercept, int s);

#endif /* SEASONAL_DETECTION_H */
