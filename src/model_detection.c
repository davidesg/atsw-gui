// model_detection.c - Implementación con reporte de progreso
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "model_detection.h"
#include "ARMA.h"
#include "root.h"
#include <stdlib.h>
#include "weighting_utils.h"
#include "seasonal_detection.h"
#include "ml_classifier.h"
#include <gsl/gsl_permutation.h>
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_blas.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#define usleep(ms) Sleep((ms)/1000)
#else
#include <unistd.h>
#include <pthread.h>
#endif

// Missing macro definitions
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif



// Variable global para el callback de progreso
static ProgressCallback progress_callback = NULL;

// Modo de identificación Box-Jenkins: 1 = salida = argmax/shortlist del MLP (sin parsimonia).
static int g_mlp_direct = 0;
void set_identification_mode(int mlp_direct) { g_mlp_direct = mlp_direct; }

// Función para configurar el callback de progreso
void set_progress_callback(ProgressCallback callback) {
    progress_callback = callback;
}

// Función auxiliar para reportar progreso
static void report_progress_internal(int stage, double progress, const char *message, const char *overall) {
    if (progress_callback) {
        // Si el mensaje comienza con "PLOT_DATA_READY", es una solicitud de gráfico
        if (message && strncmp(message, "PLOT_DATA_READY:", 15) == 0) {
            // Este mensaje especial será manejado por el callback en main.c
            progress_callback(stage, progress, message, overall);
        } else {
            // Mensaje de progreso normal
            progress_callback(stage, progress, message, overall);
        }
    }
}

// CORREGIR: Añadir función send_plot_data que falta


 // Función completa para enviar datos de gráficos al hilo principal de GTK


static void send_plot_data(double *acf_theoretical, double *pacf_theoretical,
                          double *acf_empirical, double *pacf_empirical,
                          int lags, double cmax, const char *title) {

    // Validar parámetros de entrada
    if (!acf_theoretical || !pacf_theoretical || !acf_empirical || !pacf_empirical) {
        printf("ERROR en send_plot_data: Arrays de datos NULL\n");
        return;
    }

    if (lags <= 0 || lags > MAX_LAGS) {
        printf("ERROR en send_plot_data: Número de lags inválido: %d\n", lags);
        return;
    }

    printf("send_plot_data: Preparando gráfico - %s (lags: %d, cmax: %.3f)\n",
           title ? title : "Sin título", lags, cmax);

    // Crear copias profundas de los datos
    double *acf_th_copy = malloc((lags + 1) * sizeof(double));
    double *pacf_th_copy = malloc((lags + 1) * sizeof(double));
    double *acf_emp_copy = malloc((lags + 1) * sizeof(double));
    double *pacf_emp_copy = malloc((lags + 1) * sizeof(double));

    // CORRECCIÓN: Usar strdup de forma segura
    char *title_copy = NULL;
    if (title) {
        title_copy = strdup(title);
    } else {
        title_copy = strdup("Gráfico en Vivo");
    }

    // Verificar asignaciones
    if (!acf_th_copy || !pacf_th_copy || !acf_emp_copy || !pacf_emp_copy || !title_copy) {
        printf("ERROR en send_plot_data: Fallo en asignación de memoria\n");
        free(acf_th_copy);
        free(pacf_th_copy);
        free(acf_emp_copy);
        free(pacf_emp_copy);
        free(title_copy);
        return;
    }

    // Copiar datos
    memcpy(acf_th_copy, acf_theoretical, (lags + 1) * sizeof(double));
    memcpy(pacf_th_copy, pacf_theoretical, (lags + 1) * sizeof(double));
    memcpy(acf_emp_copy, acf_empirical, (lags + 1) * sizeof(double));
    memcpy(pacf_emp_copy, pacf_empirical, (lags + 1) * sizeof(double));

    // Crear estructura de transferencia
    PlotTransferData *transfer_data = malloc(sizeof(PlotTransferData));
    if (!transfer_data) {
        printf("ERROR en send_plot_data: Fallo al crear transfer_data\n");
        free(acf_th_copy);
        free(pacf_th_copy);
        free(acf_emp_copy);
        free(pacf_emp_copy);
        free(title_copy);
        return;
    }

    // Inicializar estructura
    transfer_data->acf_theoretical = acf_th_copy;
    transfer_data->pacf_theoretical = pacf_th_copy;
    transfer_data->acf_empirical = acf_emp_copy;
    transfer_data->pacf_empirical = pacf_emp_copy;
    transfer_data->lags = lags;
    transfer_data->cmax = cmax;
    transfer_data->title = title_copy;

    printf("send_plot_data: Enviando gráfico al hilo principal: %s\n", title_copy);

    // Enviar mensaje especial al hilo principal
    char plot_message[512]; // Aumentar tamaño del buffer
    snprintf(plot_message, sizeof(plot_message),
             "PLOT_DATA_READY:%p:%s",
             (void*)transfer_data, title_copy);

    report_progress_internal(3, 0.0, plot_message, "Mostrando gráfico en vivo...");
}


// Forward declaration
static int estimate_ar_yule_walker(double *acf, int p, double *phi);
static int estimate_arma_hannan_rissanen(double *y, int n, int p, int q,
                                         double *phi, double *theta);
static void build_mlp_shortlist(const MLPPrediction *pred, int ep, int eq,
                                int p_max, int q_max,
                                int P_max, int Q_max, ModelCandidate *best);
static void rank_shortlist_by_fit(ModelCandidate *cand, double *data, int n,
                                  double *acf_emp, double *pacf_emp,
                                  PatternFeatures *emp_features, int lags, int s);
static void add_arma_grid_candidates(ModelCandidate *cand, double *data, int n,
                                     double *acf_emp, int lags, int s,
                                     int p_max, int q_max);
static void add_seasonal_grid_candidates(ModelCandidate *cand, double *data, int n,
                                         double *acf_emp, int lags, int s,
                                         int P_max, int Q_max);

// Función para validar patrones AR basados en PACF empírica
static int validate_ar_pattern(int p, double *pacf_empirical, int lags, double threshold) {
    if (p == 0) return 1;
    // Relaxed: only skip if PACF at lag p is near zero AND later lags are all insignificant
    if (p <= lags && fabs(pacf_empirical[p]) < threshold * 0.3) {
        return 0;
    }
    return 1;  // Always allow — let similarity decide
}

// Función para validar patrones MA basados en ACF empírica
static int validate_ma_pattern(int q, double *acf_empirical, int lags, double threshold) {
    if (q == 0) return 1;
    // Relaxed: only skip if ACF at lag q is near zero
    if (q <= lags && fabs(acf_empirical[q]) < threshold * 0.3) {
        return 0;
    }
    return 1;  // Always allow — let similarity decide
}

// Función para determinar órdenes máximos efectivos basados en significancia empírica
static void determine_effective_orders(double *acf_empirical, double *pacf_empirical,
                                      int lags, int n_data, int s,
                                      int *effective_p_max, int *effective_q_max,
                                      int *effective_P_max, int *effective_Q_max) {
    double threshold = 1.96 / sqrt(n_data);

    // Inicializar órdenes efectivos
    *effective_p_max = 0;
    *effective_q_max = 0;
    *effective_P_max = 0;
    *effective_Q_max = 0;

    // Determinar p_max efectivo (PACF significativa)
    for (int lag = 1; lag <= lags; lag++) {
        if (fabs(pacf_empirical[lag]) > threshold) {
            *effective_p_max = lag;
        } else {
            // Verificar patrón de corte: 3 lags consecutivos no significativos
            if (lag + 2 <= lags &&
                fabs(pacf_empirical[lag]) < threshold &&
                fabs(pacf_empirical[lag + 1]) < threshold &&
                fabs(pacf_empirical[lag + 2]) < threshold) {
                break;
            }
        }
    }

    // Determinar q_max efectivo (ACF significativa)
    for (int lag = 1; lag <= lags; lag++) {
        if (fabs(acf_empirical[lag]) > threshold) {
            *effective_q_max = lag;
        } else {
            // Verificar patrón de corte: 3 lags consecutivos no significativos
            if (lag + 2 <= lags &&
                fabs(acf_empirical[lag]) < threshold &&
                fabs(acf_empirical[lag + 1]) < threshold &&
                fabs(acf_empirical[lag + 2]) < threshold) {
                break;
            }
        }
    }

    // Determinar órdenes estacionales efectivos si hay estacionalidad
    if (s > 1) {
        // P_max efectivo (PACF estacional significativa)
        for (int i = 1; i * s <= lags; i++) {
            int seasonal_lag = i * s;
            if (fabs(pacf_empirical[seasonal_lag]) > threshold * 1.2) { // Umbral más estricto para estacionalidad
                *effective_P_max = i;
            }
        }

        // Q_max efectivo (ACF estacional significativa)
        for (int i = 1; i * s <= lags; i++) {
            int seasonal_lag = i * s;
            if (fabs(acf_empirical[seasonal_lag]) > threshold * 1.2) { // Umbral más estricto para estacionalidad
                *effective_Q_max = i;
            }
        }
    }

    printf("Órdenes efectivos determinados: p=%d, q=%d, P=%d, Q=%d\n",
           *effective_p_max, *effective_q_max, *effective_P_max, *effective_Q_max);
}

// [Las funciones existentes permanecen iguales hasta adaptive_grid_search...]

// Función para liberar memoria de un ModelCandidate (CORREGIDA)
void liberar_model_candidate(ModelCandidate *candidate) {
    if (!candidate) return;

    if (candidate->phi) free(candidate->phi);
    if (candidate->theta) free(candidate->theta);
    if (candidate->Phi) free(candidate->Phi);
    if (candidate->Theta) free(candidate->Theta);
    if (candidate->best_phi) free(candidate->best_phi);
    if (candidate->best_theta) free(candidate->best_theta);
    if (candidate->best_Phi) free(candidate->best_Phi);
    if (candidate->best_Theta) free(candidate->best_Theta);

    // Liberar arrays de gráficos
    if (candidate->acf_empirical) free(candidate->acf_empirical);
    if (candidate->pacf_empirical) free(candidate->pacf_empirical);
    if (candidate->acf_theoretical) free(candidate->acf_theoretical);
    if (candidate->pacf_theoretical) free(candidate->pacf_theoretical);

    candidate->phi = candidate->theta = candidate->Phi = candidate->Theta = NULL;
    candidate->best_phi = candidate->best_theta = candidate->best_Phi = candidate->best_Theta = NULL;
    candidate->acf_empirical = candidate->pacf_empirical = NULL;
    candidate->acf_theoretical = candidate->pacf_theoretical = NULL;
}

// Función para cargar datos desde archivo
 int load_data(const char *filename, double **data, int *n_points) {
    // En Windows, asegurar que la ruta use separadores correctos
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Error: No se pudo abrir el archivo %s\n", filename);
        printf("Error code: %d\n", errno);
        return 0;
    }

    double *temp_data = malloc(MAX_DATA_POINTS * sizeof(double));
    if (!temp_data) {
        fclose(file);
        return 0;
    }

    int count = 0;
    double value;
    char line[256];

    // Leer línea por línea para mejor manejo de errores
    while (fgets(line, sizeof(line), file) != NULL && count < MAX_DATA_POINTS) {
        // Ignorar líneas vacías o comentarios
        if (line[0] == '\n' || line[0] == '#') continue;

        if (sscanf(line, "%lf", &value) == 1) {
            temp_data[count++] = value;
        }
    }

    fclose(file);

    if (count == 0) {
        free(temp_data);
        printf("Error: No se encontraron datos válidos en el archivo\n");
        return 0;
    }

    *data = malloc(count * sizeof(double));
    if (!*data) {
        free(temp_data);
        return 0;
    }

    memcpy(*data, temp_data, count * sizeof(double));
    *n_points = count;
    free(temp_data);

    printf("Datos cargados exitosamente: %d puntos\n", count);
    return 1;
}

// Función para aplicar transformaciones a los datos
void transform_data(DataParameters *params) {
    if (params->apply_log) {
        for (int i = 0; i < params->n_points; i++) {
            if (params->data[i] > 0) {
                params->data[i] = log(params->data[i]);
            }
        }
    }

    // Aplicar diferencias regulares
    for (int diff = 0; diff < params->d; diff++) {
        for (int i = 1; i < params->n_points - diff; i++) {
            params->data[i - 1] = params->data[i] - params->data[i - 1];
        }
        params->n_points--;
    }

    // Aplicar diferencias estacionales
    for (int diff = 0; diff < params->D; diff++) {
        for (int i = params->s; i < params->n_points - diff * params->s; i++) {
            params->data[i - params->s] = params->data[i] - params->data[i - params->s];
        }
        params->n_points -= params->s;
    }
}

// Función para calcular ACF muestral (MEJORADA)
void calcular_ACF_muestral(double *data, int n, double *acf, int lags) {
    if (n <= 0 || lags <= 0 || !data || !acf) return;

    // Calcular media
    double mean = 0.0;
    for (int i = 0; i < n; i++) {
        mean += data[i];
    }
    mean /= n;

    // Calcular varianza
    double variance = 0.0;
    for (int i = 0; i < n; i++) {
        double diff = data[i] - mean;
        variance += diff * diff;
    }
    variance /= n;

    // Calcular ACF para cada lag
    acf[0] = 1.0; // ACF en lag 0 es siempre 1

    for (int k = 1; k <= lags; k++) {
        if (k >= n) {
            acf[k] = 0.0;
            continue;
        }

        double cov = 0.0;
        for (int i = 0; i < n - k; i++) {
            cov += (data[i] - mean) * (data[i + k] - mean);
        }

        if (variance > 1e-10) {
            acf[k] = (cov / (n - k)) / variance;
        } else {
            acf[k] = 0.0;
        }

        // Asegurar que esté en el rango [-1, 1]
        if (acf[k] > 1.0) acf[k] = 1.0;
        if (acf[k] < -1.0) acf[k] = -1.0;
    }
}

// Función para calcular PACF muestral usando algoritmo de Durbin-Levinson
void calcular_PACF_muestral(double *acf, double *pacf, int lags) {
    if (lags <= 0) return;

    double *phi = malloc((lags + 1) * sizeof(double));
    double *phi_old = malloc((lags + 1) * sizeof(double));

    // Inicializar
    for (int i = 0; i <= lags; i++) {
        phi[i] = phi_old[i] = 0.0;
    }

    pacf[0] = 1.0;
    if (lags >= 1) {
        pacf[1] = acf[1];
        phi_old[1] = acf[1];
    }

    for (int n = 2; n <= lags; n++) {
        double num = acf[n];
        double den = 1.0;

        for (int j = 1; j < n; j++) {
            num -= phi_old[j] * acf[n - j];
            den -= phi_old[j] * acf[j];
        }

        if (fabs(den) < 1e-10) {
            phi[n] = 0.0;
        } else {
            phi[n] = num / den;
        }

        pacf[n] = phi[n];

        for (int j = 1; j < n; j++) {
            phi[j] = phi_old[j] - phi[n] * phi_old[n - j];
        }

        for (int j = 1; j <= n; j++) {
            phi_old[j] = phi[j];
        }
    }

    free(phi);
    free(phi_old);
}

/**
 * @brief Extrae características de patrones ACF/PACF para comparación de modelos
 *
 * Esta función analiza las funciones de autocorrelación (ACF) y autocorrelación parcial (PACF)
 * para extraer características que distinguen entre modelos AR, MA, ARMA y componentes estacionales.
 * Incluye detección mejorada de patrones estacionales tanto en ACF como en PACF.
 *
 * @param acf Array de autocorrelaciones (debe tener lags+1 elementos)
 * @param pacf Array de autocorrelaciones parciales (debe tener lags+1 elementos)
 * @param lags Número de lags a analizar
 * @param s Período estacional (s=1 para no estacional)
 * @param features Estructura donde almacenar las características extraídas
 */
