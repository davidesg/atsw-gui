/**
 * @file main_cli.c
 * @brief Versión CLI de ART para pruebas Monte Carlo.
 *
 * Uso:
 *   ./art_cli -i archivo.dat                     # Analizar una serie desde archivo
 *   ./art_cli -s -n 200 -p 1 -q 0 -phi 0.5      # Simular AR(1) con phi=0.5
 *   ./art_cli -s --reps 1000 -p 1 -q 0 -phi 0.5 --mahalanobis
 *
 * Opciones completas con --help.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <getopt.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#include "model_detection.h"
#include "ARMA.h"
#include "seasonal_detection.h"

static void convolve_polynomials(double *a, int deg_a, double *b, int deg_b, double *c, int *deg_c);

/* ------------------------------------------------------------
 *  Funciones auxiliares para simulación de series ARMA/SARIMA
 * ------------------------------------------------------------ */

/**
 * Genera una serie ARMA(p,q) (no estacional) usando innovaciones gaussianas.
 * @param phi   Coeficientes AR (longitud p)
 * @param p     Orden AR
 * @param theta Coeficientes MA (longitud q)
 * @param q     Orden MA
 * @param n     Número de observaciones a generar
 * @param rng   Generador GSL
 * @return      Puntero a la serie (double*) que debe liberarse con free()
 */

static double* simulate_arma(double *phi, int p, double *theta, int q, int n, gsl_rng *rng) {
    double *series = (double*)calloc(n, sizeof(double));
    double *innov = (double*)malloc(n * sizeof(double));
    int i, j;
    double tmp;

    for (i = 0; i < n; i++) innov[i] = gsl_ran_gaussian(rng, 1.0);

    for (i = 0; i < n; i++) {
        tmp = innov[i];
        // Parte MA: signo NEGATIVO para seguir Box-Jenkins
        for (j = 1; j <= q && i - j >= 0; j++)
            tmp -= theta[j-1] * innov[i - j];
        // Parte AR: signo POSITIVO (coeficientes phi)
        for (j = 1; j <= p && i - j >= 0; j++)
            tmp += phi[j-1] * series[i - j];
        series[i] = tmp;
    }
    free(innov);
    return series;
}

