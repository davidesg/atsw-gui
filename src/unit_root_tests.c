/**
 * @file unit_root_tests.c
 * @brief Implementación de contrastes ADF y KPSS para raíces unitarias usando GSL
 */

#include "unit_root_tests.h"
#include <string.h>
#include <gsl/gsl_multifit.h>
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_permutation.h>

/**
 * @brief Realiza los contrastes ADF y KPSS para detectar raíces unitarias usando GSL
 */
UnitRootTestResult* perform_unit_root_tests(double *data, int n, int has_seasonality, double *residuals) {
    UnitRootTestResult *result = malloc(sizeof(UnitRootTestResult));
    if (!result) return NULL;

    memset(result, 0, sizeof(UnitRootTestResult));

    // Determinar qué serie usar para los tests
    double *test_series = data;
    int test_n = n;

    if (has_seasonality && residuals) {
        test_series = residuals;
        printf("Usando residuos de regresión armónica para contrastes de raíces unitarias\n");
    } else {
        printf("Usando serie original para contrastes de raíces unitarias\n");
    }

    // Verificar longitud mínima
    if (test_n < 25) {
        result->warning_message = strdup("Serie demasiado corta para contrastes de raíces unitarias confiables");
        result->unit_root_suspected = 0;
        return result;
    }

    // CONTRASTE ADF (Augmented Dickey-Fuller) usando GSL
    printf("Realizando contraste ADF con GSL...\n");
    result->adf_test_statistic = adf_test_statistic_gsl(test_series, test_n);

    // Valor crítico ADF al 5%
    double adf_critical_value = -2.86; // Para n grande, modelo con constante

    // Calcular valor p aproximado usando distribución t
    result->adf_p_value = gsl_cdf_tdist_P(result->adf_test_statistic, test_n - 5);
    result->adf_rejects_unit_root = (result->adf_test_statistic < adf_critical_value);

    printf("ADF: estadístico=%.4f, crítico=%.4f, rechaza H0=%s, p=%.4f\n",
           result->adf_test_statistic, adf_critical_value,
           result->adf_rejects_unit_root ? "SÍ" : "NO", result->adf_p_value);

    // CONTRASTE KPSS (Kwiatkowski-Phillips-Schmidt-Shin) usando GSL
    printf("Realizando contraste KPSS con GSL...\n");
    result->kpss_test_statistic = kpss_test_statistic_gsl(test_series, test_n);
    result->kpss_critical_value = kpss_critical_value_5pct(test_n);

    result->kpss_rejects_stationarity = (result->kpss_test_statistic > result->kpss_critical_value);

    printf("KPSS: estadístico=%.4f, crítico=%.4f, rechaza H0=%s\n",
           result->kpss_test_statistic, result->kpss_critical_value,
           result->kpss_rejects_stationarity ? "SÍ" : "NO");

    // EVALUACIÓN CONJUNTA
    result->unit_root_suspected = (!result->adf_rejects_unit_root && result->kpss_rejects_stationarity);

    // Construir mensaje de advertencia
    if (result->unit_root_suspected) {
        char buffer[512];
        snprintf(buffer, sizeof(buffer),
                "ADVERTENCIA: Posible raíz unitaria detectada.\n"
                "ADF no rechaza raíz unitaria (estadístico=%.4f, p=%.4f) y KPSS rechaza estacionariedad.\n"
                "Los resultados del modelo ARMA/SARIMA pueden estar viciados.\n"
                "Considere aplicar diferencias regulares (d>0).",
                result->adf_test_statistic, result->adf_p_value);
        result->warning_message = strdup(buffer);
    } else if (!result->adf_rejects_unit_root) {
        char buffer[256];
        snprintf(buffer, sizeof(buffer),
                "ADF no rechaza raíz unitaria (estadístico=%.4f, p=%.4f).\n"
                "Considere verificar estacionariedad antes del modelado ARMA.",
                result->adf_test_statistic, result->adf_p_value);
        result->warning_message = strdup(buffer);
    } else if (result->kpss_rejects_stationarity) {
        char buffer[256];
        snprintf(buffer, sizeof(buffer),
                "KPSS rechaza estacionariedad (estadístico=%.4f).\n"
                "Verifique la presencia de tendencias o raíces unitarias.",
                result->kpss_test_statistic);
        result->warning_message = strdup(buffer);
    } else {
        result->warning_message = strdup("No se detectaron problemas evidentes de estacionariedad.");
    }

    printf("Resultado conjunto: %sraíz unitaria sospechada\n",
           result->unit_root_suspected ? "" : "NO ");

    return result;
}

/**
 * @brief Calcula el estadístico ADF usando GSL para regresión
 */