void extract_pattern_features(double *acf, double *pacf, int lags, int s, PatternFeatures *features) {
    double threshold = 1.96 / sqrt(200); // Umbral de significancia para n~200

    // Inicializar todas las características
    features->acf_cutting_lag = 0;
    features->pacf_cutting_lag = 0;
    features->acf_decay_rate = 0.0;
    features->pacf_decay_rate = 0.0;
    features->acf_initial_spikes = 0;
    features->pacf_initial_spikes = 0;
    features->mixed_pattern_score = 0.0;
    features->parsimony_penalty = 0.0;

    // Inicializar características estacionales mejoradas
    features->seasonal_acf_strength = 0.0;
    features->seasonal_pacf_strength = 0.0;
    features->seasonal_acf_peaks = 0;
    features->seasonal_pacf_peaks = 0;

    // 1. DETECCIÓN DE CORTES (MA y AR puros)
    int acf_cut_found = 0;
    int pacf_cut_found = 0;

    for (int i = 1; i <= lags; i++) {
        // Corte en ACF (indicativo de MA) - requiere 3 lags consecutivos no significativos
        if (!acf_cut_found && fabs(acf[i]) < threshold) {
            if (i+1 <= lags && fabs(acf[i+1]) < threshold &&
                i+2 <= lags && fabs(acf[i+2]) < threshold) {
                features->acf_cutting_lag = i;
                acf_cut_found = 1;
            }
        }

        // Corte en PACF (indicativo de AR) - mismo criterio
        if (!pacf_cut_found && fabs(pacf[i]) < threshold) {
            if (i+1 <= lags && fabs(pacf[i+1]) < threshold &&
                i+2 <= lags && fabs(pacf[i+2]) < threshold) {
                features->pacf_cutting_lag = i;
                pacf_cut_found = 1;
            }
        }

        if (acf_cut_found && pacf_cut_found) break;
    }

    // 2. TASAS DE DECAIMIENTO SEPARADAS (primeros 8 lags)
    if (lags >= 3) {
        double acf_decay_sum = 0, pacf_decay_sum = 0;
        double acf_decay_weights = 0, pacf_decay_weights = 0;

        for (int i = 2; i <= MIN(8, lags); i++) {
            if (fabs(acf[i-1]) > 1e-6) {
                double weight = geometric_weight(i, 0.9);
                acf_decay_sum += fabs(acf[i] / acf[i-1]) * weight;
                acf_decay_weights += weight;
            }
            if (fabs(pacf[i-1]) > 1e-6) {
                double weight = geometric_weight(i, 0.9);
                pacf_decay_sum += fabs(pacf[i] / pacf[i-1]) * weight;
                pacf_decay_weights += weight;
            }
        }

        features->acf_decay_rate = (acf_decay_weights > 0) ? acf_decay_sum / acf_decay_weights : 0.0;
        features->pacf_decay_rate = (pacf_decay_weights > 0) ? pacf_decay_sum / pacf_decay_weights : 0.0;
        features->decay_rate = (features->acf_decay_rate + features->pacf_decay_rate) / 2.0;
    }

    // 3. CONTEO DE PICOS INICIALES (primeros 5 lags)
    double initial_threshold = threshold * 1.2;
    for (int i = 1; i <= MIN(5, lags); i++) {
        if (fabs(acf[i]) > initial_threshold) features->acf_initial_spikes++;
        if (fabs(pacf[i]) > initial_threshold) features->pacf_initial_spikes++;
    }

    // 4. PUNTUACIÓN DE PATRÓN MIXTO (ARMA)
    int mixed_signals = 0;
    if (features->acf_cutting_lag == 0 && features->pacf_cutting_lag == 0) mixed_signals++;
    if (features->acf_decay_rate > 0.3 && features->pacf_decay_rate > 0.3) mixed_signals++;
    if (features->acf_initial_spikes > 0 && features->pacf_initial_spikes > 0) mixed_signals++;
    features->mixed_pattern_score = mixed_signals / 3.0;

    // 5. CARACTERÍSTICAS ORIGINALES (compatibilidad)
    features->cutting_off_lag = MIN(features->acf_cutting_lag, features->pacf_cutting_lag);
    if (features->cutting_off_lag == 0) {
        features->cutting_off_lag = MAX(features->acf_cutting_lag, features->pacf_cutting_lag);
    }

    for (int i = 0; i < 3; i++) {
        if ((i+1) <= lags) {
            features->peak_values[i] = pacf[i+1];
        } else {
            features->peak_values[i] = 0.0;
        }
    }

    features->significant_lags = 0;
    for (int i = 1; i <= lags; i++) {
        if (fabs(acf[i]) > threshold || fabs(pacf[i]) > threshold) {
            features->significant_lags++;
        }
    }

    // 6. ANÁLISIS ESTACIONAL MEJORADO (ACF y PACF)
    if (s > 1) {
        double seasonal_acf_strength = 0.0;
        double seasonal_pacf_strength = 0.0;
        double seasonal_count = 0.0;

        for (int i = 1; i * s <= lags && i <= 5; i++) {
            int main_lag = i * s;
            seasonal_count++;

            // Fuerza estacional en ACF (MA estacional)
            seasonal_acf_strength += fabs(acf[main_lag]);
            if (fabs(acf[main_lag]) > threshold * 1.5) {
                features->seasonal_acf_peaks++;
            }

            // Fuerza estacional en PACF (AR estacional)
            seasonal_pacf_strength += fabs(pacf[main_lag]);
            if (fabs(pacf[main_lag]) > threshold * 1.5) {
                features->seasonal_pacf_peaks++;
            }

            // Satélites estacionales (peso reducido)
            if (main_lag - 1 >= 1 && main_lag - 1 <= lags) {
                seasonal_acf_strength += fabs(acf[main_lag-1]) * 0.3;
                seasonal_pacf_strength += fabs(pacf[main_lag-1]) * 0.3;
            }
            if (main_lag + 1 <= lags) {
                seasonal_acf_strength += fabs(acf[main_lag+1]) * 0.3;
                seasonal_pacf_strength += fabs(pacf[main_lag+1]) * 0.3;
            }
        }

        if (seasonal_count > 0) {
            features->seasonal_acf_strength = seasonal_acf_strength / seasonal_count;
            features->seasonal_pacf_strength = seasonal_pacf_strength / seasonal_count;
            features->seasonal_pattern = (features->seasonal_acf_strength + features->seasonal_pacf_strength) / 2.0;
        }
    } else {
        features->seasonal_pattern = 0.0;
    }

    // 7. OSCILACIÓN (original)
    int sign_changes = 0;
    double oscillation_weight = 0.0;
    for (int i = 2; i <= MIN(10, lags); i++) {
        if (acf[i] * acf[i-1] < 0) {
            double weight = geometric_weight(i, 0.85);
            sign_changes++;
            oscillation_weight += weight;
        }
    }
    features->oscillation_freq = (lags > 1 && oscillation_weight > 0) ?
                               (double)sign_changes / oscillation_weight : 0.0;
}


/**
 * @brief Evalúa la similitud de un modelo SARIMA con el patrón empírico aplicando principio de parsimonia
 *
 * Esta función realiza una evaluación completa de un modelo SARIMA candidato:
 * 1. Verifica estabilidad e invertibilidad de los polinomios
 * 2. Calcula ACF/PACF teóricas del modelo
 * 3. Extrae características de patrones
 * 4. Calcula similitud con patrón empírico
 * 5. Aplica penalización por complejidad (principio de parsimonia)
 * 6. Ajusta puntuación considerando evidencia estacional
 *
 * @param p Orden AR regular
 * @param phi Coeficientes AR regulares
 * @param q Orden MA regular
 * @param theta Coeficientes MA regulares
 * @param P Orden AR estacional
 * @param Phi Coeficientes AR estacionales
 * @param Q Orden MA estacional
 * @param Theta Coeficientes MA estacionales
 * @param s Período estacional
 * @param empirical_features Características del patrón empírico
 * @param lags Número de lags a comparar
 * @param n Tamaño de muestra (para penalización)
 * @return double Puntuación de similitud ajustada por parsimonia
 */
double evaluate_model_similarity(int p, double *phi, int q, double *theta,
                                int P, double *Phi, int Q, double *Theta,
                                int s, PatternFeatures *empirical_features, int lags, int n) {

    // Validación de parámetros
    if ((p > 0 && (!phi || p > 10)) || (q > 0 && (!theta || q > 10)) ||
        (P > 0 && (!Phi || P > 5)) || (Q > 0 && (!Theta || Q > 5))) {
        return 0.0;
    }

    if (lags <= 0 || lags > MAX_LAGS) {
        return 0.0;
    }

    // VERIFICACIÓN DE ESTABILIDAD E INVERTIBILIDAD
    if (p > 0 && !check_ar_roots(phi, p)) return 0.0;
    if (P > 0 && !check_ar_roots(Phi, P)) return 0.0;
    if (q > 0 && !check_ma_roots(theta, q)) return 0.0;
    if (Q > 0 && !check_ma_roots(Theta, Q)) return 0.0;

    // Cálculo de ACF/PACF teóricas
    double acf_theoretical[MAX_LAGS + 1] = {0};
    double pacf_theoretical[MAX_LAGS + 1] = {0};

    calcular_ACF_PACF_SARIMA(p, phi, q, theta, P, Phi, Q, Theta,
                            s, acf_theoretical, pacf_theoretical, lags);

    PatternFeatures theoretical_features;
    extract_pattern_features(acf_theoretical, pacf_theoretical, lags, s, &theoretical_features);

    // Configurar arrays para comparación
    theoretical_features.acf_values = acf_theoretical;
    theoretical_features.pacf_values = pacf_theoretical;
    empirical_features->acf_values = empirical_features->acf_values;
    empirical_features->pacf_values = empirical_features->pacf_values;

    // Calcular similitudes euclídeas
    calculate_euclidean_similarities(&theoretical_features, empirical_features, lags, s);

    // Calcular similitud de patrones
    double similarity = pattern_similarity(&theoretical_features, empirical_features, s, lags);

    // APLICAR PRINCIPIO DE PARSIMONIA MEJORADO
    int total_params = p + q + P + Q;
    double parsimony_penalty = 0.0;

    if (total_params > 0) {
        // Penalización base más agresiva
        parsimony_penalty = 0.03 + (total_params * 0.015);

        // Penalización EXTRA para modelos ARMA estacionales simultáneos
        if (P > 0 && Q > 0) {
            parsimony_penalty += 0.12;
        }

        // Penalización progresiva por complejidad
        if (total_params > 4) parsimony_penalty += 0.08;
        if (total_params > 6) parsimony_penalty += 0.12;
        if (total_params > 8) parsimony_penalty += 0.20;
    }

    double final_similarity = similarity - parsimony_penalty;
    if (final_similarity < 0) final_similarity = 0.0;

    // BONIFICACIÓN PARA MODELOS SIMPLES CON BUEN AJUSTE
    if (total_params <= 3 && similarity > 0.6) {
        final_similarity += 0.05;
        if (final_similarity > 1.0) final_similarity = 1.0;
    }

    // AJUSTE BASADO EN EVIDENCIA ESTACIONAL
    if (s > 1) {
        // Bonificar modelos estacionales cuando hay evidencia empírica
        if ((Q > 0 && empirical_features->seasonal_acf_strength > 0.2) ||
            (P > 0 && empirical_features->seasonal_pacf_strength > 0.2)) {
            final_similarity += 0.03;
        }

        // Penalizar modelos estacionales sin evidencia
        if ((P > 0 || Q > 0) && empirical_features->seasonal_pattern < 0.1) {
            final_similarity -= 0.08;
        }

        if (final_similarity > 1.0) final_similarity = 1.0;
        if (final_similarity < 0.0) final_similarity = 0.0;
    }

    return final_similarity;
}

// Algoritmo adaptativo de búsqueda en grid con gráficos en vivo

/**
 * @brief Adaptive grid search for best ARMA/SARIMA model using pattern similarity.
 *        Optionally re‑ranks candidates using Mahalanobis distance on feature vectors.
 *
 * This function performs a two‑stage search:
 * 1. Coarse grid (step 0.30) over effective orders (derived from empirical ACF/PACF).
 * 2. Fine refinement (step 0.10) around the best coarse parameters.
 * If `use_mahalanobis` is non‑zero, all evaluated models are stored and later
 * re‑evaluated using a combination of the original similarity and a Mahalanobis
 * distance computed from the empirical feature vector.
 *
 * @param empirical_data  Pointer to the (already transformed) time series data.
 * @param n_data          Number of observations in `empirical_data`.
 * @param s               Seasonal period (1 for non‑seasonal).
 * @param p_max           Maximum regular AR order to consider.
 * @param q_max           Maximum regular MA order to consider.
 * @param P_max           Maximum seasonal AR order to consider.
 * @param Q_max           Maximum seasonal MA order to consider (capped to 1 internally).
 * @param best_candidate  Structure where the best model found will be stored.
 * @param use_mahalanobis If non‑zero, enable Mahalanobis re‑ranking at the end.
 */

/* Construye el shortlist Box-Jenkins a partir de la predicción del MLP:
 * producto cartesiano de los 2 mejores p y los 2 mejores q (P,Q en el argmax),
 * ordenado por probabilidad conjunta aproximada. El verdadero modelo suele caer
 * en el top-2 de cada cabeza aunque no sea el argmax, igual que un analista BJ
 * propone varios modelos tentativos. */
static void build_mlp_shortlist(const MLPPrediction *pred, int ep, int eq,
                                int p_max, int q_max,
                                int P_max, int Q_max, ModelCandidate *best) {
    // Dos mejores p
    int p1 = pred->orders[0], p2 = -1; double bp2 = -1.0;
    for (int i = 0; i < MLP_NUM_p; i++)
        if (i != p1 && pred->prob_p[i] > bp2) { bp2 = pred->prob_p[i]; p2 = i; }
    // Dos mejores q
    int q1 = pred->orders[1], q2 = -1; double bq2 = -1.0;
    for (int i = 0; i < MLP_NUM_q; i++)
        if (i != q1 && pred->prob_q[i] > bq2) { bq2 = pred->prob_q[i]; q2 = i; }

    int P0 = pred->orders[2]; if (P0 > P_max) P0 = P_max; if (P0 < 0) P0 = 0;
    int Q0 = pred->orders[3]; if (Q0 > Q_max) Q0 = Q_max; if (Q0 < 0) Q0 = 0;

    OrderCandidate tmp[12]; int nt = 0;
    // Candidatos tentativos (filosofía Box-Jenkins), en orden de prioridad:
    //  - Top-2 p × top-2 q del MLP (mezclas ARMA)
    //  - Reducciones puras del MLP: (p1,0) AR puro, (0,q1) MA puro
    //  - Lectura CLÁSICA del correlograma empírico: corte de PACF → AR(ep) puro,
    //    corte de ACF → MA(eq) puro, y la mezcla (ep,eq). Esto refuerza la
    //    identificación AR, donde el MLP es débil.
    int cp_pairs[9][2] = {
        {p1,q1},{p1,q2},{p2,q1},{p2,q2},
        {p1,0},{0,q1},
        {ep,0},{0,eq},{ep,eq}
    };
    for (int idx = 0; idx < 9; idx++) {
        int pp = cp_pairs[idx][0], qq = cp_pairs[idx][1];
        if (pp < 0 || pp > p_max || qq < 0 || qq > q_max) continue;
        int dup = 0;
        for (int k = 0; k < nt; k++) if (tmp[k].p == pp && tmp[k].q == qq) dup = 1;
        if (dup) continue;
        tmp[nt].p = pp; tmp[nt].q = qq; tmp[nt].P = P0; tmp[nt].Q = Q0;
        tmp[nt].prob = pred->prob_p[pp < MLP_NUM_p ? pp : MLP_NUM_p-1]
                     * pred->prob_q[qq < MLP_NUM_q ? qq : MLP_NUM_q-1]
                     * pred->prob_P[P0] * pred->prob_Q[Q0];
        nt++;
    }
    // Fallback: si ningún (p,q) del top-2 cae dentro del rango permitido, incluir el
    // argmax recortado al rango (el shortlist nunca debe quedar vacío).
    if (nt == 0) {
        int pp = p1 > p_max ? p_max : (p1 < 0 ? 0 : p1);
        int qq = q1 > q_max ? q_max : (q1 < 0 ? 0 : q1);
        tmp[nt].p = pp; tmp[nt].q = qq; tmp[nt].P = P0; tmp[nt].Q = Q0;
        tmp[nt].prob = pred->prob_p[pp] * pred->prob_q[qq]
                     * pred->prob_P[P0] * pred->prob_Q[Q0];
        nt++;
    }

    // Orden descendente por probabilidad
    for (int i = 0; i < nt; i++)
        for (int j = i + 1; j < nt; j++)
            if (tmp[j].prob > tmp[i].prob) { OrderCandidate t = tmp[i]; tmp[i] = tmp[j]; tmp[j] = t; }

    int kmax = nt < MAX_ORDER_CANDIDATES ? nt : MAX_ORDER_CANDIDATES;
    for (int i = 0; i < kmax; i++) best->candidates[i] = tmp[i];
    best->n_candidates = kmax;
}