/**
 * Genera una serie SARIMA(p,d,q)(P,D,Q)_s con diferencias.
 * Se aplican las diferencias regulares y estacionales después de generar
 * la serie estacionaria subyacente (d=0, D=0).
 * @param phi   Coeficientes AR regulares (p)
 * @param theta Coeficientes MA regulares (q)
 * @param Phi   Coeficientes AR estacionales (P)
 * @param Theta Coeficientes MA estacionales (Q)
 * @param s     Período estacional
 * @param d     Orden de diferencias regulares
 * @param D     Orden de diferencias estacionales
 * @param n     Número de observaciones finales
 * @param rng   Generador GSL
 * @return      Serie (double*) de longitud n (sin transformación log)
 */
 static double* simulate_sarima(double *phi, int p, double *theta, int q,
                               double *Phi, int P, double *Theta, int Q, int s,
                               int d, int D, int n, gsl_rng *rng) {
    int i, j;  // <-- declaración añadida

    // Construir polinomios AR regular y estacional (incluyendo constante 1).
    // CLAVE: los coeficientes estacionales van en los lags s, 2s, ..., P*s
    // (no en 1..P), para que la estructura aparezca en el lag estacional.
    int deg_ar_reg = p;
    int deg_ar_sea = P * s;
    double *ar_reg = (double*)calloc(deg_ar_reg + 1, sizeof(double));
    double *ar_sea = (double*)calloc(deg_ar_sea + 1, sizeof(double));
    ar_reg[0] = 1.0;
    ar_sea[0] = 1.0;
    for (i = 0; i < p; i++) ar_reg[i+1] = -phi[i];
    for (i = 0; i < P; i++) ar_sea[(i+1)*s] = -Phi[i];

    // Convolucionar para obtener polinomio AR total
    int deg_ar_total;
    double *ar_total = (double*)calloc(deg_ar_reg + deg_ar_sea + 1, sizeof(double));
    convolve_polynomials(ar_reg, deg_ar_reg, ar_sea, deg_ar_sea, ar_total, &deg_ar_total);

    // Construir polinomios MA regular y estacional (estacional en lags s,2s,...,Q*s)
    int deg_ma_reg = q;
    int deg_ma_sea = Q * s;
    double *ma_reg = (double*)calloc(deg_ma_reg + 1, sizeof(double));
    double *ma_sea = (double*)calloc(deg_ma_sea + 1, sizeof(double));
    ma_reg[0] = 1.0;
    ma_sea[0] = 1.0;
    for (i = 0; i < q; i++) ma_reg[i+1] = -theta[i];
    for (i = 0; i < Q; i++) ma_sea[(i+1)*s] = -Theta[i];

    // Convolucionar para obtener polinomio MA total
    int deg_ma_total;
    double *ma_total = (double*)calloc(deg_ma_reg + deg_ma_sea + 1, sizeof(double));
    convolve_polynomials(ma_reg, deg_ma_reg, ma_sea, deg_ma_sea, ma_total, &deg_ma_total);

    // Extraer coeficientes (sin constante) y cambiar signo para obtener los valores reales
    int p_total = deg_ar_total;
    int q_total = deg_ma_total;
    double *phi_total = (double*)malloc(p_total * sizeof(double));
    double *theta_total = (double*)malloc(q_total * sizeof(double));
    for (i = 0; i < p_total; i++) phi_total[i] = -ar_total[i+1];
    for (i = 0; i < q_total; i++) theta_total[i] = -ma_total[i+1];

    // Generar serie estacionaria subyacente
    double *series = simulate_arma(phi_total, p_total, theta_total, q_total,
                                   n + d + D*s, rng);

    // Liberar memoria de polinomios temporales y coeficientes
    free(ar_reg); free(ar_sea); free(ar_total);
    free(ma_reg); free(ma_sea); free(ma_total);
    free(phi_total); free(theta_total);

    // Aplicar diferencias regulares (d)
    int current_len = n + d + D*s;
    for (i = 0; i < d; i++) {
        for (j = 1; j < current_len; j++) series[j-1] = series[j] - series[j-1];
        current_len--;
    }
    // Aplicar diferencias estacionales (D)
    for (i = 0; i < D; i++) {
        for (j = s; j < current_len; j++) series[j-s] = series[j] - series[j-s];
        current_len -= s;
    }
    // Truncar a n observaciones
    if (current_len > n) {
        memmove(series, series + (current_len - n), n * sizeof(double));
        current_len = n;
    }
    if (current_len < n) {
        for (i = current_len; i < n; i++) series[i] = 0.0;
    }
    return series;
}
/* ------------------------------------------------------------
 *  Función de progreso (vacía para CLI)
 * ------------------------------------------------------------ */
static void cli_progress_callback(int stage, double progress, const char *message, const char *overall) {
    // Opcional: imprimir progreso en stderr
    // fprintf(stderr, "[Stage %d] %.1f%%: %s\n", stage, progress*100, message);
}

/* ------------------------------------------------------------
 *  Estructura para almacenar resultados de una ejecución
 * ------------------------------------------------------------ */
typedef struct {
    int true_p, true_q, true_P, true_Q;
    int det_p, det_q, det_P, det_Q;
    double similarity;
    int correct;
    int in_shortlist;       // 1 si el modelo verdadero está en el shortlist BJ del MLP
    int true_rank;          // rango (1-based) del verdadero en el shortlist ordenado; 0 si ausente
    double elapsed_ms;
} RunResult;

/* ------------------------------------------------------------
 *  Función principal de detección para una serie
 * ------------------------------------------------------------ */
static int detect_series(double *data, int n, DataParameters *params,
                         int p_max, int q_max, int P_max, int Q_max,
                         ModelCandidate *candidate, const char *filename) {
    set_progress_callback(cli_progress_callback);

    params->data = data;
    params->n_points = n;

    int success = ejecutar_deteccion_automatica(
        (filename && filename[0]) ? filename : "", params, p_max, q_max, P_max, Q_max, candidate);
    params->data = NULL;
    return success;
}

/* ------------------------------------------------------------
 *  Impresión de ayuda
 * ------------------------------------------------------------ */
