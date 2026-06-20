#ifndef MODEL_DETECTION_H
#define MODEL_DETECTION_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unit_root_tests.h"
#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix.h>



#define MAX_DATA_POINTS 10000
#define MAX_LAGS 40
#define GRID_MIN -0.9
#define GRID_MAX 0.9
#define COARSE_GRID_STEP 0.30
#define FINE_GRID_STEP 0.10

#define MAX_SEASONAL_MA_ORDER 1
#define MIN_SEASONAL_MA_COEF 0.1
#define MAX_SEASONAL_MA_COEF 0.8

#define MAHALANOBIS_FEATURE_DIM 42

// Número máximo de candidatos (modelos tentativos estilo Box-Jenkins) en el shortlist
#define MAX_ORDER_CANDIDATES 12

typedef struct {
    double *data;
    int n_points;
    int apply_log;
    int d;
    int D;
    int s;
    int deseasonalize;      // <-- Añadir esta línea
    int use_mahalanobis;      // Nuevo flag
    int mlp_direct;         // Identificación BJ: usar argmax/top-K del MLP sin penalización de parsimonia
} DataParameters;

// Candidato tentativo (orden) con su probabilidad MLP. ART identifica, atsw-MCP estima/elige.
typedef struct {
    int p, q, P, Q;
    double prob;            // probabilidad conjunta aproximada del MLP
} OrderCandidate;

typedef struct {
    int p, q, P, Q;
    double *phi, *theta, *Phi, *Theta;
    double *best_phi, *best_theta, *best_Phi, *best_Theta;
    double similarity;

    // Shortlist de modelos tentativos (filosofía Box-Jenkins) ordenado por probabilidad
    OrderCandidate candidates[MAX_ORDER_CANDIDATES];
    int n_candidates;

    // Para gráficos finales
    double *acf_empirical;
    double *pacf_empirical;
    double *acf_theoretical;
    double *pacf_theoretical;
    int lags_used;
} ModelCandidate;

typedef struct {
    int p, q, P, Q;
    double similarity_original;
    double *best_phi, *best_theta, *best_Phi, *best_Theta; // copia de coeficientes
    gsl_vector *feature_vector;
} ModelRecord;

/**
 * @brief Estructura que almacena características de patrones ACF/PACF
 *
 * Esta estructura contiene todas las características extraídas de las funciones
 * de autocorrelación para la comparación de patrones entre modelos teóricos y empíricos.
 * Incluye características para modelos AR, MA, ARMA y componentes estacionales.
 */
typedef struct {
    // Arrays para comparación directa
    double *acf_values;
    double *pacf_values;

    // Características generales (compatibilidad)
    int cutting_off_lag;
    double decay_rate;
    double peak_values[3];
    int significant_lags;
    double seasonal_pattern;
    int seasonal_peaks;
    double oscillation_freq;

    // Características separadas AR/MA/ARMA
    int acf_cutting_lag;          /**< Corte en ACF (indicador MA) */
    int pacf_cutting_lag;         /**< Corte en PACF (indicador AR) */
    double acf_decay_rate;        /**< Tasa decaimiento ACF (comportamiento AR) */
    double pacf_decay_rate;       /**< Tasa decaimiento PACF (comportamiento MA) */
    int acf_initial_spikes;       /**< Picos iniciales ACF (MA) */
    int pacf_initial_spikes;      /**< Picos iniciales PACF (AR) */
    double mixed_pattern_score;   /**< Puntuación patrón mixto ARMA */
    double parsimony_penalty;     /**< Penalización por complejidad */

    // Métricas de similitud euclídea
    double acf_euclidean_similarity;      /**< Similitud euclídea ACF primeros lags */
    double pacf_euclidean_similarity;     /**< Similitud euclídea PACF primeros lags */
    double seasonal_euclidean_similarity; /**< Similitud euclídea lags estacionales */
    int comparison_lags;                  /**< Número de lags usados en comparación */

    // Características estacionales mejoradas
    double seasonal_acf_strength;    /**< Fuerza estacional en ACF (MA estacional) */
    double seasonal_pacf_strength;   /**< Fuerza estacional en PACF (AR estacional) */
    int seasonal_acf_peaks;          /**< Picos estacionales significativos en ACF */
    int seasonal_pacf_peaks;         /**< Picos estacionales significativos en PACF */
} PatternFeatures;

// Estructura para transferencia de datos de gráficos entre hilos
typedef struct {
    double *acf_theoretical;
    double *pacf_theoretical;
    double *acf_empirical;
    double *pacf_empirical;
    int lags;
    double cmax;
    char *title;
} PlotTransferData;

// Tipo callback para reporte de progreso
typedef void (*ProgressCallback)(int stage, double progress, const char *message, const char *overall);

// Declaraciones de funciones principales
void set_progress_callback(ProgressCallback callback);
void set_identification_mode(int mlp_direct);
void liberar_model_candidate(ModelCandidate *candidate);
int load_data(const char *filename, double **data, int *n_points);
void transform_data(DataParameters *params);
void calcular_ACF_muestral(double *data, int n, double *acf, int lags);
void calcular_PACF_muestral(double *acf, double *pacf, int lags);

// Funciones de extracción y comparación de patrones
void extract_pattern_features(double *acf, double *pacf, int lags, int s, PatternFeatures *features);
double pattern_similarity(PatternFeatures *theoretical, PatternFeatures *empirical, int s, int lags);
double evaluate_model_similarity(int p, double *phi, int q, double *theta,
                                int P, double *Phi, int Q, double *Theta,
                                int s, PatternFeatures *empirical_features, int lags, int n);

// Algoritmo de búsqueda y detección
void adaptive_grid_search(double *empirical_data, int n_data, int s,
                         int p_max, int q_max, int P_max, int Q_max,
                         ModelCandidate *best_candidate,
                         int use_mahalanobis);
int ejecutar_deteccion_automatica(const char *filename, DataParameters *params,
                                 int p_max, int q_max, int P_max, int Q_max,
                                 ModelCandidate *best_candidate);

// Funciones de utilidad y gráficos
void calcular_ACF_PACF_SARIMA(int p, double *phi, int q, double *theta,
                             int P, double *Phi, int Q, double *Theta,
                             int s, double *acf, double *pacf, int lags);
void calculate_euclidean_similarities(PatternFeatures *theoretical, PatternFeatures *empirical,
                                     int lags, int s);
double euclidean_similarity(double *array1, double *array2, int start_lag, int end_lag);
void plot_comparison_acf_pacf(double *acf_theoretical, double *pacf_theoretical,
                             double *acf_empirical, double *pacf_empirical,
                             int lags, ModelCandidate *candidate, DataParameters *data_params);

// Integración con detección estacional
int detectar_y_ajustar_estacionalidad(const char *filename, DataParameters *params,
                                     int *P_max, int *Q_max, char **mensaje_advertencia);
void extract_feature_vector(PatternFeatures *features, double *vector, int dim, int s, int lags);
void reorder_with_mahalanobis(gsl_vector **vectors, int n,
                              gsl_vector *emp_vector,
                              ModelRecord *records, int n_records,
                              double alpha, ModelCandidate *best);
#endif