void adaptive_grid_search(double *empirical_data, int n_data, int s,
                         int p_max, int q_max, int P_max, int Q_max,
                         ModelCandidate *best_candidate,
                         int use_mahalanobis) {
    if (!empirical_data || n_data <= 0 || !best_candidate) return;

    double acf_empirical[MAX_LAGS + 1] = {0};
    double pacf_empirical[MAX_LAGS + 1] = {0};

    // Restrict seasonal MA order to 1 (see MAX_SEASONAL_MA_ORDER)
    if (Q_max > MAX_SEASONAL_MA_ORDER) {
        printf("WARNING: Q_max reduced from %d to %d\n", Q_max, MAX_SEASONAL_MA_ORDER);
        Q_max = MAX_SEASONAL_MA_ORDER;
    }

    // Determine number of lags for ACF/PACF
    int lags = MIN(MAX_LAGS, n_data / 4);
    if (lags < 10) lags = 10;
    if (lags > MAX_LAGS) lags = MAX_LAGS;

    // Empirical ACF and PACF
    calcular_ACF_muestral(empirical_data, n_data, acf_empirical, lags);
    calcular_PACF_muestral(acf_empirical, pacf_empirical, lags);

    // Extract empirical pattern features (will also be used for Mahalanobis)
    PatternFeatures empirical_features;
    extract_pattern_features(acf_empirical, pacf_empirical, lags, s, &empirical_features);
    empirical_features.acf_values = acf_empirical;
    empirical_features.pacf_values = pacf_empirical;

    best_candidate->similarity = 0.0;

    // =========================================================================
    // MLP-BASED ORDER IDENTIFICATION (replaces heuristic effective_orders)
    // =========================================================================
    double feature_vector[MAHALANOBIS_FEATURE_DIM];
    extract_feature_vector(&empirical_features, feature_vector, MAHALANOBIS_FEATURE_DIM, s, lags);

    MLPPrediction mlp_pred;
    int mlp_ok = mlp_predict(feature_vector, &mlp_pred);

    if (mlp_ok == 0) {
        printf("MLP predicted orders: p=%d (prob=%.3f), q=%d (prob=%.3f), "
               "P=%d (prob=%.3f), Q=%d (prob=%.3f)  conf=%.4f\n",
               mlp_pred.orders[0], mlp_pred.prob_p[mlp_pred.orders[0]],
               mlp_pred.orders[1], mlp_pred.prob_q[mlp_pred.orders[1]],
               mlp_pred.orders[2], mlp_pred.prob_P[mlp_pred.orders[2]],
               mlp_pred.orders[3], mlp_pred.prob_Q[mlp_pred.orders[3]],
               mlp_pred.confidence);
    } else {
        printf("MLP failed, falling back to heuristic effective_orders.\n");
    }

    // =========================================================================
    // SALIDA DE IDENTIFICACIÓN BOX-JENKINS (shortlist de modelos tentativos)
    // El MLP lee la ACF/PACF como un analista BJ y propone órdenes. Construimos
    // siempre el shortlist; en modo mlp_direct ES la salida (sin parsimonia).
    // atsw-MCP estima y elige el modelo final entre los candidatos.
    // =========================================================================
    if (mlp_ok == 0) {
        // Lectura clásica del correlograma empírico (corte de PACF/ACF) para reforzar
        // la identificación AR/MA pura, donde el MLP es débil.
        int ep_c, eq_c, eP_c, eQ_c;
        determine_effective_orders(acf_empirical, pacf_empirical, lags, n_data, s,
                                   &ep_c, &eq_c, &eP_c, &eQ_c);
        build_mlp_shortlist(&mlp_pred, ep_c, eq_c, p_max, q_max, P_max, Q_max, best_candidate);
        // Identificación de órdenes mixtos (rejilla ARMA + AICc) para no perder
        // modelos como ARMA(2,1) que el MLP/cortes leen como AR.
        add_arma_grid_candidates(best_candidate, empirical_data, n_data,
                                 acf_empirical, lags, s, p_max, q_max);
        // Identificación estacional: cruza bases regulares con hipótesis (P,Q) y deja
        // que el AICc resuelva la ambigüedad SAR<->SMA del MLP.
        add_seasonal_grid_candidates(best_candidate, empirical_data, n_data,
                                     acf_empirical, lags, s, P_max, Q_max);
        // Cierre del lazo: estimación ligera + puntuación por similitud, reordena el
        // shortlist para que el top-k sea el conjunto que el atsw-MCP elegiría.
        rank_shortlist_by_fit(best_candidate, empirical_data, n_data,
                              acf_empirical, pacf_empirical, &empirical_features, lags, s);
        printf("MLP shortlist (%d):", best_candidate->n_candidates);
        for (int i = 0; i < best_candidate->n_candidates; i++)
            printf(" (%d,%d)(%d,%d) p=%.3f",
                   best_candidate->candidates[i].p, best_candidate->candidates[i].q,
                   best_candidate->candidates[i].P, best_candidate->candidates[i].Q,
                   best_candidate->candidates[i].prob);
        printf("\n");
    }

    if (g_mlp_direct && mlp_ok == 0 && best_candidate->n_candidates > 0) {
        // Modo identificación pura: salida = argmax del MLP + shortlist; NO grid search,
        // NO penalización de parsimonia. Coeficientes provisionales (atsw-MCP re-estima).
        OrderCandidate top = best_candidate->candidates[0];
        best_candidate->p = top.p; best_candidate->q = top.q;
        best_candidate->P = top.P; best_candidate->Q = top.Q;
        best_candidate->similarity = top.prob;

        if (top.p > 0) {
            best_candidate->best_phi = (double*)calloc(top.p, sizeof(double));
            estimate_ar_yule_walker(acf_empirical, top.p, best_candidate->best_phi);
        }
        if (top.q > 0) {
            best_candidate->best_theta = (double*)calloc(top.q, sizeof(double));
            for (int i = 0; i < top.q; i++) best_candidate->best_theta[i] = 0.3 / (i + 1);
        }
        if (top.P > 0) {
            double acf_seasonal[16]; acf_seasonal[0] = 1.0;
            for (int i = 1; i <= top.P && i < 16; i++) {
                int lag = i * s;
                acf_seasonal[i] = (lag <= lags) ? acf_empirical[lag] : 0.0;
            }
            best_candidate->best_Phi = (double*)calloc(top.P, sizeof(double));
            estimate_ar_yule_walker(acf_seasonal, top.P, best_candidate->best_Phi);
        }
        if (top.Q > 0) {
            best_candidate->best_Theta = (double*)calloc(top.Q, sizeof(double));
            for (int i = 0; i < top.Q; i++) best_candidate->best_Theta[i] = MIN_SEASONAL_MA_COEF;
        }

        best_candidate->lags_used = lags;
        best_candidate->acf_empirical = (double*)malloc((lags + 1) * sizeof(double));
        best_candidate->pacf_empirical = (double*)malloc((lags + 1) * sizeof(double));
        if (best_candidate->acf_empirical && best_candidate->pacf_empirical) {
            memcpy(best_candidate->acf_empirical, acf_empirical, (lags + 1) * sizeof(double));
            memcpy(best_candidate->pacf_empirical, pacf_empirical, (lags + 1) * sizeof(double));
        }
        printf("MLP-direct identification: (%d,%d)(%d,%d)s=%d prob=%.4f\n",
               top.p, top.q, top.P, top.Q, s, top.prob);
        report_progress_internal(1, 1.0, "Identification completed (MLP-direct)", "Process finished");
        return;
    }

    // Narrow search to MLP top-1 ± 1 (or keep effective_orders if MLP failed)
    int p_start, p_end, q_start, q_end, P_start, P_end, Q_start, Q_end;

    if (mlp_ok == 0 && mlp_pred.confidence > 0.03) {
        // MLP-guided: top probabilities above 0.10, always include 0, at least ±2
        double prob_threshold = 0.10;

        // Determine range from probability distribution
        int p_min_mlp = p_max, p_max_mlp = 0;
        for (int i = 0; i <= p_max && i < MLP_NUM_p; i++) {
            if (mlp_pred.prob_p[i] >= prob_threshold) {
                if (i < p_min_mlp) p_min_mlp = i;
                if (i > p_max_mlp) p_max_mlp = i;
            }
        }
        // MLP-guided but always explore at least [0, min(3, p_max)]
        int cp = mlp_pred.orders[0]; if (cp > p_max) cp = p_max;
        p_start = 0;  // Always include p=0
        p_end   = MIN(p_max, MAX(MAX(p_max_mlp, cp + 2), MIN(3, p_max)));

        int q_min_mlp = q_max, q_max_mlp = 0;
        for (int i = 0; i <= q_max && i < MLP_NUM_q; i++) {
            if (mlp_pred.prob_q[i] >= prob_threshold) {
                if (i < q_min_mlp) q_min_mlp = i;
                if (i > q_max_mlp) q_max_mlp = i;
            }
        }
        int cq = mlp_pred.orders[1]; if (cq > q_max) cq = q_max;
        q_start = 0;  // Always include q=0
        q_end   = MIN(q_max, MAX(MAX(q_max_mlp, cq + 2), MIN(3, q_max)));

        // For seasonal orders, always include 0, ±2 around top
        int cP = mlp_pred.orders[2]; if (cP > P_max) cP = P_max;
        int cQ = mlp_pred.orders[3]; if (cQ > Q_max) cQ = Q_max;
        P_start = 0;  // seasonal 0 is always plausible
        P_end   = MIN(P_max, cP + 2);
        Q_start = 0;
        Q_end   = MIN(Q_max, cQ + 2);
        printf("MLP-guided search space: p=[%d,%d] q=[%d,%d] P=[%d,%d] Q=[%d,%d]\n",
               p_start, p_end, q_start, q_end, P_start, P_end, Q_start, Q_end);
    } else {
        // Fallback: use effective orders based on empirical significance
        int ep, eq, eP, eQ;
        determine_effective_orders(acf_empirical, pacf_empirical, lags, n_data, s,
                                   &ep, &eq, &eP, &eQ);
        p_start = 0;  p_end = MIN(ep, p_max);
        q_start = 0;  q_end = MIN(eq, q_max);
        P_start = 0;  P_end = MIN(eP, P_max);
        Q_start = 0;  Q_end = MIN(eQ, Q_max);
        printf("Heuristic effective orders: p=[%d,%d] q=[%d,%d] P=[%d,%d] Q=[%d,%d]\n",
               p_start, p_end, q_start, q_end, P_start, P_end, Q_start, Q_end);
    }

    int total_models = (p_end - p_start + 1) * (q_end - q_start + 1) *
                       (P_end - P_start + 1) * (Q_end - Q_start + 1);

    if (total_models <= 0) {
        printf("WARNING: No valid models to evaluate.\n");
        return;
    }

    int current_model = 0;
    report_progress_internal(1, 0.0, "Starting MLP-guided grid search...",
                             "Stage 1: Coarse Grid Search (step 0.30)");

    int plot_counter = 0;
    const int PLOT_INTERVAL = 8;
    double significance_threshold = 1.96 / sqrt(n_data);

    // ------------------------------------------------------------------
    //  Stage 1: Coarse grid search over MLP-guided orders
    // ------------------------------------------------------------------
    for (int p = p_start; p <= p_end; p++) {
        if (!validate_ar_pattern(p, pacf_empirical, lags, significance_threshold)) {
            printf("Skipping AR(%d) - invalid pattern in empirical PACF\n", p);
            continue;
        }
            for (int q = q_start; q <= q_end; q++) {
            if (!validate_ma_pattern(q, acf_empirical, lags, significance_threshold)) {
                printf("Skipping MA(%d) - invalid pattern in empirical ACF\n", q);
                continue;
            }
            for (int P = P_start; P <= P_end; P++) {
                for (int Q = Q_start; Q <= Q_end; Q++) {
                    if (p == 0 && q == 0 && P == 0 && Q == 0) continue;

                    current_model++;
                    double progress = (double)current_model / total_models;
                    char message[100], overall[100];
                    snprintf(message, sizeof(message),
                             "Evaluating %d/%d: p=%d, q=%d, P=%d, Q=%d",
                             current_model, total_models, p, q, P, Q);
                    snprintf(overall, sizeof(overall),
                             "Stage 1: %.1f%% completed", progress * 100);
                    report_progress_internal(1, progress, message, overall);

                    // For high‑order models, use default coefficients to avoid combinatorial explosion
                    if (p + q + P + Q > 10) {
                        double phi[10] = {0}, theta[10] = {0}, Phi[5] = {0}, Theta[5] = {0};
                        for (int i = 0; i < p; i++) phi[i] = 0.5 / (i + 1);
                        for (int i = 0; i < q; i++) theta[i] = 0.3 / (i + 1);
                        for (int i = 0; i < P; i++) Phi[i] = 0.4 / (i + 1);
                        for (int i = 0; i < Q; i++) Theta[i] = 0.2 / (i + 1);
                        // Seasonal MA coefficients forced positive
                        for (int i = 0; i < Q; i++) {
                            Theta[i] = MIN_SEASONAL_MA_COEF + i * 0.1;
                            if (Theta[i] > MAX_SEASONAL_MA_COEF) Theta[i] = MAX_SEASONAL_MA_COEF;
                        }

                        // Compute theoretical ACF/PACF
                        double acf_theoretical[MAX_LAGS + 1], pacf_theoretical[MAX_LAGS + 1];
                        calcular_ACF_PACF_SARIMA(p, phi, q, theta, P, Phi, Q, Theta,
                                                s, acf_theoretical, pacf_theoretical, lags);

                        double similarity = evaluate_model_similarity(p, phi, q, theta,
                                                                     P, Phi, Q, Theta,
                                                                     s, &empirical_features,
                                                                     lags, n_data);

                        // Update best candidate (original similarity)
                        if (similarity > best_candidate->similarity) {
                            best_candidate->p = p; best_candidate->q = q;
                            best_candidate->P = P; best_candidate->Q = Q;
                            best_candidate->similarity = similarity;
                            liberar_model_candidate(best_candidate);
                            // Copy coefficients into best_candidate
                            if (p > 0) {
                                best_candidate->best_phi = (double*)malloc(p * sizeof(double));
                                memcpy(best_candidate->best_phi, phi, p * sizeof(double));
                            }
                            if (q > 0) {
                                best_candidate->best_theta = (double*)malloc(q * sizeof(double));
                                memcpy(best_candidate->best_theta, theta, q * sizeof(double));
                            }
                            if (P > 0) {
                                best_candidate->best_Phi = (double*)malloc(P * sizeof(double));
                                memcpy(best_candidate->best_Phi, Phi, P * sizeof(double));
                            }
                            if (Q > 0) {
                                best_candidate->best_Theta = (double*)malloc(Q * sizeof(double));
                                memcpy(best_candidate->best_Theta, Theta, Q * sizeof(double));
                            }

                            // Live plot for promising models
                            if (similarity > 0.4) {

                                // Calcular cmax y título para el gráfico en vivo
                            double cmax = 0.0;
                            for (int i = 1; i <= lags; i++) {
                                if (fabs(acf_empirical[i]) > cmax) cmax = fabs(acf_empirical[i]);
                                if (fabs(pacf_empirical[i]) > cmax) cmax = fabs(pacf_empirical[i]);
                                if (fabs(acf_theoretical[i]) > cmax) cmax = fabs(acf_theoretical[i]);
                                if (fabs(pacf_theoretical[i]) > cmax) cmax = fabs(pacf_theoretical[i]);
                                    }
                                cmax += 0.1;

                                char title[256];
                                if (P > 0 || Q > 0) {
                                    snprintf(title, sizeof(title),
                                    "BÚSQUEDA - SARIMA(%d,0,%d)(%d,0,%d)%d - Sim: %.3f",
                                        p, q, P, Q, s, similarity);
                                } else {
                                        snprintf(title, sizeof(title),
                                        "BÚSQUEDA - ARMA(%d,%d) - Sim: %.3f",
                                        p, q, similarity);
                                }

                                send_plot_data(acf_theoretical, pacf_theoretical,
                                            acf_empirical, pacf_empirical,
                                            lags, cmax, title);
                            }
                        }
                        continue;
                    }

                    // ---------- Yule-Walker fast path for pure AR (q=0, Q=0) ----------
                    if (q == 0 && Q == 0) {
                        int use_yw = 0;
                        double phi_yw[10] = {0}, Phi_yw[5] = {0};

                        if (p > 0) {
                            use_yw = estimate_ar_yule_walker(acf_empirical, p, phi_yw);
                        } else {
                            use_yw = 1; // p=0 is trivially satisfied
                        }

                        if (use_yw && P > 0) {
                            // Build seasonal-lag ACF for SAR(P)
                            double acf_seasonal[P + 1];
                            acf_seasonal[0] = 1.0;
                            for (int i = 1; i <= P; i++) {
                                int lag = i * s;
                                acf_seasonal[i] = (lag <= lags) ? acf_empirical[lag] : 0.0;
                            }
                            use_yw = estimate_ar_yule_walker(acf_seasonal, P, Phi_yw);
                        }

                        if (use_yw && (p > 0 || P > 0)) {
                            double sim = evaluate_model_similarity(p, phi_yw, 0, NULL,
                                                                       P, Phi_yw, 0, NULL,
                                                                       s, &empirical_features, lags, n_data);
                                if (sim > best_candidate->similarity) {
                                    best_candidate->p = p; best_candidate->q = 0;
                                    best_candidate->P = P; best_candidate->Q = 0;
                                    best_candidate->similarity = sim;
                                    liberar_model_candidate(best_candidate);
                                    if (p > 0) {
                                        best_candidate->best_phi = malloc(p * sizeof(double));
                                        memcpy(best_candidate->best_phi, phi_yw, p * sizeof(double));
                                    }
                                    if (P > 0) {
                                        best_candidate->best_Phi = malloc(P * sizeof(double));
                                        memcpy(best_candidate->best_Phi, Phi_yw, P * sizeof(double));
                                    }
                                    // Live plot
                                    double act[MAX_LAGS+1], pact[MAX_LAGS+1];
                                    calcular_ACF_PACF_SARIMA(p, phi_yw, 0, NULL, P, Phi_yw, 0, NULL,
                                                            s, act, pact, lags);
                                    double cmax = 0.0;
                                    for (int i = 1; i <= lags; i++) {
                                        if (fabs(acf_empirical[i]) > cmax) cmax = fabs(acf_empirical[i]);
                                        if (fabs(pacf_empirical[i]) > cmax) cmax = fabs(pacf_empirical[i]);
                                        if (fabs(act[i]) > cmax) cmax = fabs(act[i]);
                                        if (fabs(pact[i]) > cmax) cmax = fabs(pact[i]);
                                    }
                                    cmax += 0.1;
                                    char title[256];
                                    snprintf(title, sizeof(title), "YULE-WALKER AR(%d,%d) s=%d Sim=%.3f",
                                             p, P, s, sim);
                                    send_plot_data(act, pact, acf_empirical, pacf_empirical,
                                                  lags, cmax, title);
                                }
                                continue; // Skip grid search for this pure AR model
                        }
                    }

                    // ---------- Normal case (low orders) – coarse grid search ----------
                    int num_steps_coarse = (int)((GRID_MAX - GRID_MIN) / COARSE_GRID_STEP) + 1;
                    double best_similarity_coarse = 0.0;
                    double best_phi_coarse[10] = {0}, best_theta_coarse[10] = {0};
                    double best_Phi_coarse[5] = {0}, best_Theta_coarse[5] = {0};
                    int num_steps_seasonal_ma = (int)((MAX_SEASONAL_MA_COEF - MIN_SEASONAL_MA_COEF) / COARSE_GRID_STEP) + 1;

                    int total_coarse = (p > 0 ? num_steps_coarse : 1) *
                                       (q > 0 ? num_steps_coarse : 1) *
                                       (P > 0 ? num_steps_coarse : 1) *
                                       (Q > 0 ? num_steps_seasonal_ma : 1);
                    int current_coarse = 0;

                    for (int phi_idx = 0; phi_idx < (p > 0 ? num_steps_coarse : 1); phi_idx++) {
                        for (int theta_idx = 0; theta_idx < (q > 0 ? num_steps_coarse : 1); theta_idx++) {
                            for (int Phi_idx = 0; Phi_idx < (P > 0 ? num_steps_coarse : 1); Phi_idx++) {
                                for (int Theta_idx = 0; Theta_idx < (Q > 0 ? num_steps_seasonal_ma : 1); Theta_idx++) {
                                    current_coarse++;
                                    double coarse_progress = (double)current_coarse / total_coarse;
                                    char coarse_msg[100];
                                    snprintf(coarse_msg, sizeof(coarse_msg),
                                             "Coarse grid: %d/%d combos", current_coarse, total_coarse);
                                    report_progress_internal(1, progress + coarse_progress * 0.8 / total_models,
                                                           coarse_msg, NULL);

                                    double phi[10] = {0}, theta[10] = {0}, Phi[5] = {0}, Theta[5] = {0};
                                    for (int i = 0; i < p; i++)
                                        phi[i] = GRID_MIN + phi_idx * COARSE_GRID_STEP;
                                    for (int i = 0; i < q; i++)
                                        theta[i] = GRID_MIN + theta_idx * COARSE_GRID_STEP;
                                    for (int i = 0; i < P; i++)
                                        Phi[i] = GRID_MIN + Phi_idx * COARSE_GRID_STEP;
                                    for (int i = 0; i < Q; i++)
                                        Theta[i] = MIN_SEASONAL_MA_COEF + Theta_idx * COARSE_GRID_STEP;

                                    double sim = evaluate_model_similarity(p, phi, q, theta,
                                                                           P, Phi, Q, Theta,
                                                                           s, &empirical_features,
                                                                           lags, n_data);
                                    if (sim > best_similarity_coarse) {
                                        best_similarity_coarse = sim;
                                        memcpy(best_phi_coarse, phi, p * sizeof(double));
                                        memcpy(best_theta_coarse, theta, q * sizeof(double));
                                        memcpy(best_Phi_coarse, Phi, P * sizeof(double));
                                        memcpy(best_Theta_coarse, Theta, Q * sizeof(double));
                                    }
                                }
                            }
                        }
                    }

                    // ---------- Fine refinement ----------
                    report_progress_internal(2, 0.0, "Starting fine refinement...",
                                             "Stage 2: Fine Refinement (step 0.10)");
                    double best_similarity_fine = best_similarity_coarse;
                    double best_phi_fine[10], best_theta_fine[10], best_Phi_fine[5], best_Theta_fine[5];
                    memcpy(best_phi_fine, best_phi_coarse, p * sizeof(double));
                    memcpy(best_theta_fine, best_theta_coarse, q * sizeof(double));
                    memcpy(best_Phi_fine, best_Phi_coarse, P * sizeof(double));
                    memcpy(best_Theta_fine, best_Theta_coarse, Q * sizeof(double));

                    int total_ref = p + q + P + Q;
                    int cur_ref = 0;
                    for (int param_type = 0; param_type < 4; param_type++) {
                        int order = 0;
                        double *current_best = NULL, *best_refined = NULL;
                        switch (param_type) {
                            case 0: if (p == 0) continue; order = p; current_best = best_phi_coarse; best_refined = best_phi_fine; break;
                            case 1: if (q == 0) continue; order = q; current_best = best_theta_coarse; best_refined = best_theta_fine; break;
                            case 2: if (P == 0) continue; order = P; current_best = best_Phi_coarse; best_refined = best_Phi_fine; break;
                            case 3: if (Q == 0) continue; order = Q; current_best = best_Theta_coarse; best_refined = best_Theta_fine; break;
                        }
                        for (int coeff = 0; coeff < order; coeff++) {
                            cur_ref++;
                            double ref_prog = (double)cur_ref / total_ref;
                            char ref_msg[100], ref_overall[100];
                            snprintf(ref_msg, sizeof(ref_msg), "Refining param %d/%d", cur_ref, total_ref);
                            snprintf(ref_overall, sizeof(ref_overall), "Stage 2: %.1f%%", ref_prog * 100);
                            report_progress_internal(2, ref_prog, ref_msg, ref_overall);

                            double orig = current_best[coeff];
                            double best_val = orig;
                            double best_sim = best_similarity_fine;
                            for (double delta = -0.2; delta <= 0.2 + 1e-6; delta += FINE_GRID_STEP) {
                                double test = orig + delta;
                                if (param_type == 3) { // Theta (seasonal MA)
                                    if (test < MIN_SEASONAL_MA_COEF) test = MIN_SEASONAL_MA_COEF;
                                    if (test > MAX_SEASONAL_MA_COEF) test = MAX_SEASONAL_MA_COEF;
                                } else {
                                    if (test < GRID_MIN) test = GRID_MIN;
                                    if (test > GRID_MAX) test = GRID_MAX;
                                }
                                double phi_t[10] = {0}, theta_t[10] = {0}, Phi_t[5] = {0}, Theta_t[5] = {0};
                                memcpy(phi_t, best_phi_fine, p * sizeof(double));
                                memcpy(theta_t, best_theta_fine, q * sizeof(double));
                                memcpy(Phi_t, best_Phi_fine, P * sizeof(double));
                                memcpy(Theta_t, best_Theta_fine, Q * sizeof(double));
                                switch (param_type) {
                                    case 0: phi_t[coeff] = test; break;
                                    case 1: theta_t[coeff] = test; break;
                                    case 2: Phi_t[coeff] = test; break;
                                    case 3: Theta_t[coeff] = test; break;
                                }
                                double sim = evaluate_model_similarity(p, phi_t, q, theta_t,
                                                                       P, Phi_t, Q, Theta_t,
                                                                       s, &empirical_features,
                                                                       lags, n_data);
                                if (sim > best_sim) {
                                    best_sim = sim;
                                    best_val = test;
                                }
                            }
                            if (best_sim > best_similarity_fine) {
                                best_similarity_fine = best_sim;
                                best_refined[coeff] = best_val;
                            }
                        }
                    }

                    // Update global best candidate
                    if (best_similarity_fine > best_candidate->similarity) {
                        best_candidate->p = p; best_candidate->q = q;
                        best_candidate->P = P; best_candidate->Q = Q;
                        best_candidate->similarity = best_similarity_fine;
                        liberar_model_candidate(best_candidate);
                        if (p > 0) {
                            best_candidate->best_phi = (double*)malloc(p * sizeof(double));
                            memcpy(best_candidate->best_phi, best_phi_fine, p * sizeof(double));
                        }
                        if (q > 0) {
                            best_candidate->best_theta = (double*)malloc(q * sizeof(double));
                            memcpy(best_candidate->best_theta, best_theta_fine, q * sizeof(double));
                        }
                        if (P > 0) {
                            best_candidate->best_Phi = (double*)malloc(P * sizeof(double));
                            memcpy(best_candidate->best_Phi, best_Phi_fine, P * sizeof(double));
                        }
                        if (Q > 0) {
                            best_candidate->best_Theta = (double*)malloc(Q * sizeof(double));
                            memcpy(best_candidate->best_Theta, best_Theta_fine, Q * sizeof(double));
                        }

                        // Live plot for the best candidate (optional)
                        plot_counter++;
                        if (plot_counter % PLOT_INTERVAL == 0 || best_similarity_fine > 0.7) {
                            double acf_theoretical[MAX_LAGS + 1], pacf_theoretical[MAX_LAGS + 1];
                            calcular_ACF_PACF_SARIMA(p, best_phi_fine, q, best_theta_fine,
                                                    P, best_Phi_fine, Q, best_Theta_fine,
                                                    s, acf_theoretical, pacf_theoretical, lags);
                            double cmax = 0.0;
                            for (int i = 1; i <= lags; i++) {
                                if (fabs(acf_empirical[i]) > cmax) cmax = fabs(acf_empirical[i]);
                                if (fabs(pacf_empirical[i]) > cmax) cmax = fabs(pacf_empirical[i]);
                                if (fabs(acf_theoretical[i]) > cmax) cmax = fabs(acf_theoretical[i]);
                                if (fabs(pacf_theoretical[i]) > cmax) cmax = fabs(pacf_theoretical[i]);
                            }
                            cmax += 0.1;
                            char title[256];
                            if (P > 0 || Q > 0) {
                                snprintf(title, sizeof(title),
                                         "CURRENT BEST - SARIMA(%d,0,%d)(%d,0,%d)%d - Sim: %.3f",
                                         p, q, P, Q, s, best_similarity_fine);
                            } else {
                                snprintf(title, sizeof(title),
                                         "CURRENT BEST - ARMA(%d,%d) - Sim: %.3f",
                                         p, q, best_similarity_fine);
                            }
                            send_plot_data(acf_theoretical, pacf_theoretical,
                                           acf_empirical, pacf_empirical,
                                           lags, cmax, title);
                        }
                    }
                }
            }
        }
    }

    // Save empirical ACF/PACF for final plotting
    best_candidate->lags_used = lags;
    best_candidate->acf_empirical = (double*)malloc((lags + 1) * sizeof(double));
    best_candidate->pacf_empirical = (double*)malloc((lags + 1) * sizeof(double));
    if (best_candidate->acf_empirical && best_candidate->pacf_empirical) {
        memcpy(best_candidate->acf_empirical, acf_empirical, (lags + 1) * sizeof(double));
        memcpy(best_candidate->pacf_empirical, pacf_empirical, (lags + 1) * sizeof(double));
    }

    report_progress_internal(1, 1.0, "Search completed", "Process finished");
}