static void print_help(const char *prog) {
    printf("Uso: %s [opciones]\n", prog);
    printf("Opciones:\n");
    printf("  -i, --input FILE        Leer serie desde archivo (una columna)\n");
    printf("  -s, --simulate          Generar serie sintética (requiere -n y modelo)\n");
    printf("  -n, --nobs N            Número de observaciones (simulación)\n");
    printf("  -p, --ar-order P        Orden AR regular (por defecto 0)\n");
    printf("  -q, --ma-order Q        Orden MA regular (por defecto 0)\n");
    printf("  -P, --sar-order P       Orden AR estacional (por defecto 0)\n");
    printf("  -Q, --sma-order Q       Orden MA estacional (por defecto 0)\n");
    printf("  -d, --diff D            Diferencias regulares (por defecto 0)\n");
    printf("  -D, --sdiff D           Diferencias estacionales (por defecto 0)\n");
    printf("  -s, --seasonal S        Período estacional (por defecto 1)\n");
    printf("  --phi LIST              Coeficientes AR separados por comas (ej. 0.5,0.2)\n");
    printf("  --theta LIST            Coeficientes MA separados por comas\n");
    printf("  --Phi LIST              Coeficientes AR estacionales\n");
    printf("  --Theta LIST            Coeficientes MA estacionales\n");
    printf("  --mahalanobis           Activar distancia de Mahalanobis\n");
    printf("  --deseasonalize         Activar desestacionalización previa\n");
    printf("  --mlp-direct            Identificación BJ: salida = shortlist del MLP (sin parsimonia)\n");
    printf("  --reps N                Número de réplicas Monte Carlo (simulación)\n");
    printf("  --seed SEED             Semilla aleatoria\n");
    printf("  --output FILE           Guardar resultados en archivo CSV\n");
    printf("  --help                  Mostrar esta ayuda\n");
}

/* ------------------------------------------------------------
 *  Parseo de lista de coeficientes (ej. "0.5,0.2,-0.1")
 * ------------------------------------------------------------ */
static int parse_coef_list(const char *str, double **coefs) {
    if (!str) return 0;
    int count = 1;
    const char *p = str;
    while (*p) if (*p++ == ',') count++;
    *coefs = (double*)malloc(count * sizeof(double));
    char *copy = strdup(str);
    char *token = strtok(copy, ",");
    int i = 0;
    while (token && i < count) {
        (*coefs)[i++] = atof(token);
        token = strtok(NULL, ",");
    }
    free(copy);
    return count;
}

/* ------------------------------------------------------------
 *  main
 * ------------------------------------------------------------ */
