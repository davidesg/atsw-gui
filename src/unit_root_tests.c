/**
 * @file unit_root_tests.c
 * @brief Contrastes ADF y KPSS para raíces unitarias (GSL para la regresión).
 *
 * Reescrito el 2026-09-28 (rama fix-unit-root-tests). Las fórmulas son las de
 * art-python `art/_raiz_unitaria.py`, que reproduce a statsmodels 0.14
 * (`adfuller(autolag="AIC")`, `kpss(regression="c", nlags="auto")`), de modo
 * que C, Python y statsmodels dan el mismo número. Defectos corregidos:
 *
 *  ADF
 *   1. El p-valor salía de una t de Student (gsl_cdf_tdist_P); la ley de
 *      Dickey-Fuller no es una t. Ahora: superficie de respuesta de MacKinnon
 *      (1994), constante, N = 1.
 *   2. El crítico era fijo (-2.86, el asintótico). Ahora: MacKinnon (2010),
 *      con la corrección por el tamaño muestral.
 *   3. El retardo máximo era (int)pow(n/100, 1/4) * 12: el cast truncaba
 *      ANTES de multiplicar (0 -> 1 para n < 100, 12 para 100 <= n < 1600).
 *      Ahora: Schwert, ceil(12 (n/100)^(1/4)), con tope n/2 - 2.
 *   4. No elegía el retardo: usaba siempre el máximo. Ahora: AIC sobre una
 *      muestra común (la que deja el retardo máximo), y el elegido se
 *      reestima sobre su propia muestra.
 *  KPSS
 *   5. El "crítico al 5 %" cambiaba con n entre 0.347/0.463/0.574/0.739 (las
 *      columnas del 10 % al 1 % de la tabla de KPSS, que no dependen de n) y
 *      0.863/0.939 (que no son de ninguna tabla). Ahora: 0.463, y el p-valor
 *      interpolado en la tabla 1 de KPSS (1992), acotado a [0.01, 0.10].
 *   6. El ancho de banda era n^(1/3) con tope 8. Ahora: Hobijn, Franses y
 *      Ooms (1998).
 *   7. Las autocovarianzas se dividían por n - k; el estimador de KPSS divide
 *      por n.
 *  Y la decisión: ADF y KPSS rechazan con p < 0.05, como art-python.
 *
 * Referencias: Dickey y Fuller (1979); MacKinnon (1994), JBES 12, 167-176;
 * MacKinnon (2010), Queen's WP 1227; Schwert (1989), JBES 7, 147-159;
 * Kwiatkowski, Phillips, Schmidt y Shin (1992), J. Econometrics 54, 159-178;
 * Hobijn, Franses y Ooms (1998), Econometric Institute Report 9802/A.
 */

#include "unit_root_tests.h"
#include <string.h>
#include <gsl/gsl_multifit.h>
#include <gsl/gsl_errno.h>

/* MacKinnon (1994), constante, N = 1: potencias crecientes, ya escaladas.   */
static const double TAU_STAR = -1.61, TAU_MIN = -18.83, TAU_MAX = 2.74;
static const double SMALLP[3] = { 2.1659, 1.4412, 3.8269e-2 };
static const double LARGEP[4] = { 1.7339, 9.3202e-1, -1.2745e-1, -1.0368e-2 };

/* MacKinnon (2010), constante, N = 1: 1 %, 5 %, 10 %.                       */
static const double CRIT2010[3][4] = {
    { -3.43035, -6.5393, -16.786, -79.433 },
    { -2.86154, -2.8903,  -4.234, -40.040 },
    { -2.56677, -1.5384,  -2.809,   0.0   } };

/* KPSS (1992), tabla 1, estacionariedad en nivel: 10 %, 5 %, 2.5 %, 1 %.     */
static const double KPSS_CRIT[4] = { 0.347, 0.463, 0.574, 0.739 };
static const double KPSS_P[4]    = { 0.10,  0.05,  0.025, 0.01  };

double mackinnon_p(double tau)
{
    const double *c; int k, m; double s = 0.0, x = 1.0;
    if (tau > TAU_MAX) return 1.0;
    if (tau < TAU_MIN) return 0.0;
    if (tau <= TAU_STAR) { c = SMALLP; m = 3; } else { c = LARGEP; m = 4; }
    for (k = 0; k < m; k++) { s += c[k] * x; x *= tau; }
    return gsl_cdf_ugaussian_P(s);
}

void mackinnon_crit(int nobs, double crit[3])
{
    double T = (double) nobs;
    for (int i = 0; i < 3; i++) {
        const double *b = CRIT2010[i];
        crit[i] = b[0] + b[1] / T + b[2] / (T * T) + b[3] / (T * T * T);
    }
}

