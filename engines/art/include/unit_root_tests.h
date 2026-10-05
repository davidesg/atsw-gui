/**
 * @file unit_root_tests.h
 * @brief Contrastes de raíces unitarias ADF y KPSS para detección de estacionariedad
 */

#ifndef UNIT_ROOT_TESTS_H
#define UNIT_ROOT_TESTS_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <gsl/gsl_statistics.h>
#include <gsl/gsl_cdf.h>

#define UNIT_ROOT_SIGNIFICANCE_LEVEL 0.05

/**
 * @brief Resultados de los contrastes de raíces unitarias
 */
typedef struct {
    int adf_rejects_unit_root;    /**< ADF rechaza H0 (raíz unitaria) */
    int kpss_rejects_stationarity; /**< KPSS rechaza H0 (estacionariedad) */
    double adf_test_statistic;     /**< Estadístico ADF */
    double adf_p_value;           /**< Valor p ADF */
    double kpss_test_statistic;    /**< Estadístico KPSS */
    double kpss_critical_value;   /**< Valor crítico KPSS (5%) */
    char *warning_message;        /**< Mensaje de advertencia */
    int unit_root_suspected;      /**< Bandera de posible raíz unitaria */
    /* 2026-09-28 (fix-unit-root-tests): lo que los contrastes corregidos saben */
    double adf_critical_value;    /**< Crítico ADF 5 % de MacKinnon (2010), por n */
    int adf_lags;                 /**< Retardos elegidos por AIC */
    int adf_nobs;                 /**< Observaciones de la regresión ADF */
    double kpss_p_value;          /**< Valor p KPSS (tabla de KPSS 1992, en [0.01, 0.10]) */
    int kpss_lags;                /**< Ancho de banda de Hobijn et al. (1998) */
} UnitRootTestResult;

// Funciones principales
UnitRootTestResult* perform_unit_root_tests(double *data, int n, int has_seasonality, double *residuals);
void free_unit_root_test_result(UnitRootTestResult *result);

// Funciones de cálculo (las mismas fórmulas que art-python art/_raiz_unitaria.py)
int adf_test(const double *x, int n, double *stat, double *pvalue,
             int *usedlag, int *nobs, double crit[3]);
int kpss_test(const double *x, int n, double *stat, double *pvalue, int *lags);
double mackinnon_p(double tau);
void mackinnon_crit(int nobs, double crit[3]);
double adf_test_statistic_gsl(double *data, int n);
double kpss_test_statistic_gsl(double *data, int n);
double kpss_critical_value_5pct(int n);

// Funciones auxiliares
double calculate_dickey_fuller_critical_value(int n, int trend_type);
double* compute_regression_residuals(double *y, double *x, int n, int include_constant);

#endif /* UNIT_ROOT_TESTS_H */