int main(int argc, char *argv[]) {
    int opt;
    int option_index = 0;
    static struct option long_options[] = {
        {"input",        required_argument, 0, 'i'},
        {"simulate",     no_argument,       0, 's'},
        {"nobs",         required_argument, 0, 'n'},
        {"ar-order",     required_argument, 0, 'p'},
        {"ma-order",     required_argument, 0, 'q'},
        {"sar-order",    required_argument, 0, 'P'},
        {"sma-order",    required_argument, 0, 'Q'},
        {"diff",         required_argument, 0, 'd'},
        {"sdiff",        required_argument, 0, 'D'},
        {"seasonal",     required_argument, 0, 'S'},
        {"phi",          required_argument, 0, 1000},
        {"theta",        required_argument, 0, 1001},
        {"Phi",          required_argument, 0, 1002},
        {"Theta",        required_argument, 0, 1003},
        {"mahalanobis",  no_argument,       0, 1004},
        {"deseasonalize",no_argument,       0, 1005},
        {"mlp-direct",   no_argument,       0, 1009},
        {"pmax",         required_argument, 0, 1010},
        {"qmax",         required_argument, 0, 1011},
        {"log",          no_argument,       0, 1012},
        {"reps",         required_argument, 0, 1006},
        {"seed",         required_argument, 0, 1007},
        {"output",       required_argument, 0, 1008},
        {"help",         no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    char *input_file = NULL;
    int simulate = 0;
    int n_obs = 0;
    int true_p = 0, true_q = 0, true_P = 0, true_Q = 0;
    int d = 0, D = 0, s = 1;
    double *phi = NULL, *theta = NULL, *Phi = NULL, *Theta = NULL;
    int phi_len = 0, theta_len = 0, Phi_len = 0, Theta_len = 0;
    int use_mahalanobis = 0;
    int use_deseasonalize = 0;
    int use_mlp_direct = 0;
    int pmax_override = -1, qmax_override = -1;
    int use_log = 0;
    int reps = 1;
    unsigned long seed = time(NULL);
    char *output_file = NULL;

    while ((opt = getopt_long(argc, argv, "i:sn:p:q:P:Q:d:D:S:h", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'i': input_file = optarg; break;
            case 's': simulate = 1; break;
            case 'n': n_obs = atoi(optarg); break;
            case 'p': true_p = atoi(optarg); break;
            case 'q': true_q = atoi(optarg); break;
            case 'P': true_P = atoi(optarg); break;
            case 'Q': true_Q = atoi(optarg); break;
            case 'd': d = atoi(optarg); break;
            case 'D': D = atoi(optarg); break;
            case 'S': s = atoi(optarg); break;
            case 1000: phi_len = parse_coef_list(optarg, &phi); break;
            case 1001: theta_len = parse_coef_list(optarg, &theta); break;
            case 1002: Phi_len = parse_coef_list(optarg, &Phi); break;
            case 1003: Theta_len = parse_coef_list(optarg, &Theta); break;
            case 1004: use_mahalanobis = 1; break;
            case 1005: use_deseasonalize = 1; break;
            case 1009: use_mlp_direct = 1; break;
            case 1010: pmax_override = atoi(optarg); break;
            case 1011: qmax_override = atoi(optarg); break;
            case 1012: use_log = 1; break;
            case 1006: reps = atoi(optarg); break;
            case 1007: seed = atol(optarg); break;
            case 1008: output_file = optarg; break;
            case 'h':
            default:
                print_help(argv[0]);
                return 0;
        }
    }

    if (!simulate && !input_file) {
        fprintf(stderr, "Error: debe especificar --input o --simulate\n");
        print_help(argv[0]);
        return 1;
    }
    if (simulate && n_obs <= 0) {
        fprintf(stderr, "Error: para simulación debe indicar -n\n");
        return 1;
    }

    // Inicializar generador aleatorio
    gsl_rng *rng = gsl_rng_alloc(gsl_rng_mt19937);
    gsl_rng_set(rng, seed);

    // Configurar parámetros de búsqueda (máximos)
    /* art-python's limits (suggest_orders): p <= max(3, s/2), q <= 2, P <= 1,
     * Q <= 1. --pmax/--qmax still override (the benchmarks pass 5). */
    int p_max = (true_p > 0) ? true_p : (s / 2 > 3 ? s / 2 : 3);
    int q_max = (true_q > 0) ? true_q : 2;
    int P_max = (true_P > 0) ? true_P : 1;
    int Q_max = (true_Q > 0) ? true_Q : 1;
    // Overrides explícitos (evitan el oráculo p_max=true_p en los tests Monte Carlo)
    if (pmax_override >= 0) p_max = pmax_override;
    if (qmax_override >= 0) q_max = qmax_override;
    if (s == 1) { P_max = 0; Q_max = 0; }

    // Abrir archivo de salida si se pide
    FILE *out = stdout;
    if (output_file) {
        out = fopen(output_file, "w");
        if (!out) {
            fprintf(stderr, "Error: no se pudo abrir %s\n", output_file);
            return 1;
        }
        fprintf(out, "rep,true_p,true_q,true_P,true_Q,det_p,det_q,det_P,det_Q,similarity,correct,in_shortlist,elapsed_ms\n");
    }

    RunResult *results = (RunResult*)malloc(reps * sizeof(RunResult));
    double total_time = 0.0;
    int correct_count = 0;
    int shortlist_count = 0;
    int rank_hist[MAX_ORDER_CANDIDATES + 2] = {0};  // rank_hist[r] = #series con verdadero en rango r

    for (int rep = 0; rep < reps; rep++) {
        double *series = NULL;
        int n_effective = n_obs;

        if (simulate) {
            if (true_p > 0 || true_q > 0 || true_P > 0 || true_Q > 0) {
                // Simular según modelo especificado
                series = simulate_sarima(phi, true_p, theta, true_q,
                                         Phi, true_P, Theta, true_Q, s,
                                         d, D, n_obs, rng);
            } else {
                // Si no hay modelo, simular ruido blanco
                series = (double*)malloc(n_obs * sizeof(double));
                for (int i = 0; i < n_obs; i++) series[i] = gsl_ran_gaussian(rng, 1.0);
            }
        } else {
            // Leer archivo
            FILE *f = fopen(input_file, "r");
            if (!f) { perror("fopen"); return 1; }
            double *temp = NULL;
            int count = 0;
            double val;
            while (fscanf(f, "%lf", &val) == 1) {
                temp = (double*)realloc(temp, (count+1)*sizeof(double));
                temp[count++] = val;
            }
            fclose(f);
            n_effective = count;
            series = temp;
        }

        // Configurar parámetros de transformación
        DataParameters params;
        memset(&params, 0, sizeof(DataParameters));
        params.apply_log = use_log;   // --log aplica logaritmo natural (como el checkbox de la GUI)
        params.d = d;
        params.D = D;
        params.s = s;
        params.deseasonalize = use_deseasonalize;
        params.use_mahalanobis = use_mahalanobis;
        params.mlp_direct = use_mlp_direct;
        params.data = NULL;
        params.n_points = 0;

        ModelCandidate candidate;
        memset(&candidate, 0, sizeof(ModelCandidate));

        clock_t start = clock();
        int success = detect_series(series, n_effective, &params,
                                    p_max, q_max, P_max, Q_max, &candidate,
                                    simulate ? NULL : input_file);
        clock_t end = clock();
        double elapsed = 1000.0 * (end - start) / CLOCKS_PER_SEC;

        results[rep].elapsed_ms = elapsed;
        results[rep].similarity = candidate.similarity;
        results[rep].det_p = candidate.p;
        results[rep].det_q = candidate.q;
        results[rep].det_P = candidate.P;
        results[rep].det_Q = candidate.Q;
        results[rep].true_p = true_p;
        results[rep].true_q = true_q;
        results[rep].true_P = true_P;
        results[rep].true_Q = true_Q;
        results[rep].correct = (candidate.p == true_p && candidate.q == true_q &&
                                candidate.P == true_P && candidate.Q == true_Q);
        if (results[rep].correct) correct_count++;

        // Rango (1-based) del modelo verdadero en el shortlist ORDENADO por similitud;
        // 0 si no está. Permite estudiar la potencia (recall@k) al recortar el shortlist.
        int true_rank = 0;
        for (int c = 0; c < candidate.n_candidates; c++) {
            if (candidate.candidates[c].p == true_p && candidate.candidates[c].q == true_q &&
                candidate.candidates[c].P == true_P && candidate.candidates[c].Q == true_Q) {
                true_rank = c + 1; break;
            }
        }
        results[rep].true_rank = true_rank;
        results[rep].in_shortlist = (true_rank > 0);
        if (true_rank > 0) { shortlist_count++; rank_hist[true_rank]++; }

        if (out != stdout) {
            fprintf(out, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%.6f,%d,%d,%.3f\n",
                    rep+1, true_p, true_q, true_P, true_Q,
                    candidate.p, candidate.q, candidate.P, candidate.Q,
                    candidate.similarity, results[rep].correct,
                    results[rep].in_shortlist, elapsed);
        }

        // Limpiar
        if (series) free(series);
        liberar_model_candidate(&candidate);
        if (params.data) free(params.data);
    }

    if (out != stdout) fclose(out);

    // Estadísticas resumen
    double avg_sim = 0.0, avg_time = 0.0;
    for (int i = 0; i < reps; i++) {
        avg_sim += results[i].similarity;
        avg_time += results[i].elapsed_ms;
    }
    avg_sim /= reps;
    avg_time /= reps;
    double accuracy = (double)correct_count / reps * 100.0;

    printf("\n=== Resultados Monte Carlo ===\n");
    printf("Réplicas: %d\n", reps);
    printf("Modelo verdadero: AR(%d), MA(%d), SAR(%d), SMA(%d), s=%d, d=%d, D=%d\n",
           true_p, true_q, true_P, true_Q, s, d, D);
    printf("Opciones: Mahalanobis=%s, Desestacionalizar=%s, MLP-direct=%s\n",
           use_mahalanobis ? "SÍ" : "NO", use_deseasonalize ? "SÍ" : "NO",
           use_mlp_direct ? "SÍ" : "NO");
    printf("Precisión (orden exacto): %.2f%% (%d/%d)\n", accuracy, correct_count, reps);
    printf("Modelo verdadero en shortlist: %.2f%% (%d/%d)\n",
           (double)shortlist_count / reps * 100.0, shortlist_count, reps);
    // Potencia recall@k: P(verdadero entre los k de mayor similitud) al recortar el shortlist.
    printf("Potencia recall@k (shortlist ordenado por similitud):\n");
    int cum = 0;
    for (int k = 1; k <= MAX_ORDER_CANDIDATES; k++) {
        cum += rank_hist[k];
        printf("  recall@%d = %5.1f%%\n", k, (double)cum / reps * 100.0);
    }
    printf("Similitud media: %.4f\n", avg_sim);
    printf("Tiempo medio por ejecución: %.2f ms\n", avg_time);

    free(results);
    if (phi) free(phi);
    if (theta) free(theta);
    if (Phi) free(Phi);
    if (Theta) free(Theta);
    gsl_rng_free(rng);
    return 0;
}


static void convolve_polynomials(double *a, int deg_a, double *b, int deg_b, double *c, int *deg_c) {
    *deg_c = deg_a + deg_b;
    for (int i = 0; i <= *deg_c; i++) c[i] = 0.0;
    for (int i = 0; i <= deg_a; i++) {
        for (int j = 0; j <= deg_b; j++) {
            c[i + j] += a[i] * b[j];
        }
    }
}