double adf_test_statistic_gsl(double *data, int n) {
    // Determinar orden de lags automáticamente (regla de Schwert)
    int max_lags = (int)pow(n/100.0, 1.0/4.0) * 12;
    max_lags = (max_lags < 1) ? 1 : (max_lags > 12) ? 12 : max_lags;

    printf("ADF: n=%d, lags=%d\n", n, max_lags);

    // Número de observaciones efectivas después de considerar lags
    int effective_n = n - max_lags - 1;
    if (effective_n < max_lags + 5) {
        max_lags = 1;
        effective_n = n - max_lags - 1;
        if (effective_n < 10) return 0.0;
    }

    // Número de variables: constante + y_{t-1} + lags de Δy
    int p = max_lags + 2;

    // Configurar matrices para GSL
    gsl_matrix *X = gsl_matrix_alloc(effective_n, p);
    gsl_vector *y = gsl_vector_alloc(effective_n);
    gsl_vector *c = gsl_vector_alloc(p);
    gsl_matrix *cov = gsl_matrix_alloc(p, p);
    double chisq;

    // Calcular primeras diferencias
    double *diff = malloc((n-1) * sizeof(double));
    for (int i = 0; i < n-1; i++) {
        diff[i] = data[i+1] - data[i];
    }

    // Construir matriz de diseño X y vector y
    for (int i = 0; i < effective_n; i++) {
        int t = max_lags + i;

        // Variable dependiente: Δy_t
        gsl_vector_set(y, i, diff[t]);

        // Columna 0: constante
        gsl_matrix_set(X, i, 0, 1.0);

        // Columna 1: y_{t-1} (nivel rezagado)
        gsl_matrix_set(X, i, 1, data[t]);

        // Columnas 2...p-1: lags de Δy
        for (int j = 0; j < max_lags; j++) {
            gsl_matrix_set(X, i, j+2, diff[t - j - 1]);
        }
    }

    // Realizar regresión usando GSL
    gsl_multifit_linear_workspace *work = gsl_multifit_linear_alloc(effective_n, p);
    int status = gsl_multifit_linear(X, y, c, cov, &chisq, work);

    double t_statistic = 0.0;

    if (status == GSL_SUCCESS) {
        // Obtener el coeficiente para y_{t-1} (índice 1) y su error estándar
        double beta = gsl_vector_get(c, 1);
        double se_beta = sqrt(gsl_matrix_get(cov, 1, 1));

        if (fabs(se_beta) > 1e-12) {
            t_statistic = beta / se_beta;
        }

        printf("ADF: beta(y_{t-1})=%.6f, SE=%.6f, t=%.4f\n", beta, se_beta, t_statistic);
    } else {
        printf("Error en regresión ADF: %s\n", gsl_strerror(status));
    }

    // Liberar memoria GSL
    gsl_matrix_free(X);
    gsl_vector_free(y);
    gsl_vector_free(c);
    gsl_matrix_free(cov);
    gsl_multifit_linear_free(work);
    free(diff);

    return t_statistic;
}

/**
 * @brief Calcula el estadístico KPSS usando GSL
 */
double kpss_test_statistic_gsl(double *data, int n) {
    // Calcular media de la serie
    double mean = 0.0;
    for (int i = 0; i < n; i++) {
        mean += data[i];
    }
    mean /= n;

    // Calcular residuos respecto a la media
    gsl_vector *residuals = gsl_vector_alloc(n);
    for (int i = 0; i < n; i++) {
        gsl_vector_set(residuals, i, data[i] - mean);
    }

    // Calcular suma acumulada S_t
    gsl_vector *S = gsl_vector_alloc(n);
    gsl_vector_set(S, 0, gsl_vector_get(residuals, 0));
    for (int i = 1; i < n; i++) {
        double St_prev = gsl_vector_get(S, i-1);
        gsl_vector_set(S, i, St_prev + gsl_vector_get(residuals, i));
    }

    // Calcular suma de cuadrados de S_t
    double sum_S2 = 0.0;
    for (int i = 0; i < n; i++) {
        double St = gsl_vector_get(S, i);
        sum_S2 += St * St;
    }

    // Estimar varianza de largo plazo usando kernel de Bartlett
    int max_lags = (int)pow(n, 1.0/3.0); // Regla práctica
    max_lags = (max_lags < 1) ? 1 : (max_lags > 8) ? 8 : max_lags;

    double long_run_variance = 0.0;

    // Lag 0
    double gamma0 = 0.0;
    for (int i = 0; i < n; i++) {
        double res = gsl_vector_get(residuals, i);
        gamma0 += res * res;
    }
    gamma0 /= n;
    long_run_variance += gamma0;

    // Lags 1 a max_lags
    for (int lag = 1; lag <= max_lags; lag++) {
        double gamma_lag = 0.0;
        int count = 0;

        for (int i = 0; i < n - lag; i++) {
            gamma_lag += gsl_vector_get(residuals, i) * gsl_vector_get(residuals, i + lag);
            count++;
        }

        if (count > 0) {
            gamma_lag /= count;
            double weight = 1.0 - (double)lag / (max_lags + 1.0); // Kernel de Bartlett
            long_run_variance += 2.0 * weight * gamma_lag;
        }
    }

    // Asegurar que la varianza sea positiva
    if (long_run_variance < 1e-12) {
        long_run_variance = 1e-12;
    }

    // Calcular estadístico KPSS
    double kpss_stat = sum_S2 / (n * n * long_run_variance);

    printf("KPSS: n=%d, lags=%d, var_largo_plazo=%.6f, estadístico=%.4f\n",
           n, max_lags, long_run_variance, kpss_stat);

    // Liberar memoria
    gsl_vector_free(residuals);
    gsl_vector_free(S);

    return kpss_stat;
}

/**
 * @brief Valor crítico para KPSS al 5% (valores de MacKinnon)
 */
double kpss_critical_value_5pct(int n) {
    // Valores críticos de MacKinnon para KPSS (sin tendencia)
    if (n <= 25) return 0.347;
    if (n <= 50) return 0.463;
    if (n <= 100) return 0.574;
    if (n <= 250) return 0.739;
    if (n <= 500) return 0.863;
    return 0.939; // n > 500
}

/**
 * @brief Libera memoria de los resultados de contrastes de raíces unitarias
 */
void free_unit_root_test_result(UnitRootTestResult *result) {
    if (!result) return;

    if (result->warning_message) {
        free(result->warning_message);
    }
    free(result);
}