/**
 * @brief Main automatic detection function with seasonal integration.
 *
 * This function implements the complete automatic detection pipeline:
 * 1. Seasonal detection and adjustment (if not in simulation mode).
 * 2. Data loading and transformation (log, regular/seasonal differences).
 * 3. Adaptive grid search for the best ARMA/SARIMA model.
 * 4. Computation of theoretical ACF/PACF for the best model.
 * 5. Reporting results with seasonality information.
 *
 * In simulation mode (filename empty), the function assumes that
 * `params->data` already contains the time series and no file I/O is performed.
 * Seasonal detection and unit root tests are skipped.
 *
 * @param filename Path to data file (or empty string for simulation mode)
 * @param params   Data parameters (transformations, flags, etc.)
 * @param p_max    Maximum regular AR order
 * @param q_max    Maximum regular MA order
 * @param P_max    Maximum seasonal AR order
 * @param Q_max    Maximum seasonal MA order
 * @param best_candidate Structure to store the best model found
 * @return 1 on success, 0 on error
 */
int ejecutar_deteccion_automatica(const char *filename, DataParameters *params,
                                 int p_max, int q_max, int P_max, int Q_max,
                                 ModelCandidate *best_candidate) {
    if (!params || !best_candidate) return 0;

    // Modo identificación Box-Jenkins (salida = shortlist del MLP sin parsimonia)
    set_identification_mode(params->mlp_direct);

    // Determine if we are in simulation mode (no file to read)
    int is_simulation = (filename == NULL || filename[0] == '\0');

    // =====================================================================
    // OPTIONAL DESEASONALIZATION (only if enabled and NOT in simulation mode)
    // =====================================================================
    if (params->deseasonalize && !is_simulation) {
        printf("=== DESESTACIONALIZACIÓN ACTIVADA ===\n");
        printf("Se eliminará la componente estacional mediante regresión armónica.\n");
        printf("Diferencias estacionales (D) y MA estacional (Q) se forzarán a 0.\n");

        // Load original data from file
        double *datos_originales;
        int n_original;
        if (!load_data(filename, &datos_originales, &n_original)) {
            printf("Error: No se pudieron cargar los datos para desestacionalizar.\n");
            return 0;
        }

        // Apply natural logarithm if requested
        double *log_series = malloc(n_original * sizeof(double));
        if (params->apply_log) {
            for (int i = 0; i < n_original; i++) {
                if (datos_originales[i] <= 0) {
                    printf("Error: Non-positive value, cannot apply logarithm.\n");
                    free(datos_originales);
                    free(log_series);
                    return 0;
                }
                log_series[i] = log(datos_originales[i]);
            }
        } else {
            memcpy(log_series, datos_originales, n_original * sizeof(double));
        }

        // Obtain seasonal dummies (in percentage) via harmonic regression
        SeasonalDetectionResult *seasonal_res = detect_seasonality_harmonic_regression(
            filename, params->d, 1, params->s);
        if (!seasonal_res || !seasonal_res->seasonal_dummies) {
            printf("Error: Could not estimate seasonal dummies.\n");
            free(datos_originales);
            free(log_series);
            if (seasonal_res) free_seasonal_detection_result(seasonal_res);
            return 0;
        }

        // Subtract seasonal component (convert from percentage to natural log scale)
        for (int i = 0; i < n_original; i++) {
            int period = i % params->s;
            double dummy = seasonal_res->seasonal_dummies[period];
            log_series[i] -= dummy / 100.0;
        }

        free_seasonal_detection_result(seasonal_res);

        // Apply regular differences to the deseasonalized series
        int n_diff = n_original;
        double *diff_series = malloc(n_original * sizeof(double));
        memcpy(diff_series, log_series, n_original * sizeof(double));
        free(log_series);
        free(datos_originales);

        for (int diff = 0; diff < params->d; diff++) {
            for (int i = 1; i < n_diff; i++) {
                diff_series[i-1] = diff_series[i] - diff_series[i-1];
            }
            n_diff--;
        }

        if (n_diff <= 0) {
            printf("Error: Insufficient observations after regular differencing.\n");
            free(diff_series);
            return 0;
        }

        // Assign transformed series to params->data
        params->data = diff_series;
        params->n_points = n_diff;
        params->D = 0;   // Force seasonal differences to 0

        // Adjust maximum orders: Q must be 0, P remains as specified
        int P_max_ajustado = P_max;
        int Q_max_ajustado = 0;

        printf("Search on deseasonalized and differenced series (d=%d).\n", params->d);
        printf("Effective orders: p_max=%d, q_max=%d, P_max=%d, Q_max=%d\n",
               p_max, q_max, P_max_ajustado, Q_max_ajustado);

        memset(best_candidate, 0, sizeof(ModelCandidate));
        best_candidate->similarity = 0.0;

        adaptive_grid_search(params->data, params->n_points, params->s,
                            p_max, q_max, P_max_ajustado, Q_max_ajustado,
                            best_candidate, params->use_mahalanobis);

        if (best_candidate->similarity > 0) {
            int lags = best_candidate->lags_used;
            if (lags <= 0) lags = MIN(MAX_LAGS, params->n_points / 4);
            best_candidate->acf_theoretical = malloc((lags + 1) * sizeof(double));
            best_candidate->pacf_theoretical = malloc((lags + 1) * sizeof(double));
            if (best_candidate->acf_theoretical && best_candidate->pacf_theoretical) {
                calcular_ACF_PACF_SARIMA(best_candidate->p, best_candidate->best_phi,
                                        best_candidate->q, best_candidate->best_theta,
                                        best_candidate->P, best_candidate->best_Phi,
                                        best_candidate->Q, best_candidate->best_Theta,
                                        params->s,
                                        best_candidate->acf_theoretical,
                                        best_candidate->pacf_theoretical,
                                        lags);
            }
        }

        if (best_candidate->similarity > 0) {
            char mensaje_final[256];
            if (best_candidate->P > 0 || best_candidate->Q > 0) {
                snprintf(mensaje_final, sizeof(mensaje_final),
                        "MEJOR MODELO (desestacionalizado): SARIMA(%d,%d,%d)(%d,%d,%d)%d - Similitud: %.3f",
                        best_candidate->p, params->d, best_candidate->q,
                        best_candidate->P, params->D, best_candidate->Q, params->s,
                        best_candidate->similarity);
            } else {
                snprintf(mensaje_final, sizeof(mensaje_final),
                        "MEJOR MODELO (desestacionalizado): ARMA(%d,%d) - Similitud: %.3f",
                        best_candidate->p, best_candidate->q, best_candidate->similarity);
            }
            report_progress_internal(3, 1.0, mensaje_final, "Detección completada");
        } else {
            report_progress_internal(3, 1.0, "No suitable models found", "Detección completada");
        }

        // Note: params->data is freed later by the caller (since it's a local allocation)
        return 1;
    } else if (params->deseasonalize && is_simulation) {
        fprintf(stderr, "Warning: --deseasonalize is not supported in simulation mode. Ignoring.\n");
        params->deseasonalize = 0;
    }

    // =====================================================================
    // SEASONAL DETECTION AND ADJUSTMENT (only if NOT in simulation mode)
    // =====================================================================
    int P_max_ajustado = P_max;
    int Q_max_ajustado = Q_max;
    char *mensaje_advertencia = NULL;
    char *mensaje_estacionalidad = NULL;
    int estacionalidad_detectada = 0;

    if (!is_simulation) {
        estacionalidad_detectada = detectar_y_ajustar_estacionalidad(
            filename, params, &P_max_ajustado, &Q_max_ajustado, &mensaje_advertencia);

        // Prepare informative message
        if (params->s > 1) {
            char buffer[512];
            if (estacionalidad_detectada) {
                snprintf(buffer, sizeof(buffer),
                        "Seasonality detected (s=%d). Searching SARIMA(%d,0,%d)(%d,0,%d)%d models.",
                        params->s, p_max, q_max, P_max_ajustado, Q_max_ajustado, params->s);
            } else {
                snprintf(buffer, sizeof(buffer),
                        "No seasonality detected (s=%d). Searching ARMA(%d,%d) models.",
                        params->s, p_max, q_max);
            }
            mensaje_estacionalidad = strdup(buffer);
            report_progress_internal(0, 0.0, mensaje_estacionalidad, "Starting automatic detection...");
            free(mensaje_estacionalidad);
        }
    } else {
        // In simulation mode, assume seasonality is as specified by the user (s>1)
        if (params->s > 1) {
            printf("Simulation mode: seasonal period s=%d, SARIMA models will be considered.\n", params->s);
        }
    }

    // =========================================================================
    // UNIT ROOT TESTS (only if d<=1, D=0 and NOT in simulation mode)
    // =========================================================================
    if (!is_simulation && params->d <= 1 && params->D == 0) {
        printf("\n=== UNIT ROOT TESTS (d=%d, D=%d) ===\n", params->d, params->D);
        double *test_data;
        int test_n;
        if (load_data(filename, &test_data, &test_n)) {
            if (params->apply_log) {
                for (int i = 0; i < test_n; i++) {
                    if (test_data[i] > 0) test_data[i] = log(test_data[i]);
                }
            }
            int n_diff = test_n;
            double *differenced_data = malloc(test_n * sizeof(double));
            if (differenced_data) {
                memcpy(differenced_data, test_data, test_n * sizeof(double));
                if (params->d == 1) {
                    for (int i = 1; i < test_n; i++) {
                        differenced_data[i-1] = test_data[i] - test_data[i-1];
                    }
                    n_diff = test_n - 1;
                }
                UnitRootTestResult *unit_root_result = perform_unit_root_tests(
                    differenced_data, n_diff, estacionalidad_detectada, NULL);
                if (unit_root_result) {
                    if (unit_root_result->unit_root_suspected) {
                        printf("❌ %s\n", unit_root_result->warning_message);
                        char warning_msg[512];
                        snprintf(warning_msg, sizeof(warning_msg),
                                "WARNING: Possible unit root detected.\n"
                                "ADF p=%.4f, KPSS rejects stationarity.\n"
                                "Model results may be biased.\n"
                                "Consider applying more regular differences.",
                                unit_root_result->adf_p_value);
                        printf("=== WARNING ===\n%s\n==================\n", warning_msg);
                    } else {
                        printf("✅ No stationarity problems detected.\n");
                    }
                    free_unit_root_test_result(unit_root_result);
                }
                free(differenced_data);
            }
            free(test_data);
        }
    } else if (!is_simulation) {
        printf("Differences applied (d=%d, D=%d), skipping unit root tests.\n", params->d, params->D);
    }

    // =====================================================================
    // DATA LOADING AND TRANSFORMATION
    // =====================================================================
    double *datos_originales = NULL;
    int n_puntos_original = 0;
    int allocated = 0;

    if (!is_simulation) {
        if (!load_data(filename, &datos_originales, &n_puntos_original)) {
            if (mensaje_advertencia) free(mensaje_advertencia);
            return 0;
        }
        allocated = 1;
    } else {
        // Use data already present in params->data (simulation mode)
        datos_originales = params->data;
        n_puntos_original = params->n_points;
    }

    // Create a copy for transformations
    params->data = malloc(n_puntos_original * sizeof(double));
    if (!params->data) {
        if (allocated) free(datos_originales);
        if (mensaje_advertencia) free(mensaje_advertencia);
        return 0;
    }
    memcpy(params->data, datos_originales, n_puntos_original * sizeof(double));
    params->n_points = n_puntos_original;

    // Apply transformations (log, regular differences, seasonal differences)
    transform_data(params);

    if (params->n_points <= 0) {
        free(params->data);
        params->data = NULL;
        if (allocated) free(datos_originales);
        if (mensaje_advertencia) free(mensaje_advertencia);
        return 0;
    }

    // =====================================================================
    // ADAPTIVE GRID SEARCH
    // =====================================================================
    memset(best_candidate, 0, sizeof(ModelCandidate));
    best_candidate->similarity = 0.0;

    printf("=== EXECUTING SEARCH WITH ADJUSTED PARAMETERS ===\n");
    printf("p_max=%d, q_max=%d, P_max_ajustado=%d, Q_max_ajustado=%d, s=%d\n",
           p_max, q_max, P_max_ajustado, Q_max_ajustado, params->s);

    adaptive_grid_search(params->data, params->n_points, params->s,
                        p_max, q_max, P_max_ajustado, Q_max_ajustado,
                        best_candidate, params->use_mahalanobis);

    // =====================================================================
    // THEORETICAL ACF/PACF FOR THE BEST MODEL
    // =====================================================================
    if (best_candidate->similarity > 0) {
        int lags = best_candidate->lags_used;
        if (lags <= 0) lags = MIN(MAX_LAGS, params->n_points / 4);
        best_candidate->acf_theoretical = malloc((lags + 1) * sizeof(double));
        best_candidate->pacf_theoretical = malloc((lags + 1) * sizeof(double));
        if (best_candidate->acf_theoretical && best_candidate->pacf_theoretical) {
            calcular_ACF_PACF_SARIMA(best_candidate->p, best_candidate->best_phi,
                                    best_candidate->q, best_candidate->best_theta,
                                    best_candidate->P, best_candidate->best_Phi,
                                    best_candidate->Q, best_candidate->best_Theta,
                                    params->s,
                                    best_candidate->acf_theoretical,
                                    best_candidate->pacf_theoretical,
                                    lags);
        }
    }

    // =====================================================================
    // CLEANUP AND FINAL REPORT
    // =====================================================================
    if (mensaje_advertencia) {
        printf("\n=== SEASONALITY WARNING ===\n%s\n=========================================\n",
               mensaje_advertencia);
        free(mensaje_advertencia);
    }

    free(params->data);
    params->data = NULL;
    if (allocated) free(datos_originales);

    // Report completion
    if (best_candidate->similarity > 0) {
        char mensaje_final[256];
        if (best_candidate->P > 0 || best_candidate->Q > 0) {
            snprintf(mensaje_final, sizeof(mensaje_final),
                    "BEST MODEL: SARIMA(%d,%d,%d)(%d,%d,%d)%d - Similarity: %.3f",
                    best_candidate->p, params->d, best_candidate->q,
                    best_candidate->P, params->D, best_candidate->Q, params->s,
                    best_candidate->similarity);
        } else {
            snprintf(mensaje_final, sizeof(mensaje_final),
                    "BEST MODEL: ARMA(%d,%d) - Similarity: %.3f",
                    best_candidate->p, best_candidate->q, best_candidate->similarity);
        }
        report_progress_internal(3, 1.0, mensaje_final, "Detection completed");
    } else {
        report_progress_internal(3, 1.0, "No suitable models found", "Detection completed");
    }

    return 1;
}