/* OLS: el t de la columna `col` y el AIC gaussiano, -2 llf + 2 p, como      */
/* statsmodels. gsl_multifit_linear devuelve cov = s² (X'X)^-1, s² = SCR/(n-p). */
static int ols_t_aic(gsl_matrix *X, gsl_vector *y, int col, double *t, double *aic)
{
    int n = (int) X->size1, p = (int) X->size2, status;
    double chisq = 0.0;
    gsl_vector *c = gsl_vector_alloc(p);
    gsl_matrix *cov = gsl_matrix_alloc(p, p);
    gsl_multifit_linear_workspace *w = gsl_multifit_linear_alloc(n, p);
    gsl_error_handler_t *old = gsl_set_error_handler_off();

    status = gsl_multifit_linear(X, y, c, cov, &chisq, w);
    gsl_set_error_handler(old);
    if (status == GSL_SUCCESS) {
        double se = sqrt(gsl_matrix_get(cov, col, col));
        *t = (se > 0.0) ? gsl_vector_get(c, col) / se : 0.0;
        *aic = n * (log(2.0 * M_PI) + log(chisq / n) + 1.0) + 2.0 * p;
    }
    gsl_multifit_linear_free(w);
    gsl_matrix_free(cov);
    gsl_vector_free(c);
    return status;
}

/* Regresión ADF sobre las diferencias d[t], t = start .. nd-1:              */
/*   Δx_t  ~  [x_t (nivel retardado), Δx_{t-1} .. Δx_{t-lags}, 1]            */
static void adf_design(const double *x, const double *d, int nd, int start,
                       int lags, gsl_matrix **X, gsl_vector **y)
{
    int nobs = nd - start, i, j;
    *X = gsl_matrix_alloc(nobs, lags + 2);
    *y = gsl_vector_alloc(nobs);
    for (i = 0; i < nobs; i++) {
        int t = start + i;
        gsl_vector_set(*y, i, d[t]);
        gsl_matrix_set(*X, i, 0, x[t]);
        for (j = 1; j <= lags; j++)
            gsl_matrix_set(*X, i, j, d[t - j]);
        gsl_matrix_set(*X, i, lags + 1, 1.0);
    }
}

int adf_test(const double *x, int n, double *stat, double *pvalue,
             int *usedlag, int *nobs, double crit[3])
{
    int maxlag, nd = n - 1, lag, best = 0, status = GSL_SUCCESS;
    double *d, besta = 0.0, t = 0.0, aic = 0.0;
    gsl_matrix *X; gsl_vector *y;

    maxlag = (int) ceil(12.0 * pow(n / 100.0, 0.25));
    if (maxlag > n / 2 - 2) maxlag = n / 2 - 2;
    if (maxlag < 0 || nd - maxlag < 3) return -1;

    d = malloc(nd * sizeof(double));
    for (int i = 0; i < nd; i++) d[i] = x[i + 1] - x[i];

    /* Búsqueda por AIC sobre la muestra común que deja maxlag */
    for (lag = 0; lag <= maxlag; lag++) {
        adf_design(x, d, nd, maxlag, lag, &X, &y);
        status = ols_t_aic(X, y, 0, &t, &aic);
        gsl_matrix_free(X); gsl_vector_free(y);
        if (status != GSL_SUCCESS) break;
        if (lag == 0 || aic < besta) { besta = aic; best = lag; }
    }

    /* El retardo elegido, sobre su propia muestra */
    adf_design(x, d, nd, best, best, &X, &y);
    *nobs = (int) y->size;
    status = ols_t_aic(X, y, 0, stat, &aic);
    gsl_matrix_free(X); gsl_vector_free(y);
    free(d);

    *usedlag = best;
    *pvalue = mackinnon_p(*stat);
    mackinnon_crit(*nobs, crit);
    return status == GSL_SUCCESS ? 0 : -1;
}

/* Hobijn, Franses y Ooms (1998): el ancho de banda automático de Bartlett. */
static int kpss_bandwidth(const double *e, int n)
{
    int covlags = (int) pow(n, 2.0 / 9.0), i, t;
    double s0 = 0.0, s1 = 0.0;
    for (t = 0; t < n; t++) s0 += e[t] * e[t];
    s0 /= n;
    for (i = 1; i <= covlags; i++) {
        double g = 0.0;
        for (t = i; t < n; t++) g += e[t] * e[t - i];
        g /= n / 2.0;
        s0 += g;
        s1 += i * g;
    }
    return (int) (1.1447 * pow((s1 / s0) * (s1 / s0), 1.0 / 3.0) * pow(n, 1.0 / 3.0));
}