/**
 * @brief Calcula similitud entre patrones teóricos y empíricos considerando estacionalidad en ACF y PACF
 *
 * Esta función compara las características de patrones ACF/PACF de un modelo teórico
 * con las empíricas, dando peso balanceado a componentes regulares y estacionales.
 * Considera específicamente patrones estacionales tanto en ACF (MA estacional)
 * como en PACF (AR estacional).
 *
 * @param theoretical Características del modelo teórico
 * @param empirical Características del patrón empírico
 * @param s Período estacional
 * @param lags Número de lags considerados
 * @return double Puntuación de similitud entre 0.0 (sin similitud) y 1.0 (idéntico)
 */
double pattern_similarity(PatternFeatures *theoretical, PatternFeatures *empirical, int s, int lags) {
    double similarity = 0.0;
    double total_weight = 0.0;

    int should_evaluate_seasonal = (s > 1);

    // SISTEMA DE PESOS BALANCEADO
    double first_lags_weight = 0.60;    // Primeros 8 lags (ACF + PACF)
    double seasonal_weight = 0.25;      // Estacionalidad (ACF + PACF)
    double cutting_weight = 0.15;       // Cortes específicos

    // 1. COMPARACIÓN DE PRIMEROS 8 LAGS (COMPORTAMIENTO NO ESTACIONAL)
    double first_lags_sim = 0.0;
    int critical_lags = MIN(8, lags);
    double first_lags_total_weight = 0.0;

    for (int i = 1; i <= critical_lags; i++) {
        double acf_diff = fabs(theoretical->acf_values[i] - empirical->acf_values[i]);
        double pacf_diff = fabs(theoretical->pacf_values[i] - empirical->pacf_values[i]);

        double lag_sim = 1.0 - (acf_diff + pacf_diff) / 2.0;
        double weight = geometric_weight(i, 0.8); // Más peso a primeros lags

        first_lags_sim += lag_sim * weight;
        first_lags_total_weight += weight;
    }

    if (critical_lags > 0) {
        first_lags_sim /= first_lags_total_weight;
        similarity += first_lags_sim * first_lags_weight;
        total_weight += first_lags_weight;
    }

    // 2. ANÁLISIS ESTACIONAL MEJORADO (ACF Y PACF)
    if (should_evaluate_seasonal) {
        double seasonal_sim = 0.0;
        double seasonal_total_weight = 0.0;

        // A. COMPORTAMIENTO ESTACIONAL EN ACF (MA Estacional)
        double acf_seasonal_sim = 0.0;
        double acf_seasonal_weight = 0.0;

        for (int i = 1; i * s <= lags && i <= 2; i++) {
            int lag = i * s;
            double acf_diff = fabs(theoretical->acf_values[lag] - empirical->acf_values[lag]);
            double lag_sim = 1.0 - acf_diff;

            // MA estacional: dar más peso a primeros lags estacionales
            double weight = 1.0 / i;
            acf_seasonal_sim += lag_sim * weight;
            acf_seasonal_weight += weight;
        }

        if (acf_seasonal_weight > 0) {
            seasonal_sim += (acf_seasonal_sim / acf_seasonal_weight) * 0.5;
            seasonal_total_weight += 0.5;
        }

        // B. COMPORTAMIENTO ESTACIONAL EN PACF (AR Estacional)
        double pacf_seasonal_sim = 0.0;
        double pacf_seasonal_weight = 0.0;

        for (int i = 1; i * s <= lags && i <= 2; i++) {
            int lag = i * s;
            double pacf_diff = fabs(theoretical->pacf_values[lag] - empirical->pacf_values[lag]);
            double lag_sim = 1.0 - pacf_diff;

            double weight = 1.0 / i;
            pacf_seasonal_sim += lag_sim * weight;
            pacf_seasonal_weight += weight;
        }

        if (pacf_seasonal_weight > 0) {
            seasonal_sim += (pacf_seasonal_sim / pacf_seasonal_weight) * 0.5;
            seasonal_total_weight += 0.5;
        }

        if (seasonal_total_weight > 0) {
            similarity += (seasonal_sim / seasonal_total_weight) * seasonal_weight;
            total_weight += seasonal_weight;
        }
    }

    // 3. DETECCIÓN DE CORTES ESPECÍFICOS
    double cutting_sim = 0.0;
    double cutting_total_weight = 0.0;

    // A. Corte en ACF (MA regular)
    if (theoretical->acf_cutting_lag > 0 && empirical->acf_cutting_lag > 0) {
        double cutoff_diff = fabs(theoretical->acf_cutting_lag - empirical->acf_cutting_lag);
        double max_cutoff = MAX(theoretical->acf_cutting_lag, empirical->acf_cutting_lag);
        double cutoff_sim = 1.0 - (cutoff_diff / max_cutoff);
        cutting_sim += cutoff_sim * 0.4;
        cutting_total_weight += 0.4;
    }

    // B. Corte en PACF (AR regular)
    if (theoretical->pacf_cutting_lag > 0 && empirical->pacf_cutting_lag > 0) {
        double cutoff_diff = fabs(theoretical->pacf_cutting_lag - empirical->pacf_cutting_lag);
        double max_cutoff = MAX(theoretical->pacf_cutting_lag, empirical->pacf_cutting_lag);
        double cutoff_sim = 1.0 - (cutoff_diff / max_cutoff);
        cutting_sim += cutoff_sim * 0.4;
        cutting_total_weight += 0.4;
    }

    // C. Corte estacional (combinado)
    if (theoretical->cutting_off_lag > 0 && empirical->cutting_off_lag > 0) {
        double cutoff_diff = fabs(theoretical->cutting_off_lag - empirical->cutting_off_lag);
        double max_cutoff = MAX(theoretical->cutting_off_lag, empirical->cutting_off_lag);
        double cutoff_sim = 1.0 - (cutoff_diff / max_cutoff);
        cutting_sim += cutoff_sim * 0.2;
        cutting_total_weight += 0.2;
    }

    if (cutting_total_weight > 0) {
        similarity += (cutting_sim / cutting_total_weight) * cutting_weight;
        total_weight += cutting_weight;
    }

    return (total_weight > 0) ? similarity / total_weight : 0.0;
}