int kpss_test(const double *x, int n, double *stat, double *pvalue, int *lags)
{
    double mean = 0.0, S = 0.0, eta = 0.0, s = 0.0;
    double *e;
    int i, t, L;

    if (n < 2) return -1;
    e = malloc(n * sizeof(double));
    for (t = 0; t < n; t++) mean += x[t];
    mean /= n;
    for (t = 0; t < n; t++) { e[t] = x[t] - mean; S += e[t]; eta += S * S; }
    eta /= (double) n * n;

    L = kpss_bandwidth(e, n);
    if (L > n - 1) L = n - 1;
    for (t = 0; t < n; t++) s += e[t] * e[t];
    for (i = 1; i <= L; i++) {
        double g = 0.0;
        for (t = i; t < n; t++) g += e[t] * e[t - i];
        s += 2.0 * g * (1.0 - i / (L + 1.0));
    }
    free(e);

    *stat = eta / (s / n);
    *lags = L;
    /* interpolación lineal en la tabla; fuera de ella, el extremo */
    if (*stat <= KPSS_CRIT[0]) *pvalue = KPSS_P[0];
    else if (*stat >= KPSS_CRIT[3]) *pvalue = KPSS_P[3];
    else {
        for (i = 0; i < 3; i++)
            if (*stat <= KPSS_CRIT[i + 1]) {
                double f = (*stat - KPSS_CRIT[i]) / (KPSS_CRIT[i + 1] - KPSS_CRIT[i]);
                *pvalue = KPSS_P[i] + f * (KPSS_P[i + 1] - KPSS_P[i]);
                break;
            }
    }
    return 0;
}

/**
 * @brief Realiza los contrastes ADF y KPSS para detectar raíces unitarias
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

    // CONTRASTE ADF: retardo por AIC, p-valor y críticos de MacKinnon
    double crit[3] = { 0.0, 0.0, 0.0 };
    if (adf_test(test_series, test_n, &result->adf_test_statistic, &result->adf_p_value,
                 &result->adf_lags, &result->adf_nobs, crit) != 0) {
        result->warning_message = strdup("La regresión ADF falló: sin contrastes de raíces unitarias");
        return result;
    }
    result->adf_critical_value = crit[1];
    result->adf_rejects_unit_root = (result->adf_p_value < UNIT_ROOT_SIGNIFICANCE_LEVEL);

    printf("ADF: estadístico=%.4f, retardos=%d, crítico 5%%=%.4f, rechaza H0=%s, p=%.4f\n",
           result->adf_test_statistic, result->adf_lags, result->adf_critical_value,
           result->adf_rejects_unit_root ? "SÍ" : "NO", result->adf_p_value);

    // CONTRASTE KPSS: ancho de banda de Hobijn et al., tabla de KPSS (1992)
    kpss_test(test_series, test_n, &result->kpss_test_statistic, &result->kpss_p_value,
              &result->kpss_lags);
    result->kpss_critical_value = kpss_critical_value_5pct(test_n);
    result->kpss_rejects_stationarity = (result->kpss_p_value < UNIT_ROOT_SIGNIFICANCE_LEVEL);

    printf("KPSS: estadístico=%.4f, retardos=%d, crítico 5%%=%.4f, rechaza H0=%s, p=%.4f\n",
           result->kpss_test_statistic, result->kpss_lags, result->kpss_critical_value,
           result->kpss_rejects_stationarity ? "SÍ" : "NO", result->kpss_p_value);

    // EVALUACIÓN CONJUNTA
    result->unit_root_suspected = (!result->adf_rejects_unit_root && result->kpss_rejects_stationarity);

    // Construir mensaje de advertencia
    if (result->unit_root_suspected) {
        char buffer[512];
        snprintf(buffer, sizeof(buffer),
                "ADVERTENCIA: Posible raíz unitaria detectada.\n"
                "ADF no rechaza raíz unitaria (estadístico=%.4f, p=%.4f) y KPSS rechaza estacionariedad (p=%.4f).\n"
                "Los resultados del modelo ARMA/SARIMA pueden estar viciados.\n"
                "Considere aplicar diferencias regulares (d>0).",
                result->adf_test_statistic, result->adf_p_value, result->kpss_p_value);
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
                "KPSS rechaza estacionariedad (estadístico=%.4f, p=%.4f).\n"
                "Verifique la presencia de tendencias o raíces unitarias.",
                result->kpss_test_statistic, result->kpss_p_value);
        result->warning_message = strdup(buffer);
    } else {
        result->warning_message = strdup("No se detectaron problemas evidentes de estacionariedad.");
    }

    printf("Resultado conjunto: %sraíz unitaria sospechada\n",
           result->unit_root_suspected ? "" : "NO ");

    return result;
}

/* La API anterior, para quien la llame: los mismos contrastes, corregidos. */
double adf_test_statistic_gsl(double *data, int n)
{
    double stat = 0.0, p, crit[3]; int lags, nobs;
    return adf_test(data, n, &stat, &p, &lags, &nobs, crit) == 0 ? stat : 0.0;
}

double kpss_test_statistic_gsl(double *data, int n)
{
    double stat = 0.0, p; int lags;
    return kpss_test(data, n, &stat, &p, &lags) == 0 ? stat : 0.0;
}

/* El crítico de KPSS al 5 % no depende de n (tabla 1 de KPSS, 1992). */
double kpss_critical_value_5pct(int n)
{
    (void) n;
    return KPSS_CRIT[1];
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