// Función para calcular similitud euclídea entre dos arrays
double euclidean_similarity(double *array1, double *array2, int start_lag, int end_lag) {
    if (start_lag >= end_lag) return 0.0;

    double sum_sq_diff = 0.0;
    int count = 0;

    for (int i = start_lag; i <= end_lag; i++) {
        double diff = array1[i] - array2[i];
        sum_sq_diff += diff * diff;
        count++;
    }

    if (count == 0) return 0.0;

    double rmse = sqrt(sum_sq_diff / count);

    // Convertir RMSE a similitud (1.0 = idéntico, 0.0 )
    // Usamos una función exponencial para mapear RMSE a [0,1]
    double similarity = exp(-2.0 * rmse);

    return similarity;
}

// Función para calcular todas las similitudes euclídeas
 void calculate_euclidean_similarities(PatternFeatures *theoretical, PatternFeatures *empirical,
                                     int lags, int s) {
    if (!theoretical || !empirical || !theoretical->acf_values ||
        !empirical->acf_values || !theoretical->pacf_values || !empirical->pacf_values) {
        return;
    }

    // 1. COMPARACIÓN DE PRIMEROS LAGS (hasta punto de corte)
    int acf_cut_lag = MIN(theoretical->acf_cutting_lag, empirical->acf_cutting_lag);
    int pacf_cut_lag = MIN(theoretical->pacf_cutting_lag, empirical->pacf_cutting_lag);

    if (acf_cut_lag == 0) acf_cut_lag = MIN(8, lags);
    if (pacf_cut_lag == 0) pacf_cut_lag = MIN(8, lags);

    acf_cut_lag = MAX(acf_cut_lag, 2);
    pacf_cut_lag = MAX(pacf_cut_lag, 2);

    theoretical->acf_euclidean_similarity = euclidean_similarity(
        theoretical->acf_values, empirical->acf_values, 1, acf_cut_lag);

    theoretical->pacf_euclidean_similarity = euclidean_similarity(
        theoretical->pacf_values, empirical->pacf_values, 1, pacf_cut_lag);

    // MEJORA 3: COMPARACIÓN ESTACIONAL MÁS DETALLADA
    if (s > 1) {
        double seasonal_sum_sq_diff_acf = 0.0;
        double seasonal_sum_sq_diff_pacf = 0.0;
        int seasonal_count = 0;

        // Considerar hasta 3 lags estacionales (s, 2s, 3s)
        for (int i = 1; i <= 3; i++) {
            int main_lag = i * s;
            if (main_lag > lags) break;

            // Lag estacional principal - PESO COMPLETO
            double diff_acf = theoretical->acf_values[main_lag] - empirical->acf_values[main_lag];
            double diff_pacf = theoretical->pacf_values[main_lag] - empirical->pacf_values[main_lag];
            seasonal_sum_sq_diff_acf += diff_acf * diff_acf;
            seasonal_sum_sq_diff_pacf += diff_pacf * diff_pacf;
            seasonal_count++;

            // MEJORA: Los satélites solo para el primer lag estacional
            if (i == 1) {
                // Satélites estacionales (s-1, s+1) - PESO REDUCIDO
                if (main_lag - 1 >= 1 && main_lag - 1 <= lags) {
                    diff_acf = theoretical->acf_values[main_lag-1] - empirical->acf_values[main_lag-1];
                    diff_pacf = theoretical->pacf_values[main_lag-1] - empirical->pacf_values[main_lag-1];
                    seasonal_sum_sq_diff_acf += diff_acf * diff_acf * 0.5; // Reducido de 0.7
                    seasonal_sum_sq_diff_pacf += diff_pacf * diff_pacf * 0.5;
                    seasonal_count += 0.5;
                }

                if (main_lag + 1 <= lags) {
                    diff_acf = theoretical->acf_values[main_lag+1] - empirical->acf_values[main_lag+1];
                    diff_pacf = theoretical->pacf_values[main_lag+1] - empirical->pacf_values[main_lag+1];
                    seasonal_sum_sq_diff_acf += diff_acf * diff_acf * 0.5;
                    seasonal_sum_sq_diff_pacf += diff_pacf * diff_pacf * 0.5;
                    seasonal_count += 0.5;
                }
            }
        }

        if (seasonal_count > 0) {
            double rmse_acf = sqrt(seasonal_sum_sq_diff_acf / seasonal_count);
            double rmse_pacf = sqrt(seasonal_sum_sq_diff_pacf / seasonal_count);

            // MEJORA: Penalizar más las discrepancias en lags estacionales posteriores
            double combined_rmse = (rmse_acf * 0.6 + rmse_pacf * 0.4); // Dar más peso a ACF estacional

            // Función de penalización más agresiva para diferencias grandes
            if (combined_rmse > 0.3) {
                combined_rmse *= 1.5; // Penalización extra para diferencias grandes
            }

            theoretical->seasonal_euclidean_similarity = exp(-3.0 * combined_rmse); // Más sensible
        } else {
            theoretical->seasonal_euclidean_similarity = 0.0;
        }
    } else {
        theoretical->seasonal_euclidean_similarity = 0.0;
    }

    theoretical->comparison_lags = MAX(acf_cut_lag, pacf_cut_lag);
}

// Función para aplicar ponderación a las características
void apply_weights_to_features(PatternFeatures *features, int lags, int s,
                              double decay_factor, double seasonal_strength) {
    // Esta función modifica las características según los pesos
    // En una implementación real, esto afectaría cómo se calcula la similitud
}

// model_detection.c - Modificar la función detectar_y_ajustar_estacionalidad

int detectar_y_ajustar_estacionalidad(const char *filename, DataParameters *params,
                                            int *P_max, int *Q_max, char **mensaje_advertencia) {
    // Si se han especificado diferencias estacionales (D > 0), omitir detección
    if (params->D > 0) {
        printf("=== DIFERENCIAS ESTACIONALES APLICADAS (D=%d) ===\n", params->D);
        printf("No se realiza detección de estacionalidad porque ya se aplicaron diferencias estacionales.\n");

        // Mantener los parámetros estacionales especificados
        *mensaje_advertencia = NULL;

        // Verificar si hay estructura ARMA estacional para complementar las diferencias
        if (*P_max == 0 && *Q_max == 0) {
            *mensaje_advertencia = strdup(
                "INFORMACIÓN: Se aplicaron diferencias estacionales (D=%d) pero no se especificaron\n"
                "componentes ARMA estacionales (P, Q). Esto puede ser adecuado para modelos\n"
                "con estacionalidad puramente determinística."
            );
            printf("ℹ️  %s\n", *mensaje_advertencia);
        } else {
            printf("✅ Diferencias estacionales (D=%d) con componentes ARMA estacionales (P_max=%d, Q_max=%d)\n",
                   params->D, *P_max, *Q_max);
        }

        return 1; // Asumir que hay estacionalidad porque el usuario especificó D > 0
    }

    if (params->s <= 1) {
        // No hay estacionalidad especificada
        *P_max = 0;
        *Q_max = 0;
        *mensaje_advertencia = NULL;
        return 0;
    }

            // Permitir s=5 además de los valores típicos
        if (params->s != 4 && params->s != 5 && params->s != 7 && params->s != 12 && params->s != 24 && params->s != 52) {
            printf("Advertencia: Período estacional s=%d poco común, pero procediendo\n", params->s);
        }

    printf("=== DETECCIÓN DE ESTACIONALIDAD (s=%d) ===\n", params->s);

    // Ejecutar detección de estacionalidad solo si no hay diferencias estacionales
    SeasonalDetectionResult *resultado_estacional =
        detect_seasonality_harmonic_regression(filename, params->d, params->apply_log, params->s);

    if (!resultado_estacional) {
        printf("Error en detección de estacionalidad\n");
        *mensaje_advertencia = strdup("Error en detección de estacionalidad");
        return 0;
    }

    int estacionalidad_detectada = resultado_estacional->seasonal_detected;
    double p_valor = resultado_estacional->p_value;

    printf("Resultado detección estacional: %s (p=%.4f)\n",
           estacionalidad_detectada ? "DETECTADA" : "NO DETECTADA", p_valor);

    if (estacionalidad_detectada) {
        // Estacionalidad detectada - mantener parámetros estacionales originales
        *mensaje_advertencia = NULL;

        // Verificar si hay estructura estacional especificada
        if (*P_max == 0 && *Q_max == 0) {
            *mensaje_advertencia = strdup(
                "ADVERTENCIA: Se detectó estacionalidad pero no se especificó estructura estacional.\n"
                "Considere añadir componentes ARMA estacionales (P, Q) o diferencias estacionales (D)."
            );
            printf("⚠️  %s\n", *mensaje_advertencia);
        } else {
            printf("✅ Estacionalidad detectada y estructura estacional especificada (P_max=%d, Q_max=%d)\n",
                   *P_max, *Q_max);
        }
    } else if (!params->mlp_direct) {
        // MODO CLÁSICO: no se detectó estacionalidad -> restringir el grid a P=Q=0
        // (la puerta es un filtro de EFICIENCIA para la búsqueda en grid).
        printf("🔒 Restringiendo búsqueda a modelos NO estacionales (P=0, Q=0)\n");
        *P_max = 0;
        *Q_max = 0;
        *mensaje_advertencia = strdup(
            "INFORMACIÓN: No se detectó estacionalidad significativa. "
            "Se restringió la búsqueda a modelos no estacionales."
        );
    } else {
        // MODO BJ (mlp-direct): NO se restringe. El MLP + features estacionales y el
        // AICc del shortlist deciden la estacionalidad (coherente con ART). El coste
        // de incluir candidatos estacionales es trivial (no hay grid que restringir),
        // y así no se pierden SARIMA estocásticos que el test F podría no captar.
        printf("ℹ️  Modo BJ: la estacionalidad la decide el MLP/AICc (sin restringir P,Q).\n");
        *mensaje_advertencia = NULL;
    }

    // Liberar memoria del resultado estacional
    free_seasonal_detection_result(resultado_estacional);

    return estacionalidad_detectada;

}


/* Yule-Walker: estimate AR(p) coefficients from empirical ACF.
 * Solves R·φ = r where R_ij = ρ[|i-j|] and r_i = ρ[i].
 * Returns 1 on success, 0 on failure (singular R). */
static int estimate_ar_yule_walker(double *acf, int p, double *phi) {
    if (p <= 0) return 1;
    // Build Toeplitz matrix R (p x p) and vector r
    double *R = (double*)malloc(p * p * sizeof(double));
    double *r = (double*)malloc(p * sizeof(double));
    for (int i = 0; i < p; i++) {
        r[i] = acf[i + 1];
        for (int j = 0; j < p; j++) {
            R[i * p + j] = acf[abs(i - j)];
        }
    }
    // Gaussian elimination with partial pivoting
    for (int k = 0; k < p; k++) {
        // Find pivot
        int max_row = k;
        double max_val = fabs(R[k * p + k]);
        for (int i = k + 1; i < p; i++) {
            if (fabs(R[i * p + k]) > max_val) {
                max_val = fabs(R[i * p + k]);
                max_row = i;
            }
        }
        if (max_val < 1e-10) { free(R); free(r); return 0; }
        // Swap rows
        if (max_row != k) {
            for (int j = k; j < p; j++) {
                double tmp = R[k * p + j];
                R[k * p + j] = R[max_row * p + j];
                R[max_row * p + j] = tmp;
            }
            double tmp = r[k]; r[k] = r[max_row]; r[max_row] = tmp;
        }
        // Eliminate
        for (int i = k + 1; i < p; i++) {
            double factor = R[i * p + k] / R[k * p + k];
            for (int j = k; j < p; j++) {
                R[i * p + j] -= factor * R[k * p + j];
            }
            r[i] -= factor * r[k];
        }
    }
    // Back substitution
    for (int i = p - 1; i >= 0; i--) {
        double sum = r[i];
        for (int j = i + 1; j < p; j++) sum -= R[i * p + j] * phi[j];
        phi[i] = sum / R[i * p + i];
    }
    free(R); free(r);
    // Verify stationarity (simple check)
    double sum_abs = 0.0;
    for (int i = 0; i < p; i++) sum_abs += fabs(phi[i]);
    if (sum_abs >= 0.99) {
        // Rescale to ensure stationarity
        double scale = 0.95 / sum_abs;
        for (int i = 0; i < p; i++) phi[i] *= scale;
    }
    return 1;
}

/* Hannan-Rissanen ITERADO: estima ARMA(p,q) vía AR largo + OLS, refinando los
 * residuos con el propio modelo ARMA en cada iteración (mejora notable de la
 * parte MA frente al HR de 2 etapas). Convención Box-Jenkins:
 *   yc_t = sum phi_i yc_{t-i} + e_t - sum theta_j e_{t-j}   (yc = serie centrada)
 * La regresión de yc sobre [yc_lags, e_lags] da coef. gamma_j = -theta_j en la
 * parte MA, de ahí theta_j = -beta[p+j] (corrige el signo del HR anterior).
 * Estimación ligera para puntuar el shortlist. Devuelve 1 si tuvo éxito. */
static int estimate_arma_hannan_rissanen(double *y, int n, int p, int q,
                                         double *phi, double *theta) {
    if (p == 0 && q == 0) return 1;
    int k = p + q;

    // Centrar la serie (la regresión no lleva intercepto)
    double mean = 0.0; for (int i = 0; i < n; i++) mean += y[i]; mean /= n;
    double *yc = (double*)malloc(n * sizeof(double));
    if (!yc) return 0;
    for (int i = 0; i < n; i++) yc[i] = y[i] - mean;

    // Etapa 1: AR largo (Yule-Walker) -> residuos iniciales e[t]
    int m = MAX(p, q) + (int)sqrt(n);
    if (m < MAX(p, q) + 2) m = MAX(p, q) + 2;
    if (m >= n - 10) m = n - 10;
    if (m < 1) m = 1;

    double acf_long[MAX_LAGS + 1];
    calcular_ACF_muestral(yc, n, acf_long, MIN(m + 5, MAX_LAGS));
    double *phi_long = (double*)calloc(m, sizeof(double));
    double *e = (double*)calloc(n, sizeof(double));
    if (!phi_long || !e) { free(yc); free(phi_long); free(e); return 0; }
    if (!estimate_ar_yule_walker(acf_long, m, phi_long)) { free(yc); free(phi_long); free(e); return 0; }
    for (int t = m; t < n; t++) {
        double pred = 0.0;
        for (int j = 0; j < m; j++) pred += phi_long[j] * yc[t - j - 1];
        e[t] = yc[t] - pred;
    }

    // Empezar donde los residuos e[t-1..t-q] ya son reales (t-q >= m)
    int start = MAX(m + q, MAX(p, q));
    int nobs = n - start;
    if (nobs < k + 5) { free(yc); free(phi_long); free(e); return 0; }

    double *X   = (double*)malloc(nobs * k * sizeof(double));
    double *Y   = (double*)malloc(nobs * sizeof(double));
    double *XtX = (double*)malloc(k * k * sizeof(double));
    double *XtY = (double*)malloc(k * sizeof(double));
    double *beta = (double*)calloc(k, sizeof(double));
    if (!X || !Y || !XtX || !XtY || !beta) {
        free(yc); free(phi_long); free(e); free(X); free(Y); free(XtX); free(XtY); free(beta);
        return 0;
    }

    const int NITER = 4;
    int ok = 1;
    for (int iter = 0; iter < NITER && ok; iter++) {
        // Regresores: Y[i]=yc[t], X=[yc_{t-1..t-p}, e_{t-1..t-q}]
        for (int i = 0; i < nobs; i++) {
            int t = start + i;
            Y[i] = yc[t];
            for (int j = 0; j < p; j++) X[i * k + j]     = yc[t - j - 1];
            for (int j = 0; j < q; j++) X[i * k + p + j] = e[t - j - 1];
        }
        // (X'X + ridge) beta = X'Y
        for (int a = 0; a < k; a++) { XtY[a] = 0.0; for (int b = 0; b < k; b++) XtX[a * k + b] = 0.0; }
        for (int i = 0; i < nobs; i++)
            for (int r = 0; r < k; r++) {
                XtY[r] += X[i * k + r] * Y[i];
                for (int c = 0; c < k; c++) XtX[r * k + c] += X[i * k + r] * X[i * k + c];
            }
        for (int i = 0; i < k; i++) XtX[i * k + i] += 1e-6;  // ridge

        for (int col = 0; col < k && ok; col++) {
            int mr = col; double mv = fabs(XtX[col * k + col]);
            for (int r = col + 1; r < k; r++)
                if (fabs(XtX[r * k + col]) > mv) { mv = fabs(XtX[r * k + col]); mr = r; }
            if (mv < 1e-12) { ok = 0; break; }
            if (mr != col) {
                for (int c = col; c < k; c++) { double t = XtX[col * k + c]; XtX[col * k + c] = XtX[mr * k + c]; XtX[mr * k + c] = t; }
                double t = XtY[col]; XtY[col] = XtY[mr]; XtY[mr] = t;
            }
            for (int r = col + 1; r < k; r++) {
                double f = XtX[r * k + col] / XtX[col * k + col];
                for (int c = col; c < k; c++) XtX[r * k + c] -= f * XtX[col * k + c];
                XtY[r] -= f * XtY[col];
            }
        }
        if (!ok) break;
        for (int i = k - 1; i >= 0; i--) {
            double sum = XtY[i];
            for (int j = i + 1; j < k; j++) sum -= XtX[i * k + j] * beta[j];
            beta[i] = sum / XtX[i * k + i];
        }

        // Recalcular residuos con el modelo actual (gamma_j = beta[p+j] = -theta_j):
        //   e_t = yc_t - sum phi_i yc_{t-i} - sum gamma_j e_{t-j}
        for (int t = start; t < n; t++) {
            double pred = 0.0;
            for (int j = 0; j < p; j++) pred += beta[j] * yc[t - j - 1];
            for (int j = 0; j < q; j++) pred += beta[p + j] * e[t - j - 1];
            e[t] = yc[t] - pred;
        }
    }

    if (ok) {
        for (int i = 0; i < p; i++) phi[i]   =  beta[i];
        for (int i = 0; i < q; i++) theta[i] = -beta[p + i];   // signo corregido
        double sa = 0.0; for (int i = 0; i < p; i++) sa += fabs(phi[i]);
        if (sa > 0.95) { double sc = 0.90 / sa; for (int i = 0; i < p; i++) phi[i] *= sc; }
        sa = 0.0; for (int i = 0; i < q; i++) sa += fabs(theta[i]);
        if (sa > 0.95) { double sc = 0.90 / sa; for (int i = 0; i < q; i++) theta[i] *= sc; }
    }
    if (ok && getenv("ART_DEBUG_HR")) {
        fprintf(stderr, "HR(%d,%d): phi=", p, q);
        for (int i = 0; i < p; i++) fprintf(stderr, "%+.3f ", phi[i]);
        fprintf(stderr, " theta(BJ, Z=(1-theta*B)a)=");
        for (int i = 0; i < q; i++) fprintf(stderr, "%+.3f ", theta[i]);
        fprintf(stderr, "\n");
    }
    free(yc); free(phi_long); free(e); free(X); free(Y); free(XtX); free(XtY); free(beta);
    return ok;
}

/* AICc de un ARMA(p,q) regular por filtrado de residuos (convención Box-Jenkins:
 * y_t = sum phi_i y_{t-i} + e_t - sum theta_j e_{t-j}). Menor = mejor. La serie se
 * centra previamente. Para s>1 con componentes estacionales se aproxima con la
 * parte regular (los tests actuales son no estacionales). */
static double compute_arma_aicc(double *y, int n, int p, double *phi, int q, double *theta) {
    int k = p + q;
    double mean = 0.0; for (int i = 0; i < n; i++) mean += y[i]; mean /= n;
    int start = MAX(p, q);
    if (start >= n - 2) return 1e30;
    double *e = (double*)calloc(n, sizeof(double));
    if (!e) return 1e30;
    double rss = 0.0; int used = 0;
    for (int t = start; t < n; t++) {
        double pred = 0.0;
        for (int i = 0; i < p; i++) pred += phi[i] * (y[t - i - 1] - mean);
        for (int j = 0; j < q; j++) pred -= theta[j] * e[t - j - 1];
        e[t] = (y[t] - mean) - pred;
        rss += e[t] * e[t]; used++;
    }
    free(e);
    if (used <= k + 2 || rss <= 0.0) return 1e30;
    double aicc = used * log(rss / used) + 2.0 * k
                + 2.0 * k * (k + 1.0) / (double)(used - k - 1);
    return aicc;
}

/* AICc de un SARIMA multiplicativo (p,q)(P,Q)_s por expansión de polinomios y
 * filtrado de residuos. Convención BJ: AR(B)·SAR(B^s) Z = MA(B)·SMA(B^s) a, con
 * coef. positivos (Z=(1-theta B)(1-Theta B^s)a). Menor = mejor. Maneja el caso
 * regular (P=Q=0) como caso particular. */
static double compute_sarima_aicc(double *y, int n, int p, double *phi, int q, double *theta,
                                  int P, double *Phi, int Q, double *Theta, int s,
                                  int min_start) {
    int k = p + q + P + Q;
    if (k == 0) return 1e30;
    int dar = p + P * s, dma = q + Q * s;
    if (dar >= n / 2 || dma >= n / 2) return 1e30;

    // Polinomios AR/MA regulares y estacionales (coef. de B^i, con 1 en B^0)
    double arr[16] = {0}, ars[64] = {0}, mar[16] = {0}, mas[64] = {0};
    if (p + 1 > 16 || P * s + 1 > 64 || q + 1 > 16 || Q * s + 1 > 64) return 1e30;
    arr[0] = 1.0; for (int i = 0; i < p; i++) arr[i + 1]      = -phi[i];
    ars[0] = 1.0; for (int i = 0; i < P; i++) ars[(i + 1) * s] = -Phi[i];
    mar[0] = 1.0; for (int i = 0; i < q; i++) mar[i + 1]      = -theta[i];
    mas[0] = 1.0; for (int i = 0; i < Q; i++) mas[(i + 1) * s] = -Theta[i];

    // Expansión por convolución: arexp = arr*ars (grado dar), maexp = mar*mas (grado dma)
    double arexp[80] = {0}, maexp[80] = {0};
    if (dar + 1 > 80 || dma + 1 > 80) return 1e30;
    for (int i = 0; i <= p; i++)
        for (int j = 0; j <= P * s; j++) arexp[i + j] += arr[i] * ars[j];
    for (int i = 0; i <= q; i++)
        for (int j = 0; j <= Q * s; j++) maexp[i + j] += mar[i] * mas[j];

    double mean = 0.0; for (int i = 0; i < n; i++) mean += y[i]; mean /= n;
    // Muestra COMÚN: arranque = max(orden propio, min_start) para que todos los
    // candidatos usen el mismo número de observaciones y el AICc sea comparable.
    int start = MAX(MAX(dar, dma), min_start);
    if (start >= n - 2) return 1e30;
    double *e = (double*)calloc(n, sizeof(double));
    if (!e) return 1e30;
    double rss = 0.0; int used = 0;
    for (int t = start; t < n; t++) {
        // e_t = sum_{i=0..dar} arexp[i] zc_{t-i}  -  sum_{j=1..dma} maexp[j] e_{t-j}
        double v = 0.0;
        for (int i = 0; i <= dar; i++) v += arexp[i] * (y[t - i] - mean);
        for (int j = 1; j <= dma; j++) v -= maexp[j] * e[t - j];
        e[t] = v;
        rss += v * v; used++;
    }
    free(e);
    if (used <= k + 2 || rss <= 0.0) return 1e30;
    return used * log(rss / used) + 2.0 * k + 2.0 * k * (k + 1.0) / (double)(used - k - 1);
}

/* Estima el coeficiente MA(1) estacional invirtiendo rho_s = -Theta/(1+Theta^2)
 * (raíz invertible |Theta|<1). Para la puntuación de candidatos SMA. */
static double estimate_seasonal_ma1(double rho_s) {
    if (fabs(rho_s) < 1e-6) return 0.0;
    if (fabs(rho_s) >= 0.5) rho_s = (rho_s > 0 ? 0.49 : -0.49);  // fuera de rango invertible
    double disc = 1.0 - 4.0 * rho_s * rho_s;
    if (disc < 0) disc = 0;
    return (-1.0 + sqrt(disc)) / (2.0 * rho_s);
}

/* Identificación de órdenes MIXTOS (estilo EACF/Box-Jenkins): recorre una pequeña
 * rejilla ARMA(p<=3, q<=2), estima cada celda por H-R/YW y la puntúa por AICc.
 * Inserta en el shortlist los mejores modelos de bajo orden con buen ajuste, que
 * es donde el MLP y la lectura por cortes fallan (p.ej. ARMA(2,1) leído como AR).
 * Los candidatos de la rejilla son regulares (P=Q=0). */
static void add_arma_grid_candidates(ModelCandidate *cand, double *data, int n,
                                     double *acf_emp, int lags, int s,
                                     int p_max, int q_max) {
    (void)lags; (void)s; (void)acf_emp;
    int Pg = MIN(3, p_max), Qg = MIN(2, q_max);
    typedef struct { int p, q; double aicc; } GC;
    GC g[16]; int ng = 0;
    // Solo celdas MIXTAS (p>=1, q>=1): el AR/MA puro ya lo cubren las reducciones
    // y la lectura por cortes. Aquí añadimos lo que falta sin meter más AR que
    // compita en el ranking y entierre al ARMA verdadero.
    for (int p = 1; p <= Pg; p++) {
        for (int q = 1; q <= Qg; q++) {
            double phi[10] = {0}, theta[10] = {0};
            if (!estimate_arma_hannan_rissanen(data, n, p, q, phi, theta)) continue;
            double a = compute_arma_aicc(data, n, p, phi, q, theta);
            if (a >= 1e29) continue;
            g[ng].p = p; g[ng].q = q; g[ng].aicc = a; ng++;
        }
    }
    for (int i = 0; i < ng; i++)
        for (int j = i + 1; j < ng; j++)
            if (g[j].aicc < g[i].aicc) { GC t = g[i]; g[i] = g[j]; g[j] = t; }

    int added = 0;
    for (int idx = 0; idx < ng && added < 4; idx++) {
        if (cand->n_candidates >= MAX_ORDER_CANDIDATES) break;
        int pp = g[idx].p, qq = g[idx].q, dup = 0;
        for (int c = 0; c < cand->n_candidates; c++)
            if (cand->candidates[c].p == pp && cand->candidates[c].q == qq &&
                cand->candidates[c].P == 0 && cand->candidates[c].Q == 0) dup = 1;
        if (dup) continue;
        cand->candidates[cand->n_candidates].p = pp;
        cand->candidates[cand->n_candidates].q = qq;
        cand->candidates[cand->n_candidates].P = 0;
        cand->candidates[cand->n_candidates].Q = 0;
        cand->candidates[cand->n_candidates].prob = 0.0;
        cand->n_candidates++;
        added++;
    }
}

/* AICc de un candidato SARIMA: estima coeficientes (YW/H-R regular, YW estacional,
 * Theta_1 por inversión) y devuelve compute_sarima_aicc. 1e30 si inestable/falla. */
static double sarima_candidate_aicc(double *data, int n, double *acf_emp, int lags,
                                    int s, int p, int q, int P, int Q, int min_start) {
    double phi[10] = {0}, theta[10] = {0}, Phi[5] = {0}, Theta[5] = {0};
    if (q == 0) { if (p > 0 && !estimate_ar_yule_walker(acf_emp, p, phi)) return 1e30; }
    else        { if (!estimate_arma_hannan_rissanen(data, n, p, q, phi, theta)) return 1e30; }
    if (P > 0) {
        double acs[16]; acs[0] = 1.0;
        for (int i = 1; i <= P && i < 16; i++) { int lg = i * s; acs[i] = (lg <= lags) ? acf_emp[lg] : 0.0; }
        estimate_ar_yule_walker(acs, P, Phi);
    }
    if (Q > 0) {
        Theta[0] = (s <= lags) ? estimate_seasonal_ma1(acf_emp[s]) : MIN_SEASONAL_MA_COEF;
        for (int i = 1; i < Q; i++) Theta[i] = MIN_SEASONAL_MA_COEF;
    }
    if (p > 0 && !check_ar_roots(phi, p)) return 1e30;
    if (P > 0 && !check_ar_roots(Phi, P)) return 1e30;
    if (q > 0 && !check_ma_roots(theta, q)) return 1e30;
    if (Q > 0 && !check_ma_roots(Theta, Q)) return 1e30;
    return compute_sarima_aicc(data, n, p, phi, q, theta, P, Phi, Q, Theta, s, min_start);
}

/* Identificación de candidatos SARIMA: cruza las bases REGULARES presentes en el
 * shortlist (p,q con p+q<=3) con TODAS las hipótesis estacionales (P,Q) de una
 * pequeña rejilla, puntúa por AICc e inserta las mejores. Esencial porque el MLP
 * confunde SAR<->SMA: aquí se exploran ambas y el AICc decide (p.ej. recupera el
 * (1,0,0,1) cuando el MLP predijo P=1 en vez de Q=1). */
static void add_seasonal_grid_candidates(ModelCandidate *cand, double *data, int n,
                                         double *acf_emp, int lags, int s,
                                         int P_max, int Q_max) {
    if (s <= 1) return;
    int Pg = MIN(2, P_max), Qg = MIN(1, Q_max);
    if (Pg == 0 && Qg == 0) return;

    // Bases regulares distintas presentes (parsimoniosas, pocas para no diluir)
    int bp[8], bq[8], nb = 0;
    for (int c = 0; c < cand->n_candidates && nb < 3; c++) {
        int p = cand->candidates[c].p, q = cand->candidates[c].q;
        if (p + q > 2) continue;
        int dup = 0; for (int k = 0; k < nb; k++) if (bp[k] == p && bq[k] == q) dup = 1;
        if (!dup) { bp[nb] = p; bq[nb] = q; nb++; }
    }

    typedef struct { int p, q, P, Q; double aicc; } GC;
    GC g[96]; int ng = 0;
    for (int b = 0; b < nb; b++)
        for (int P = 0; P <= Pg; P++)
            for (int Q = 0; Q <= Qg; Q++) {
                if (P == 0 && Q == 0) continue;   // solo añadimos estructura estacional
                double a = sarima_candidate_aicc(data, n, acf_emp, lags, s, bp[b], bq[b], P, Q, 0);
                if (a >= 1e29) continue;
                g[ng].p = bp[b]; g[ng].q = bq[b]; g[ng].P = P; g[ng].Q = Q; g[ng].aicc = a; ng++;
            }
    for (int i = 0; i < ng; i++)
        for (int j = i + 1; j < ng; j++)
            if (g[j].aicc < g[i].aicc) { GC t = g[i]; g[i] = g[j]; g[j] = t; }

    int added = 0;
    for (int idx = 0; idx < ng && added < 2; idx++) {
        if (cand->n_candidates >= MAX_ORDER_CANDIDATES) break;
        int p = g[idx].p, q = g[idx].q, P = g[idx].P, Q = g[idx].Q, dup = 0;
        for (int c = 0; c < cand->n_candidates; c++)
            if (cand->candidates[c].p == p && cand->candidates[c].q == q &&
                cand->candidates[c].P == P && cand->candidates[c].Q == Q) dup = 1;
        if (dup) continue;
        cand->candidates[cand->n_candidates].p = p;
        cand->candidates[cand->n_candidates].q = q;
        cand->candidates[cand->n_candidates].P = P;
        cand->candidates[cand->n_candidates].Q = Q;
        cand->candidates[cand->n_candidates].prob = 0.0;
        cand->n_candidates++;
        added++;
    }
}

/* Cierre del lazo: estima cada candidato del shortlist (YW para AR puro, H-R para
 * ARMA; estacional por YW/defaults) y lo PUNTÚA por similitud cruda ACF/PACF (sin
 * penalización de parsimonia). Reordena candidates[] de mejor a peor (prob = score),
 * de modo que el top-k sea el conjunto que el atsw-MCP estimaría/elegiría. */
static void rank_shortlist_by_fit(ModelCandidate *cand, double *data, int n,
                                  double *acf_emp, double *pacf_emp,
                                  PatternFeatures *emp_features, int lags, int s) {
    if (!cand || cand->n_candidates <= 0) return;
    // Arranque COMÚN para que TODOS los candidatos puntúen con el mismo número de
    // observaciones (AICc comparable). Sin esto, los modelos estacionales pierden
    // ~P*s observaciones y son penalizados injustamente frente a los regulares.
    int common_start = 0;
    for (int c = 0; c < cand->n_candidates; c++) {
        int dar = cand->candidates[c].p + cand->candidates[c].P * s;
        int dma = cand->candidates[c].q + cand->candidates[c].Q * s;
        int st = dar > dma ? dar : dma;
        if (st > common_start) common_start = st;
    }
    for (int c = 0; c < cand->n_candidates; c++) {
        int p = cand->candidates[c].p, q = cand->candidates[c].q;
        int P = cand->candidates[c].P, Q = cand->candidates[c].Q;
        double phi[10] = {0}, theta[10] = {0}, Phi[5] = {0}, Theta[5] = {0};

        if (q == 0) {
            if (p > 0) estimate_ar_yule_walker(acf_emp, p, phi);
        } else {
            if (!estimate_arma_hannan_rissanen(data, n, p, q, phi, theta)) {
                // Si H-R falla, defaults suaves para no descartar el candidato
                for (int i = 0; i < p; i++) phi[i] = 0.3 / (i + 1);
                for (int i = 0; i < q; i++) theta[i] = 0.3 / (i + 1);
            }
        }
        if (P > 0) {
            double acf_seasonal[16]; acf_seasonal[0] = 1.0;
            for (int i = 1; i <= P && i < 16; i++) {
                int lag = i * s; acf_seasonal[i] = (lag <= lags) ? acf_emp[lag] : 0.0;
            }
            estimate_ar_yule_walker(acf_seasonal, P, Phi);
        }
        if (Q > 0) {
            // MA(1) estacional por inversión de rho_s; resto, default suave
            Theta[0] = (s <= lags) ? estimate_seasonal_ma1(acf_emp[s]) : MIN_SEASONAL_MA_COEF;
            for (int i = 1; i < Q; i++) Theta[i] = MIN_SEASONAL_MA_COEF;
        }

        // Puntuación por -AICc (seasonal-aware): penaliza los modelos que sobreajustan
        // el ruido de los lags regulares y trata con justicia los lags estacionales.
        // Ahora que la H-R está corregida (signo+iteración), el AICc ordena bien
        // tanto regular como estacional. El MCP decide el final con su MLE.
        double score = -1e30;
        if (!(p == 0 && q == 0 && P == 0 && Q == 0)) {
            int stable = 1;
            if (p > 0 && !check_ar_roots(phi, p)) stable = 0;
            if (P > 0 && !check_ar_roots(Phi, P)) stable = 0;
            if (q > 0 && !check_ma_roots(theta, q)) stable = 0;
            if (Q > 0 && !check_ma_roots(Theta, Q)) stable = 0;
            if (stable) {
                double aicc = compute_sarima_aicc(data, n, p, phi, q, theta, P, Phi, Q, Theta, s, common_start);
                score = -aicc;
                if ((P > 0 || Q > 0) && getenv("ART_DEBUG_SEAS"))
                    fprintf(stderr, "SEAS (%d,%d)(%d,%d) Phi=%.3f Theta=%.3f acf[s]=%.3f aicc=%.2f\n",
                            p, q, P, Q, P>0?Phi[0]:0.0, Q>0?Theta[0]:0.0,
                            s<=lags?acf_emp[s]:0.0, aicc);
            }
        }
        cand->candidates[c].prob = score;
        (void)compute_arma_aicc; (void)emp_features; (void)pacf_emp;
    }
    // Orden descendente por puntuación
    for (int i = 0; i < cand->n_candidates; i++)
        for (int j = i + 1; j < cand->n_candidates; j++)
            if (cand->candidates[j].prob > cand->candidates[i].prob) {
                OrderCandidate t = cand->candidates[i];
                cand->candidates[i] = cand->candidates[j];
                cand->candidates[j] = t;
            }
    if (getenv("ART_DEBUG_RANK")) {
        fprintf(stderr, "RANK:");
        for (int i = 0; i < cand->n_candidates; i++)
            fprintf(stderr, " (%d,%d)(%d,%d)=%.3f", cand->candidates[i].p, cand->candidates[i].q,
                    cand->candidates[i].P, cand->candidates[i].Q, cand->candidates[i].prob);
        fprintf(stderr, "\n");
    }
}

/* Construye el vector de features para el MLP. DEBE coincidir lag a lag con
 * train.py extract_pattern_features (42 dims): 30 regulares + 12 estacionales. */
void extract_feature_vector(PatternFeatures *features, double *vector, int dim, int s, int lags) {
    int idx = 0;
    // ACF lags 1..12
    for (int i = 1; i <= 12 && idx < dim; i++) vector[idx++] = features->acf_values[i];
    // PACF lags 1..12
    for (int i = 1; i <= 12 && idx < dim; i++) vector[idx++] = features->pacf_values[i];
    // Cutting lags
    vector[idx++] = (double)features->acf_cutting_lag;
    vector[idx++] = (double)features->pacf_cutting_lag;
    // Decay rates
    vector[idx++] = features->acf_decay_rate;
    vector[idx++] = features->pacf_decay_rate;
    // Seasonal strengths
    vector[idx++] = features->seasonal_acf_strength;
    vector[idx++] = features->seasonal_pacf_strength;

    // Bloque estacional s-relativo (lags s,2s,3s + satelites). 0 si no estacional.
    #define SG_ACF(lag)  (((lag) >= 1 && (lag) <= lags) ? features->acf_values[lag]  : 0.0)
    #define SG_PACF(lag) (((lag) >= 1 && (lag) <= lags) ? features->pacf_values[lag] : 0.0)
    int seas = (s > 1);
    if (idx < dim) vector[idx++] = seas ? SG_ACF(s)       : 0.0;  // ACF[s]
    if (idx < dim) vector[idx++] = seas ? SG_ACF(2*s)     : 0.0;  // ACF[2s]
    if (idx < dim) vector[idx++] = seas ? SG_ACF(3*s)     : 0.0;  // ACF[3s]
    if (idx < dim) vector[idx++] = seas ? SG_PACF(s)      : 0.0;  // PACF[s]
    if (idx < dim) vector[idx++] = seas ? SG_PACF(2*s)    : 0.0;  // PACF[2s]
    if (idx < dim) vector[idx++] = seas ? SG_PACF(3*s)    : 0.0;  // PACF[3s]
    if (idx < dim) vector[idx++] = seas ? SG_ACF(s-1)     : 0.0;  // ACF[s-1]
    if (idx < dim) vector[idx++] = seas ? SG_ACF(s+1)     : 0.0;  // ACF[s+1]
    if (idx < dim) vector[idx++] = seas ? SG_ACF(2*s-1)   : 0.0;  // ACF[2s-1]
    if (idx < dim) vector[idx++] = seas ? SG_ACF(2*s+1)   : 0.0;  // ACF[2s+1]
    if (idx < dim) vector[idx++] = seas ? SG_PACF(s-1)    : 0.0;  // PACF[s-1]
    if (idx < dim) vector[idx++] = seas ? SG_PACF(s+1)    : 0.0;  // PACF[s+1]
    #undef SG_ACF
    #undef SG_PACF

    // Fill remainder
    while (idx < dim) vector[idx++] = 0.0;
}

/**
 * @brief Re-evalúa los modelos candidatos utilizando la distancia de Mahalanobis
 *        y combina el resultado con la similitud original.
 *
 * @param vectors      Array de vectores de características teóricas (cada uno es un gsl_vector*)
 * @param n            Número de vectores (modelos)
 * @param emp_vector   Vector de características empíricas (extraído de los datos)
 * @param records      Array de estructuras ModelRecord que contienen los parámetros y la similitud original
 * @param n_records    Número de registros (debe coincidir con n)
 * @param alpha        Peso para la similitud original (0..1). La similitud final = alpha*sim_original + (1-alpha)*sim_mahalanobis
 * @param best         Puntero a ModelCandidate donde se almacenará el mejor modelo según la nueva métrica
 *
 * @note   Los vectores no se modifican (se restaura su valor original después de restar el empírico).
 *         La matriz de covarianza se regulariza añadiendo lambda = 1e-6 a la diagonal.
 *         El factor de escala tau se establece en 1.0 (puede ajustarse según los datos).
 */
void reorder_with_mahalanobis(gsl_vector **vectors, int n,
                              gsl_vector *emp_vector,
                              ModelRecord *records, int n_records,
                              double alpha, ModelCandidate *best) {
    if (n <= 0 || n_records != n || !vectors || !emp_vector || !records || !best) return;

    int K = vectors[0]->size;  // dimensión del vector de características

    // ---------- 1. Construir matriz de datos X (n x K) ----------
    gsl_matrix *X = gsl_matrix_alloc(n, K);
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < K; j++) {
            double val = gsl_vector_get(vectors[i], j);
            gsl_matrix_set(X, i, j, val);
        }
    }

    // ---------- 2. Calcular matriz de covarianza muestral ----------
    // Calcular medias
    gsl_vector *mean = gsl_vector_alloc(K);
    for (int j = 0; j < K; j++) {
        double sum = 0.0;
        for (int i = 0; i < n; i++) sum += gsl_matrix_get(X, i, j);
        gsl_vector_set(mean, j, sum / n);
    }

    // Matriz de covarianza (simétrica)
    gsl_matrix *cov = gsl_matrix_alloc(K, K);
    gsl_matrix_set_zero(cov);
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < K; j++) {
            double diff_j = gsl_matrix_get(X, i, j) - gsl_vector_get(mean, j);
            for (int k = 0; k <= j; k++) {
                double diff_k = gsl_matrix_get(X, i, k) - gsl_vector_get(mean, k);
                double inc = diff_j * diff_k;
                double current = gsl_matrix_get(cov, j, k);
                gsl_matrix_set(cov, j, k, current + inc);
                if (j != k) {
                    current = gsl_matrix_get(cov, k, j);
                    gsl_matrix_set(cov, k, j, current + inc);
                }
            }
        }
    }
    for (int j = 0; j < K; j++) {
        for (int k = 0; k < K; k++) {
            double val = gsl_matrix_get(cov, j, k);
            gsl_matrix_set(cov, j, k, val / (n - 1));
        }
    }

    // ---------- 3. Regularización y cálculo de la inversa ----------
    double lambda = 1e-6;  // constante de regularización
    for (int i = 0; i < K; i++) {
        double val = gsl_matrix_get(cov, i, i);
        gsl_matrix_set(cov, i, i, val + lambda);
    }

    gsl_matrix *inv_cov = gsl_matrix_alloc(K, K);
    gsl_permutation *perm = gsl_permutation_alloc(K);
    int signum;
    int status = gsl_linalg_LU_decomp(cov, perm, &signum);
    if (status != GSL_SUCCESS) {
        fprintf(stderr, "Error: no se pudo descomponer la matriz de covarianza (LU).\n");
        gsl_matrix_free(X);
        gsl_vector_free(mean);
        gsl_matrix_free(cov);
        gsl_permutation_free(perm);
        gsl_matrix_free(inv_cov);
        return;
    }
    status = gsl_linalg_LU_invert(cov, perm, inv_cov);
    if (status != GSL_SUCCESS) {
        fprintf(stderr, "Error: no se pudo invertir la matriz de covarianza.\n");
        gsl_matrix_free(X);
        gsl_vector_free(mean);
        gsl_matrix_free(cov);
        gsl_permutation_free(perm);
        gsl_matrix_free(inv_cov);
        return;
    }

    // ---------- 4. Calcular distancia de Mahalanobis para cada modelo ----------
    gsl_vector *diff = gsl_vector_alloc(K);
    gsl_vector *temp = gsl_vector_alloc(K);
    double tau = 1.0;  // factor de escala (puede ajustarse)

    for (int i = 0; i < n_records; i++) {
        // diff = vector_i - emp_vector
        gsl_vector_memcpy(diff, vectors[i]);
        gsl_vector_sub(diff, emp_vector);

        // temp = inv_cov * diff
        gsl_blas_dgemv(CblasNoTrans, 1.0, inv_cov, diff, 0.0, temp);

        // D^2 = diff^T * temp
        double D2;
        gsl_blas_ddot(diff, temp, &D2);
        double D = sqrt(D2);
        double sim_mah = exp(-D / tau);

        // Combinar con la similitud original
        double sim_final = alpha * records[i].similarity_original + (1.0 - alpha) * sim_mah;

        // Actualizar mejor candidato si es necesario
        if (sim_final > best->similarity) {
            best->similarity = sim_final;
            best->p = records[i].p;
            best->q = records[i].q;
            best->P = records[i].P;
            best->Q = records[i].Q;

            // Liberar arrays antiguos
            if (best->best_phi) free(best->best_phi);
            if (best->best_theta) free(best->best_theta);
            if (best->best_Phi) free(best->best_Phi);
            if (best->best_Theta) free(best->best_Theta);

            // Copiar nuevos coeficientes
            if (records[i].p > 0) {
                best->best_phi = malloc(records[i].p * sizeof(double));
                memcpy(best->best_phi, records[i].best_phi, records[i].p * sizeof(double));
            } else best->best_phi = NULL;
            if (records[i].q > 0) {
                best->best_theta = malloc(records[i].q * sizeof(double));
                memcpy(best->best_theta, records[i].best_theta, records[i].q * sizeof(double));
            } else best->best_theta = NULL;
            if (records[i].P > 0) {
                best->best_Phi = malloc(records[i].P * sizeof(double));
                memcpy(best->best_Phi, records[i].best_Phi, records[i].P * sizeof(double));
            } else best->best_Phi = NULL;
            if (records[i].Q > 0) {
                best->best_Theta = malloc(records[i].Q * sizeof(double));
                memcpy(best->best_Theta, records[i].best_Theta, records[i].Q * sizeof(double));
            } else best->best_Theta = NULL;
        }
    }

    // ---------- 5. Liberar memoria ----------
    gsl_vector_free(diff);
    gsl_vector_free(temp);
    gsl_matrix_free(X);
    gsl_vector_free(mean);
    gsl_matrix_free(cov);
    gsl_permutation_free(perm);
    gsl_matrix_free(inv_cov);
}
