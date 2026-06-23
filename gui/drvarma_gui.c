/**
 * drvarma_gui.c – GTK+ 3 graphical interface for DRVARMA
 *                 (Exact maximum likelihood estimation of VARMA models)
 *
 * Features:
 *   - Load any number of time series from a whitespace‑separated file.
 *   - Set frequency (annual, quarterly, monthly) and start date (metadata).
 *   - Apply log transformation, scaling, and differencing (0,1,2) to achieve stationarity.
 *   - Configure all DRVARMA options: p, q, -mean, -diagar, -diagma, -diagcov,
 *     -m {1|2}, -twostep, -volexp [α window], -volmov [window].
 *   - Generate the required .inp file and run the drvarma executable.
 *   - View the resulting .out and .inp files in an integrated text viewer.
 *   - Johansen cointegration test (any number of series) using GSL.
 *   - VAR order selection (AIC, BIC, HQ) for choosing lag length.
 *
 * Build with (Linux/macOS):
 *   gcc -o drvarma_gui drvarma_gui.c `pkg-config --cflags --libs gtk+-3.0` -lm -lgsl -lgslcblas
 *
 * On Windows (MSYS2 with mingw-w64):
 *   gcc -o drvarma_gui.exe drvarma_gui.c `pkg-config --cflags --libs gtk+-3.0` -lm -lgsl -lgslcblas -mwindows
 */

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <glib/gstdio.h>
#include <errno.h>
#include <locale.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_eigen.h>
#include <gsl/gsl_statistics.h>
#include <gsl/gsl_errno.h>
#include "seasonal_detection.h"
#include "johansen_test.h"
#include "vecm.h"

#ifdef _WIN32
#include <windows.h>
#endif


/* ---------- Data structures ---------- */
typedef struct {
    double **data;          /* raw data matrix [obs][var] */
    int n_obs;              /* number of raw observations */
    int n_var;              /* number of variables (series) */
    char *input_filename;   /* full path of the loaded text file */
    char *base_name;        /* basename (without extension) for .inp/.out */
} Data;

typedef struct {
    /* Model options (match DRVARMA command line) */
    int p;                  /* AR order */
    int q;                  /* MA order */
    int include_mean;       /* 0/1 = -mean flag */
    int diag_ar;            /* 0/1 = -diagar flag */
    int diag_ma;            /* 0/1 = -diagma flag */
    int diag_cov;           /* 0/1 = -diagcov flag */
    int method;             /* 1 = exact, 2 = approximate */
    int twostep;            /* 0/1 = -twostep flag */
    /* Volatility options */
    int use_exp_vol;        /* 0/1 = -volexp flag */
    double exp_alpha;       /* alpha parameter for exp. volatility */
    int exp_window;         /* window for exp. volatility */
    int use_mov_vol;        /* 0/1 = -volmov flag */
    int mov_window;         /* window for moving volatility */
    /* Path to drvarma executable */
    char drv_path[512];
    /* Data properties (metadata, not used by drvarma but displayed) */
    int freq;               /* 1,4,12 */
    int start_year;
    int start_sub;          /* 1..freq */
    int props_set;          /* flag: 1 if properties have been defined */
    /* Data transformation (applied before writing .inp) */
    int use_logs;           /* 0/1 : apply log transform */
    double scale_factor;    /* multiply data (or log(data)) by this factor */
    int diff_order;         /* number of regular differences -> engine d */
    int seasonal_diff;      /* number of seasonal differences -> engine D (lag=freq) */
    int deseasonalize;      // 0/1 : aplicar desestacionalización
    int deseasonalize_s;    // período estacional (s) para la desestacionalización
    int deseasonalize_auto; // 0 = forzar todas, 1 = solo significativas (p<0.05)

    /* Forecast options */
    int do_forecast;        /* 0/1 = -forecast flag */
    int forecast_horizon;   /* L */
    int forecast_seasonal;  /* seasonal period s */
} Options;

Data data = {0};
Options opts = {
    .p = 1,
    .q = 1,
    .include_mean = 0,
    .diag_ar = 0,
    .diag_ma = 0,
    .diag_cov = 0,
    .method = 1,
    .twostep = 0,
    .use_exp_vol = 0,
    .exp_alpha = 0.05,
    .exp_window = 20,
    .use_mov_vol = 0,
    .mov_window = 20,
    #ifdef _WIN32
    .drv_path = "drvarma.exe",
    #else
    .drv_path = "./drvarma",
    #endif
    .freq = 12,
    .start_year = 2000,
    .start_sub = 1,
    .props_set = 0,
    .use_logs = 0,
    .scale_factor = 1.0,
    .diff_order = 0,
    .seasonal_diff = 0,
    .deseasonalize = 0,
    .deseasonalize_s = 12,   // valor por defecto, se actualizará con la frecuencia de los datos
    .deseasonalize_auto = 1,   // por defecto automático

    .do_forecast = 0,
    .forecast_horizon = 24,
    .forecast_seasonal = 1,
};

/* UI widgets that need to be accessed from callbacks */
GtkWidget *status_label;
GtkWidget *text_view;
GtkWidget *main_window;
GtkWidget *btn_run;
GtkWidget *btn_run_forecast;   /* botón en pestaña Forecast */
GtkWidget *drv_path_entry;   /* entry to show/set drvarma path */

static GtkWidget *label_freq_info_seasonal = NULL;
static double* get_seasonal_dummies(double *diff_series, int n_diff, int s, int d, int *success, double **coeffs_out);
static double** preprocess_series_levels(double **input, int n_obs, int n_var,
                                         int *out_nobs, GString *log_output);



static void on_forecast_toggled(GtkToggleButton *btn, gpointer user_data);
static void on_forecast_horizon_changed(GtkSpinButton *btn, gpointer user_data);
static void on_forecast_seasonal_changed(GtkSpinButton *btn, gpointer user_data);
static void on_view_forecast(GtkWidget *widget, gpointer user_data);
static void on_run_and_forecast(GtkWidget *widget, gpointer user_data);


static void update_freq_info_label(void) {
    if (label_freq_info_seasonal) {
        char buf[128];
        snprintf(buf, sizeof(buf), "(Data frequency: %d period(s) per year)", opts.freq);
        gtk_label_set_text(GTK_LABEL(label_freq_info_seasonal), buf);
    }
}

static void on_deseason_force_toggled(GtkToggleButton *btn, gpointer user_data) {
    if (gtk_toggle_button_get_active(btn))
        opts.deseasonalize_auto = 0;
}
static void on_deseason_auto_toggled(GtkToggleButton *btn, gpointer user_data) {
    if (gtk_toggle_button_get_active(btn))
        opts.deseasonalize_auto = 1;
}

/* ---------- Helper: set basename from full path ---------- */
static void set_basename_from_path(const char *fullpath) {
    if (data.base_name) {
        g_free(data.base_name);
        data.base_name = NULL;
    }
    char *basename = g_path_get_basename(fullpath);
    char *dot = strrchr(basename, '.');
    if (dot) *dot = '\0';
    data.base_name = g_strdup(basename);
    g_free(basename);
}

/* ---------- Load data file (any number of columns) ---------- */
/* ---------- Load data file (any number of columns) ---------- */
static gboolean load_data_file(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return FALSE;

    /* Forzar locale numérico a C (punto decimal) para consistencia entre plataformas */
    setlocale(LC_NUMERIC, "C");
    
    /* First pass: count rows and columns */
    int n_cols = 0, n_rows = 0;
    char line[8192];
    while (fgets(line, sizeof(line), f)) {
        if (n_rows == 0) {
            /* Count columns - acepta cualquier whitespace (espacios, tabs) */
            char *p = line;
            int in_field = 0;
            while (*p) {
                if (!isspace((unsigned char)*p) && !in_field) {
                    in_field = 1;
                    n_cols++;
                } else if (isspace((unsigned char)*p)) {
                    in_field = 0;
                }
                p++;
            }
            if (n_cols == 0) { fclose(f); return FALSE; }
        }
        n_rows++;
    }
    if (n_rows < 2) { fclose(f); return FALSE; }

    /* Allocate data matrix */
    data.data = g_new(double *, n_rows);
    for (int i = 0; i < n_rows; i++)
        data.data[i] = g_new(double, n_cols);

    /* Second pass: read numbers using strtod (robusto, funciona en todas las plataformas) */
    rewind(f);
    for (int i = 0; i < n_rows; i++) {
        if (!fgets(line, sizeof(line), f)) {
            for (int k = 0; k <= i; k++) g_free(data.data[k]);
            g_free(data.data);
            data.data = NULL;
            fclose(f);
            return FALSE;
        }
        
        /* Reemplazar tabs por espacios para simplificar el parsing */
        for (char *p = line; *p; p++) {
            if (*p == '\t') *p = ' ';
        }
        
        char *p = line;
        int j = 0;
        while (j < n_cols && *p) {
            /* Saltar espacios */
            while (*p && isspace((unsigned char)*p)) p++;
            if (!*p) break;
            
            char *endptr;
            double val = strtod(p, &endptr);
            if (p == endptr) {
                for (int k = 0; k <= i; k++) g_free(data.data[k]);
                g_free(data.data);
                data.data = NULL;
                fclose(f);
                return FALSE;
            }
            data.data[i][j++] = val;
            p = endptr;
        }
        
        if (j != n_cols) {
            for (int k = 0; k <= i; k++) g_free(data.data[k]);
            g_free(data.data);
            data.data = NULL;
            fclose(f);
            return FALSE;
        }
    }
    fclose(f);

    data.n_obs = n_rows;
    data.n_var = n_cols;
    data.input_filename = g_strdup(filename);
    set_basename_from_path(filename);
    
    return TRUE;
}

/* ---------- Apply log and scale transformation to raw data ---------- */
/* Returns a new matrix (same dimensions) or NULL on error (e.g. log of non-positive) */
static double **transform_data(int *new_nobs) {
    double **newdata = g_new(double *, data.n_obs);
    for (int i = 0; i < data.n_obs; i++) {
        newdata[i] = g_new(double, data.n_var);
        for (int j = 0; j < data.n_var; j++) {
            double val = data.data[i][j];
            if (opts.use_logs) {
                if (val <= 0.0) {
                    for (int k = 0; k <= i; k++) g_free(newdata[k]);
                    g_free(newdata);
                    return NULL;
                }
                val = opts.scale_factor * log(val);
            } else {
                val = opts.scale_factor * val;
            }
            newdata[i][j] = val;
        }
    }
    *new_nobs = data.n_obs;
    return newdata;
}

/* ---------- Apply differences to a given matrix (will be freed by caller) ---------- */

static double **apply_differences_to_matrix(double **mat, int n_obs, int n_var, int diff_order, int *new_nobs) {
    if (diff_order == 0) {
        *new_nobs = n_obs;
        double **newmat = g_new(double *, n_obs);
        for (int i = 0; i < n_obs; i++) {
            newmat[i] = g_new(double, n_var);
            memcpy(newmat[i], mat[i], n_var * sizeof(double));
        }
        return newmat;
    }
    if (diff_order == 1) {
        if (n_obs <= 1) { *new_nobs = 0; return NULL; }
        *new_nobs = n_obs - 1;
        double **newmat = g_new(double *, *new_nobs);
        for (int i = 0; i < *new_nobs; i++) {
            newmat[i] = g_new(double, n_var);
            for (int j = 0; j < n_var; j++) {
                newmat[i][j] = mat[i+1][j] - mat[i][j];
            }
        }
        return newmat;
    }
    if (diff_order == 2) {
        if (n_obs <= 2) { *new_nobs = 0; return NULL; }
        *new_nobs = n_obs - 2;
        double **newmat = g_new(double *, *new_nobs);
        for (int i = 0; i < *new_nobs; i++) {
            newmat[i] = g_new(double, n_var);
            for (int j = 0; j < n_var; j++) {
                newmat[i][j] = mat[i+2][j] - 2.0 * mat[i+1][j] + mat[i][j];
            }
        }
        return newmat;
    }
    return NULL;
}


 /**
 * @brief Get seasonal dummies by estimating γ0 directly using differenced basis.
 *
 * @param series Original transformed series (level, not differenced) – NOT used directly.
 * @param n Length of series (unused here, kept for compatibility).
 * @param s Seasonal period
 * @param d Differencing order applied to obtain the series used in regression
 * @param success Output flag: 1 if successful
 * @param coeffs_out If not NULL, returns allocated array of harmonic coefficients (γ0)
 * @return double* Array of seasonal dummies (ω0) of length s, or NULL on failure.
 */
static double* get_seasonal_dummies(double *diff_series, int n_diff, int s, int d,
                                    int *success, double **coeffs_out) {
    *success = 0;

    int num_harmonics = s - 1;
    double *coefficients = malloc(num_harmonics * sizeof(double));
    double *std_errors = malloc(num_harmonics * sizeof(double));
    gsl_matrix *hac_cov = NULL;
    gsl_matrix *ols_cov = NULL;
    double intercept, intercept_std_error, f_stat, p_value, r_squared;

    int regression_ok = harmonic_regression_differenced_basis(diff_series, n_diff, d,
                                                              coefficients, std_errors,
                                                              &hac_cov, &ols_cov,
                                                              &intercept, &intercept_std_error,
                                                              &f_stat, &p_value, &r_squared,
                                                              s);

    if (!regression_ok || !ols_cov) {
        g_print("Warning: Harmonic regression failed for deseasonalization (series length %d, s=%d, d=%d).\n",
                n_diff, s, d);
        free(coefficients);
        free(std_errors);
        if (hac_cov) gsl_matrix_free(hac_cov);
        if (ols_cov) gsl_matrix_free(ols_cov);
        return NULL;
    }

    /* Imprimir coeficientes armónicos estimados (γ0) */
    g_print("Harmonic coefficients (gamma0) for s=%d, d=%d: ", s, d);
    for (int i = 0; i < num_harmonics; i++) {
        g_print("%8.4f ", coefficients[i]);
    }
    g_print("\n");

    if (coeffs_out) {
        *coeffs_out = malloc(num_harmonics * sizeof(double));
        memcpy(*coeffs_out, coefficients, num_harmonics * sizeof(double));
    }

    /* Transformar a dummies de nivel ω0 = A0 * γ0 */
    double *dummies = transform_harmonics_to_dummies_general(coefficients, NULL, s);

    /* Imprimir dummies estacionales de nivel (ω0) */
    g_print("Seasonal dummies (omega_level) for s=%d: ", s);
    for (int i = 0; i < s; i++) {
        g_print("%8.4f ", dummies[i]);
    }
    g_print("\n");

    free(coefficients);
    free(std_errors);
    if (hac_cov) gsl_matrix_free(hac_cov);
    if (ols_cov) gsl_matrix_free(ols_cov);

    *success = 1;
    return dummies;
}


/* ---------- Generate .inp file from transformed and differenced data ---------- */

/* Extrae una columna de una matriz (devuelve array nuevo) */
static double* extract_column_from_matrix(double **matrix, int n_rows, int col) {
    double *col_data = g_new(double, n_rows);
    for (int i = 0; i < n_rows; i++) {
        col_data[i] = matrix[i][col];
    }
    return col_data;
}

/* Aplica log y escala a una matriz de datos arbitraria */
static double** transform_data_from_matrix(double **input, int n_obs, int n_var,
                                           int use_logs, double scale_factor) {
    double **newdata = g_new(double *, n_obs);
    for (int i = 0; i < n_obs; i++) {
        newdata[i] = g_new(double, n_var);
        for (int j = 0; j < n_var; j++) {
            double val = input[i][j];
            if (use_logs) {
                if (val <= 0.0) {
                    // Error: liberar y retornar NULL
                    for (int k = 0; k <= i; k++) g_free(newdata[k]);
                    g_free(newdata);
                    return NULL;
                }
                val = scale_factor * log(val);
            } else {
                val = scale_factor * val;
            }
            newdata[i][j] = val;
        }
    }
    return newdata;
}

/* Libera una matriz procesada (n_rows filas) */
static void free_processed_matrix(double **matrix, int n_rows) {
    if (matrix) {
        for (int i = 0; i < n_rows; i++) free(matrix[i]);
        free(matrix);
    }
}


/**
 * Procesa todas las series según las opciones actuales:
 * - Transformación log/scale
 * - Test de estacionalidad y desestacionalización selectiva (si activada)
 * - Diferenciación final
 *
 * @param input      Matriz original [n_obs][n_var]
 * @param n_obs      Número de observaciones originales
 * @param n_var      Número de variables
 * @param out_nobs   (out) Número de observaciones tras el procesamiento
 * @param log_output (out) GString con el log de operaciones (puede ser NULL)
 * @return Nueva matriz procesada, o NULL en caso de error.
 */
static double** preprocess_series(double **input, int n_obs, int n_var,
                                  int *out_nobs, GString *log_output) {
    // 1. Transformación log/scale
    double **trans = transform_data_from_matrix(input, n_obs, n_var,
                                                opts.use_logs, opts.scale_factor);
    if (!trans) {
        if (log_output) g_string_append(log_output, "ERROR: Cannot apply log transform (non-positive values).\n");
        *out_nobs = 0;
        return NULL;
    }
    int nobs_trans = n_obs;

    // 2. Desestacionalización (si está activada)
    if (opts.deseasonalize && opts.deseasonalize_s >= 2) {
        if (log_output) {
            g_string_append_printf(log_output, "\nSeasonal adjustment (period s=%d, mode=%s):\n",
                                   opts.deseasonalize_s,
                                   opts.deseasonalize_auto ? "auto (p<0.05)" : "force all");
        }

        int s = opts.deseasonalize_s;
        int d = opts.diff_order;

        for (int j = 0; j < n_var; j++) {
            double *series = extract_column_from_matrix(trans, nobs_trans, j);
            int n_diff;
            double *diff_series = apply_differences_to_series(series, nobs_trans, d, &n_diff);

            if (!diff_series || n_diff < 2 * s) {
                if (log_output) g_string_append_printf(log_output, "  Series %d: insufficient data after differencing, skipping.\n", j+1);
                free(series);
                free(diff_series);
                continue;
            }

            // Realizar regresión armónica para test F
            int num_harm = s - 1;
            double *coeffs = malloc(num_harm * sizeof(double));
            double *std_err = malloc(num_harm * sizeof(double));
            gsl_matrix *hac_cov = NULL, *ols_cov = NULL;
            double intercept, intercept_se, f_stat, p_value, r_sq;

            int reg_ok = harmonic_regression_differenced_basis(diff_series, n_diff, d,
                                                               coeffs, std_err, &hac_cov, &ols_cov,
                                                               &intercept, &intercept_se,
                                                               &f_stat, &p_value, &r_sq,
                                                               s);
            // Guardar diff_series para posible uso en get_seasonal_dummies
            // (la regresión ya la consumió, pero necesitamos los datos para estimar dummies)
            // Podemos reusar diff_series, o mejor, volver a calcular si es necesario.
            // Para no complicar, liberamos diff_series y luego recalculamos si hace falta.
            free(diff_series);
            diff_series = NULL;

            int is_seasonal = 0;
            if (reg_ok) {
                double f_crit = f_distribution_critical_value(num_harm, n_diff - num_harm - 1, 0.05);
                is_seasonal = (f_stat > f_crit);
            }

            // Decidir si desestacionalizar esta serie
            int do_deseason = 0;
            if (opts.deseasonalize_auto) {
                do_deseason = is_seasonal;
            } else {
                do_deseason = 1;  // forzar
            }

            if (log_output) {
                g_string_append_printf(log_output, "  Series %d: F=%.3f, p=%.4f, R²=%.3f -> %s",
                                       j+1, f_stat, p_value, r_sq,
                                       is_seasonal ? "SEASONAL" : "NOT SEASONAL");
                if (do_deseason) {
                    g_string_append(log_output, " (adjusted)\n");
                } else {
                    g_string_append(log_output, " (kept as is)\n");
                }
            }

            // Aplicar ajuste si corresponde
            if (do_deseason) {
                // Recalcular diff_series para get_seasonal_dummies
                double *diff2 = apply_differences_to_series(series, nobs_trans, d, &n_diff);
                if (diff2) {
                    int success;
                    double *level_dummies = get_seasonal_dummies(diff2, n_diff, s, d, &success, NULL);
                    free(diff2);
                    if (success && level_dummies) {
                        int start_month = opts.start_sub;
                        for (int i = 0; i < nobs_trans; i++) {
                            int period = (i + start_month - 1) % s;
                            trans[i][j] -= level_dummies[period];
                        }
                        free(level_dummies);
                    } else {
                        if (log_output) g_string_append_printf(log_output, "    Warning: adjustment failed for series %d\n", j+1);
                    }
                }
            }

            free(series);
            free(coeffs);
            free(std_err);
            if (hac_cov) gsl_matrix_free(hac_cov);
            if (ols_cov) gsl_matrix_free(ols_cov);
        }
    }

    // 3. Aplicar diferenciación final
    double **final_data = apply_differences_to_matrix(trans, nobs_trans, n_var,
                                                      opts.diff_order, out_nobs);
    // Liberar matriz transformada
    for (int i = 0; i < nobs_trans; i++) free(trans[i]);
    free(trans);

    if (!final_data || *out_nobs < 1) {
        if (log_output) g_string_append(log_output, "ERROR: Not enough observations after final differencing.\n");
        return NULL;
    }

    return final_data;
}

/**
 * Preprocesa series solo en niveles (sin diferenciación final):
 * - Transformación log/scale
 * - Desestacionalización selectiva (si activada) con d=0
 *
 * @param input      Matriz original [n_obs][n_var]
 * @param n_obs      Núm. observaciones originales
 * @param n_var      Núm. variables
 * @param out_nobs   (out) Núm. observaciones (igual a n_obs si éxito)
 * @param log_output (out) GString para log (puede ser NULL)
 * @return Matriz procesada en niveles (debe liberarse con free_processed_matrix)
 */
static double** preprocess_series_levels(double **input, int n_obs, int n_var,
                                         int *out_nobs, GString *log_output) {
    // 1. Transformación log/scale
    double **trans = transform_data_from_matrix(input, n_obs, n_var,
                                                opts.use_logs, opts.scale_factor);
    if (!trans) {
        if (log_output) g_string_append(log_output, "ERROR: Cannot apply log transform (non-positive values).\n");
        *out_nobs = 0;
        return NULL;
    }
    int nobs_trans = n_obs;

    // 2. Desestacionalización (si está activada)
    //    La especificación es EN NIVELES, pero la estimación de los armónicos
    //    se hace con d=1 (serie diferenciada, estacionaria): así la tendencia
    //    no contamina el test/estimación. Mediante la matriz de transformación
    //    A0 (transform_harmonics_to_dummies_general) se recuperan las dummies
    //    estacionales DE NIVEL, que se restan a la serie en niveles.
    if (opts.deseasonalize && opts.deseasonalize_s >= 2) {
        if (log_output) {
            g_string_append_printf(log_output, "\nSeasonal adjustment (levels, s=%d, mode=%s):\n",
                                   opts.deseasonalize_s,
                                   opts.deseasonalize_auto ? "auto (p<0.05)" : "force all");
        }

        int s = opts.deseasonalize_s;
        int d = 1;  // estimar sobre datos diferenciados (estacionarios); la salida sigue en niveles

        for (int j = 0; j < n_var; j++) {
            double *series = extract_column_from_matrix(trans, nobs_trans, j);
            int n_diff;
            double *diff_series = apply_differences_to_series(series, nobs_trans, d, &n_diff);

            if (!diff_series || n_diff < 2 * s) {
                if (log_output) g_string_append_printf(log_output, "  Series %d: insufficient data, skipping.\n", j+1);
                free(series);
                free(diff_series);
                continue;
            }

            int num_harm = s - 1;
            double *coeffs = malloc(num_harm * sizeof(double));
            double *std_err = malloc(num_harm * sizeof(double));
            gsl_matrix *hac_cov = NULL, *ols_cov = NULL;
            double intercept, intercept_se, f_stat, p_value, r_sq;

            int reg_ok = harmonic_regression_differenced_basis(diff_series, n_diff, d,
                                                               coeffs, std_err, &hac_cov, &ols_cov,
                                                               &intercept, &intercept_se,
                                                               &f_stat, &p_value, &r_sq,
                                                               s);
            free(diff_series);

            int is_seasonal = 0;
            if (reg_ok) {
                double f_crit = f_distribution_critical_value(num_harm, n_diff - num_harm - 1, 0.05);
                is_seasonal = (f_stat > f_crit);
            }

            int do_deseason = opts.deseasonalize_auto ? is_seasonal : 1;

            if (log_output) {
                g_string_append_printf(log_output, "  Series %d: F=%.3f, p=%.4f -> %s",
                                       j+1, f_stat, p_value,
                                       is_seasonal ? "SEASONAL" : "NOT SEASONAL");
                if (do_deseason) {
                    g_string_append(log_output, " (adjusted)\n");
                } else {
                    g_string_append(log_output, " (kept as is)\n");
                }
            }

            if (do_deseason) {
                double *diff2 = apply_differences_to_series(series, nobs_trans, d, &n_diff);
                if (diff2) {
                    int success;
                    double *level_dummies = get_seasonal_dummies(diff2, n_diff, s, d, &success, NULL);
                    free(diff2);
                    if (success && level_dummies) {
                        int start_month = opts.start_sub;
                        for (int i = 0; i < nobs_trans; i++) {
                            int period = (i + start_month - 1) % s;
                            trans[i][j] -= level_dummies[period];
                        }
                        free(level_dummies);
                    } else {
                        if (log_output) g_string_append_printf(log_output, "    Warning: adjustment failed for series %d\n", j+1);
                    }
                }
            }

            free(series);
            free(coeffs);
            free(std_err);
            if (hac_cov) gsl_matrix_free(hac_cov);
            if (ols_cov) gsl_matrix_free(ols_cov);
        }
    }

    *out_nobs = nobs_trans;
    return trans;
}


/* ---------- generate_inp_file con depuración ---------- */


static gboolean generate_inp_file(const char *inp_path) {
    GString *log = g_string_new("");

    /* Option A: the GUI only applies its harmonic seasonal adjustment and
       returns the series in (raw) LEVELS; the engine owns the Box-Cox and the
       regular/seasonal differencing (written in the .inp header) so it can
       invert the forecasts back to levels.  We therefore deseasonalize on the
       raw scale (log delegated to the engine via lambda) and neutralise the
       scale factor for the .inp flow. */
    int saved_logs = opts.use_logs;
    double saved_scale = opts.scale_factor;
    opts.use_logs = 0;
    opts.scale_factor = 1.0;

    int nobs_levels;
    double **levels = preprocess_series_levels(data.data, data.n_obs, data.n_var,
                                               &nobs_levels, log);

    opts.use_logs = saved_logs;
    opts.scale_factor = saved_scale;

    // Mostrar el log en el text_view
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_insert(buffer, &end, log->str, -1);
    g_string_free(log, TRUE);

    if (!levels || nobs_levels < 1) {
        gtk_label_set_text(GTK_LABEL(status_label), "Error during preprocessing.");
        return FALSE;
    }

    /* Transformation spec delegated to the engine. */
    double lambda = saved_logs ? 0.0 : 1.0;   /* 0 = log, 1 = none */
    int d = opts.diff_order;
    int D = opts.seasonal_diff;

    // Escribir archivo .inp (formato fue-style simplificado)
    FILE *f = fopen(inp_path, "w");
    if (!f) {
        free_processed_matrix(levels, nobs_levels);
        return FALSE;
    }

    fprintf(f, "* DRVARMA input generated by drvarma_gui\n");
    if (opts.deseasonalize)
        fprintf(f, "* series seasonally adjusted (harmonic), s=%d\n",
                opts.deseasonalize_s);
    fprintf(f, "**********************************************************\n");
    fprintf(f, "** Frequency (1=A, 4=Q, 12=M):\n %d\n", opts.freq);
    fprintf(f, "** Series, observations, start (subperiod year):\n %d %d %d %d\n",
            data.n_var, nobs_levels, opts.start_sub, opts.start_year);
    fprintf(f, "** Series names:\n");
    for (int j = 1; j <= data.n_var; j++) fprintf(f, " y%d", j);
    fprintf(f, "\n");
    fprintf(f, "** Box-Cox lambda, regular differences, annual differences:\n");
    fprintf(f, " %g %d %d\n", lambda, d, D);
    fprintf(f, "** Data:\n");
    for (int i = 0; i < nobs_levels; i++) {
        for (int j = 0; j < data.n_var; j++)
            fprintf(f, "%f ", levels[i][j]);
        fprintf(f, "\n");
    }
    fclose(f);

    free_processed_matrix(levels, nobs_levels);

    gtk_label_set_text(GTK_LABEL(status_label), "INP generated.");
    return TRUE;
}


/* ---------- Build command line and run drvarma ---------- */

#ifdef _WIN32
static gboolean run_drvarma_win32(const char *inp_base, const char *inp_dir) {
    // Buscar el ejecutable
    char *full_path = g_find_program_in_path(opts.drv_path);
    if (!full_path) {
        if (g_file_test(opts.drv_path, G_FILE_TEST_IS_EXECUTABLE)) {
            full_path = g_strdup(opts.drv_path);
        } else {
            gchar *msg = g_strdup_printf("Executable not found: %s", opts.drv_path);
            gtk_label_set_text(GTK_LABEL(status_label), msg);
            g_free(msg);
            return FALSE;
        }
    }

    // Convertir a rutas Windows
    gchar *win_path = g_strdup(full_path);
    for (gchar *p = win_path; *p; p++) if (*p == '/') *p = '\\';
    g_free(full_path);
    full_path = win_path;

    // Construir la línea de comandos
    GString *cmdline = g_string_new("\"");
    g_string_append(cmdline, full_path);
    g_string_append(cmdline, "\" ");
    g_string_append_printf(cmdline, "%s %d %d", inp_base, opts.p, opts.q);

    if (opts.include_mean) g_string_append(cmdline, " -mean");
    if (opts.diag_ar)      g_string_append(cmdline, " -diagar");
    if (opts.diag_ma)      g_string_append(cmdline, " -diagma");
    if (opts.diag_cov)     g_string_append(cmdline, " -diagcov");
    if (opts.twostep)      g_string_append(cmdline, " -twostep");

    g_string_append_printf(cmdline, " -m %d", opts.method);

    if (opts.use_exp_vol) {
        g_string_append_printf(cmdline, " -volexp %f %d", opts.exp_alpha, opts.exp_window);
    }
    if (opts.use_mov_vol) {
        g_string_append_printf(cmdline, " -volmov %d", opts.mov_window);
    }

    if (opts.do_forecast) {
        g_string_append_printf(cmdline, " -forecast %d", opts.forecast_horizon);
        g_string_append_printf(cmdline, " -seasonal %d", opts.forecast_seasonal);
    }

    g_print("DEBUG: Línea de comandos: %s\n", cmdline->str);
    g_print("DEBUG: Directorio de trabajo: %s\n", inp_dir);

    // Configurar estructuras para CreateProcess
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {0};

    // Convertir directorio de trabajo a UTF-16
    wchar_t *wdir = g_utf8_to_utf16(inp_dir, -1, NULL, NULL, NULL);
    wchar_t *wcmdline = g_utf8_to_utf16(cmdline->str, -1, NULL, NULL, NULL);

    BOOL success = CreateProcessW(
        NULL,               // No usar nombre de aplicación (usamos línea completa)
        wcmdline,           // Línea de comandos
        NULL, NULL,         // Atributos de seguridad
        FALSE,              // No heredar handles
        CREATE_NO_WINDOW,   // No mostrar ventana de consola (opcional)
        NULL,               // Entorno (heredar)
        wdir,               // Directorio de trabajo
        &si, &pi
    );

    DWORD last_error = GetLastError();

    g_free(wdir);
    g_free(wcmdline);
    g_string_free(cmdline, TRUE);
    g_free(full_path);

    if (!success) {
        gchar *msg = g_strdup_printf("CreateProcess failed (error %lu)", last_error);
        gtk_label_set_text(GTK_LABEL(status_label), msg);
        g_free(msg);
        return FALSE;
    }

    // Esperar a que termine
    WaitForSingleObject(pi.hProcess, INFINITE);

    // Obtener código de salida
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exit_code != 0) {
        gchar *msg = g_strdup_printf("drvarma.exe exited with code %lu", exit_code);
        gtk_label_set_text(GTK_LABEL(status_label), msg);
        g_free(msg);
        return FALSE;
    }

    return TRUE;
}
#endif

/* ---------- Build command line and run drvarma ---------- */
static void run_drvarma(const char *inp_base, const char *inp_dir) {
    // Buscar el ejecutable en el PATH o en el directorio actual
    char *full_path = g_find_program_in_path(opts.drv_path);
    if (!full_path) {
        // Si no está en el PATH, probar con la ruta tal cual
        if (g_file_test(opts.drv_path, G_FILE_TEST_IS_EXECUTABLE)) {
            full_path = g_strdup(opts.drv_path);
        } else {
            gchar *msg = g_strdup_printf("Executable not found: %s", opts.drv_path);
            gtk_label_set_text(GTK_LABEL(status_label), msg);
            g_free(msg);
            return;
        }
    }

    // En Windows, convertir barras normales a invertidas para evitar problemas
#ifdef _WIN32
    gchar *win_path = g_strdup(full_path);
    for (gchar *p = win_path; *p; p++) {
        if (*p == '/') *p = '\\';
    }
    g_free(full_path);
    full_path = win_path;
#endif

    GPtrArray *args = g_ptr_array_new();
    g_ptr_array_add(args, full_path);  // argv[0] (se liberará al final)
    g_ptr_array_add(args, g_strdup(inp_base));
    g_ptr_array_add(args, g_strdup_printf("%d", opts.p));
    g_ptr_array_add(args, g_strdup_printf("%d", opts.q));

    if (opts.include_mean)  g_ptr_array_add(args, g_strdup("-mean"));
    if (opts.diag_ar)       g_ptr_array_add(args, g_strdup("-diagar"));
    if (opts.diag_ma)       g_ptr_array_add(args, g_strdup("-diagma"));
    if (opts.diag_cov)      g_ptr_array_add(args, g_strdup("-diagcov"));
    if (opts.twostep)       g_ptr_array_add(args, g_strdup("-twostep"));

    g_ptr_array_add(args, g_strdup("-m"));
    g_ptr_array_add(args, g_strdup_printf("%d", opts.method));

    if (opts.use_exp_vol) {
        g_ptr_array_add(args, g_strdup("-volexp"));
        g_ptr_array_add(args, g_strdup_printf("%f", opts.exp_alpha));
        g_ptr_array_add(args, g_strdup_printf("%d", opts.exp_window));
    }

    if (opts.use_mov_vol) {
        g_ptr_array_add(args, g_strdup("-volmov"));
        g_ptr_array_add(args, g_strdup_printf("%d", opts.mov_window));
    }

    if (opts.do_forecast) {
        g_ptr_array_add(args, g_strdup("-forecast"));
        g_ptr_array_add(args, g_strdup_printf("%d", opts.forecast_horizon));
        g_ptr_array_add(args, g_strdup("-seasonal"));
        g_ptr_array_add(args, g_strdup_printf("%d", opts.forecast_seasonal));
    }

    g_ptr_array_add(args, NULL);  // terminador

    gchar **argv = (gchar **)g_ptr_array_free(args, FALSE);

    // Mostrar comando para depuración
    g_print("DEBUG: Ejecutando: ");
    for (int i = 0; argv[i] != NULL; i++) g_print("%s ", argv[i]);
    g_print("\n");
    g_print("DEBUG: Directorio de trabajo: %s\n", inp_dir);

    GError *error = NULL;
    gboolean ok = g_spawn_sync(inp_dir, argv, NULL,
                               G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL,
                               NULL, NULL, NULL, NULL, NULL, &error);

    // Liberar argumentos (argv[0] se libera con full_path)
    for (int i = 1; argv[i] != NULL; i++) g_free(argv[i]);
    g_free(argv);  // libera el array (pero no los strings internos, ya liberados)
    // Nota: full_path se liberó implícitamente porque era argv[0] y ya se incluyó en args,
    // pero al hacer g_free(argv) no se libera automáticamente. Lo liberamos explícitamente:
    g_free(full_path);

    if (!ok) {
        gchar *msg = g_strdup_printf("Failed to run drvarma: %s", error->message);
            g_print("DEBUG: g_spawn_sync falló: %s\n", error->message);
    #ifdef _WIN32
    g_print("DEBUG: Windows error code: %lu\n", GetLastError());
    #endif
        gtk_label_set_text(GTK_LABEL(status_label), msg);
        g_free(msg);
        g_error_free(error);
    } else {
        gtk_label_set_text(GTK_LABEL(status_label), "DRVARMA finished successfully.");
    }
}

/* ---------- Añadir función para diferenciar una serie univariante  ---------- */

 /* ---------- Aplicar diferencias a una serie univariante (corregido) ---------- */
double* apply_differences_to_series(double *series, int n, int d, int *new_n) {
    if (d == 0) {
        *new_n = n;
        double *new_series = malloc(n * sizeof(double));
        memcpy(new_series, series, n * sizeof(double));
        return new_series;
    }
    if (d == 1) {
        int new_len = n - 1;
        if (new_len <= 0) return NULL;
        double *diff = malloc(new_len * sizeof(double));
        for (int i = 0; i < new_len; i++) {
            diff[i] = series[i+1] - series[i];
        }
        *new_n = new_len;
        return diff;
    }
    if (d == 2) {
        int new_len = n - 2;
        if (new_len <= 0) return NULL;
        double *diff = malloc(new_len * sizeof(double));
        for (int i = 0; i < new_len; i++) {
            diff[i] = series[i+2] - 2.0 * series[i+1] + series[i];
        }
        *new_n = new_len;
        return diff;
    }
    // d > 2 no soportado
    return NULL;
}

/* ---------- Sanitize a string to contain only valid UTF-8 ---------- */
static char *sanitize_to_utf8(const char *input) {
    GString *result = g_string_new(NULL);
    const char *p = input;
    while (*p) {
        if (g_utf8_validate(p, -1, NULL)) {
            gunichar c = g_utf8_get_char(p);
            g_string_append_unichar(result, c);
            p = g_utf8_next_char(p);
        } else {
            g_string_append_c(result, '?');
            p++;
        }
    }
    return g_string_free(result, FALSE);
}

/* ---------- Display a file in the text view ---------- */
static void display_file_in_view(const char *file_path) {
    gchar *content = NULL;
    gsize length = 0;
    if (!g_file_get_contents(file_path, &content, &length, NULL)) {
        gchar *msg = g_strdup_printf("Cannot read file: %s", file_path);
        gtk_label_set_text(GTK_LABEL(status_label), msg);
        g_free(msg);
        return;
    }

    char *safe_content = sanitize_to_utf8(content);
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    gtk_text_buffer_set_text(buffer, safe_content, -1);

    g_free(safe_content);
    g_free(content);
}

/* ---------- Helper: append formatted text to the text view ---------- */
static void append_to_view(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char *buf = g_strdup_vprintf(fmt, args);
    va_end(args);

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_insert(buffer, &end, buf, -1);
    g_free(buf);
}



/* ---------- Callback for Johansen test button ---------- */
/* ---------- Callback for Johansen test button (standalone module) ---------- */
static void on_johansen_test(GtkWidget *widget, gpointer user_data) {
    if (!opts.props_set) {
        gtk_label_set_text(GTK_LABEL(status_label), "Please set data properties first.");
        return;
    }
    if (data.n_obs == 0) {
        gtk_label_set_text(GTK_LABEL(status_label), "No data loaded.");
        return;
    }

    /* ---------- diálogo de parámetros ---------- */
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Johansen Cointegration Test",
                                                    GTK_WINDOW(main_window),
                                                    GTK_DIALOG_MODAL,
                                                    "_OK", GTK_RESPONSE_OK,
                                                    "_Cancel", GTK_RESPONSE_CANCEL,
                                                    NULL);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 10);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_container_add(GTK_CONTAINER(content), grid);

    int row = 0;

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Lag order (p):"), 0, row, 1, 1);
    GtkWidget *lag_spin = gtk_spin_button_new_with_range(1, 10, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(lag_spin), 1);
    gtk_grid_attach(GTK_GRID(grid), lag_spin, 1, row, 1, 1);
    row++;

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Deterministic terms:"), 0, row, 1, 1);
    GtkWidget *det_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(det_combo), NULL, "None");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(det_combo), NULL, "Constant (restricted)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(det_combo), NULL, "Constant + Trend (restricted)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(det_combo), NULL, "Constant + Step dummy (restricted)");
    gtk_combo_box_set_active(GTK_COMBO_BOX(det_combo), 1);  /* default: constant */
    gtk_grid_attach(GTK_GRID(grid), det_combo, 1, row, 1, 1);
    row++;

    gtk_widget_show_all(content);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
        int p = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(lag_spin));
        int det_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(det_combo));
        JohansenDetType det;
        switch (det_idx) {
            case 0: det = JOH_DET_NONE; break;
            case 1: det = JOH_DET_CONST; break;
            case 2: det = JOH_DET_CONST_TREND; break;
            case 3: det = JOH_DET_CONST_DUMMY; break;
            default: det = JOH_DET_CONST;
        }

        /* preprocess data using global opts (logs, scale, deseasonalize) */
        int nobs_pp;
        GString *preplog = g_string_new("--- Preprocessing report ---\n");
        double **ppdata = preprocess_series_levels(data.data, data.n_obs, data.n_var,
                                                    &nobs_pp, preplog);
        g_string_append(preplog, "----------------------------\n\n");
        if (!ppdata || nobs_pp < p + 2) {
            gtk_label_set_text(GTK_LABEL(status_label),
                "Preprocessing failed or insufficient observations.");
            if (ppdata) free_processed_matrix(ppdata, nobs_pp);
            g_string_free(preplog, TRUE);
            gtk_widget_destroy(dialog);
            return;
        }

        /* execute Johansen test — use_logs=0 because preprocessing already applied */
        JohansenResult *jres = johansen_test_run((const double * const *)ppdata,
                                                 nobs_pp, data.n_var, p,
                                                 det, 0);
        free_processed_matrix(ppdata, nobs_pp);

        if (jres) {
            GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
            gtk_text_buffer_set_text(buffer, preplog->str, -1);
            GtkTextIter end;
            gtk_text_buffer_get_end_iter(buffer, &end);
            gtk_text_buffer_insert(buffer, &end, jres->conclusion, -1);
            gtk_label_set_text(GTK_LABEL(status_label), "Johansen test completed.");
            johansen_result_free(jres);
        } else {
            gtk_label_set_text(GTK_LABEL(status_label), "Johansen test failed (memory error).");
        }
        g_string_free(preplog, TRUE);
    }

    gtk_widget_destroy(dialog);
}

/* ---------- Callback for Estimate VECM button ---------- */
static void on_vecm_estimate(GtkWidget *widget, gpointer user_data) {
    if (!opts.props_set) {
        gtk_label_set_text(GTK_LABEL(status_label), "Please set data properties first.");
        return;
    }
    if (data.n_obs == 0) {
        gtk_label_set_text(GTK_LABEL(status_label), "No data loaded.");
        return;
    }

    GtkWidget *dialog = gtk_dialog_new_with_buttons("Estimate VECM",
                                                    GTK_WINDOW(main_window),
                                                    GTK_DIALOG_MODAL,
                                                    "_OK", GTK_RESPONSE_OK,
                                                    "_Cancel", GTK_RESPONSE_CANCEL,
                                                    NULL);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 10);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_container_add(GTK_CONTAINER(content), grid);
    int row = 0;

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Lag order (p):"), 0, row, 1, 1);
    GtkWidget *lag_spin = gtk_spin_button_new_with_range(1, 10, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(lag_spin), 2);
    gtk_grid_attach(GTK_GRID(grid), lag_spin, 1, row, 1, 1);
    row++;

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Cointegration rank (r):"), 0, row, 1, 1);
    GtkWidget *rank_spin = gtk_spin_button_new_with_range(1, data.n_var - 1, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(rank_spin), 1);
    gtk_grid_attach(GTK_GRID(grid), rank_spin, 1, row, 1, 1);
    row++;

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Det. case:"), 0, row, 1, 1);
    GtkWidget *det_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(det_combo), NULL, "None");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(det_combo), NULL, "Constant (unrestricted)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(det_combo), NULL, "Constant + Trend");
    gtk_combo_box_set_active(GTK_COMBO_BOX(det_combo), 1);
    gtk_grid_attach(GTK_GRID(grid), det_combo, 1, row, 1, 1);
    row++;

    gtk_widget_show_all(content);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
        int p = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(lag_spin));
        int r = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(rank_spin));
        int det_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(det_combo));
        VecmDetType vdet;
        switch (det_idx) {
            case 0: vdet = VECM_DET_NONE; break;
            case 1: vdet = VECM_DET_CONST; break;
            case 2: vdet = VECM_DET_CONST_TREND; break;
            default: vdet = VECM_DET_CONST;
        }

        /* preprocess data using global opts (logs, scale, deseasonalize) */
        int nobs_pp;
        GString *preplog = g_string_new("--- Preprocessing report ---\n");
        double **ppdata = preprocess_series_levels(data.data, data.n_obs, data.n_var,
                                                    &nobs_pp, preplog);
        g_string_append(preplog, "----------------------------\n\n");
        if (!ppdata || nobs_pp < p + 2) {
            gtk_label_set_text(GTK_LABEL(status_label),
                "Preprocessing failed or insufficient observations.");
            if (ppdata) free_processed_matrix(ppdata, nobs_pp);
            g_string_free(preplog, TRUE);
            gtk_widget_destroy(dialog);
            return;
        }

        VecmResult *vres = vecm_estimate((const double * const *)ppdata,
                                         nobs_pp, data.n_var, p, r,
                                         vdet, 0, NULL);
        free_processed_matrix(ppdata, nobs_pp);
        if (vres) {
            GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
            gtk_text_buffer_set_text(buffer, preplog->str, -1);
            GtkTextIter end;
            gtk_text_buffer_get_end_iter(buffer, &end);
            gtk_text_buffer_insert(buffer, &end, vres->report, -1);
            gtk_label_set_text(GTK_LABEL(status_label), "VECM estimation completed.");
            vecm_result_free(vres);
        } else {
            gtk_label_set_text(GTK_LABEL(status_label), "VECM estimation failed (memory error).");
        }
        g_string_free(preplog, TRUE);
    }

    gtk_widget_destroy(dialog);
}


/* ---------- VAR order selection (unchanged) ---------- */
static void on_select_var_order(GtkWidget *widget, gpointer user_data) {
    if (!opts.props_set) {
        gtk_label_set_text(GTK_LABEL(status_label), "Please set data properties first.");
        return;
    }
    if (data.n_obs == 0) {
        gtk_label_set_text(GTK_LABEL(status_label), "No data loaded.");
        return;
    }

    GtkWidget *dialog = gtk_dialog_new_with_buttons("Select VAR Order",
                                                    GTK_WINDOW(main_window),
                                                    GTK_DIALOG_MODAL,
                                                    "_OK", GTK_RESPONSE_OK,
                                                    "_Cancel", GTK_RESPONSE_CANCEL,
                                                    NULL);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 10);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_container_add(GTK_CONTAINER(content), grid);

    int row = 0;
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Maximum lag order:"), 0, row, 1, 1);
    GtkWidget *maxlag_spin = gtk_spin_button_new_with_range(1, 20, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(maxlag_spin), 8);
    gtk_grid_attach(GTK_GRID(grid), maxlag_spin, 1, row, 1, 1);
    row++;

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Use differenced data:"), 0, row, 1, 1);
    GtkWidget *diff_check = gtk_check_button_new();
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(diff_check), TRUE);
    gtk_grid_attach(GTK_GRID(grid), diff_check, 1, row, 1, 1);
    row++;

    gtk_widget_show_all(content);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
        int maxlag = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(maxlag_spin));
        int use_diff = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(diff_check));

        gtk_widget_destroy(dialog);

        int nobs_eff;
        double **usedata = preprocess_series(data.data, data.n_obs, data.n_var,
                                         &nobs_eff, NULL);  // sin log detallado

    if (!usedata || nobs_eff < maxlag + 2) {
        if (usedata) free_processed_matrix(usedata, nobs_eff);
        gtk_label_set_text(GTK_LABEL(status_label), "Not enough observations after preprocessing.");
        gtk_widget_destroy(dialog);
        return;
        }


        int n = data.n_var;
        double *aic = g_new(double, maxlag+1);
        double *bic = g_new(double, maxlag+1);
        double *hq  = g_new(double, maxlag+1);

        for (int p = 0; p <= maxlag; p++) {
            if (p == 0) {
                int nobs = nobs_eff;
                double *mean = g_new(double, n);
                for (int j = 0; j < n; j++) {
                    mean[j] = 0.0;
                    for (int t = 0; t < nobs; t++) mean[j] += usedata[t][j];
                    mean[j] /= nobs;
                }
                double ssr_sum = 0.0;
                for (int j = 0; j < n; j++) {
                    double ssr = 0.0;
                    for (int t = 0; t < nobs; t++) ssr += pow(usedata[t][j] - mean[j], 2);
                    ssr_sum += log(ssr / nobs);
                }
                double logdet = ssr_sum;
                int k = n;
                aic[p] = logdet + 2.0 * k / nobs;
                bic[p] = logdet + log(nobs) * k / nobs;
                hq[p]  = logdet + 2.0 * log(log(nobs)) * k / nobs;
                g_free(mean);
            } else {
                int nobs = nobs_eff - p;
                if (nobs < p + 2) {
                    aic[p] = 1e10; bic[p] = 1e10; hq[p] = 1e10;
                    continue;
                }
                int nreg = n * p + 1;
                gsl_matrix *Y = gsl_matrix_alloc(n, nobs);
                gsl_matrix *X = gsl_matrix_alloc(nreg, nobs);

                for (int t = p; t < nobs_eff; t++) {
                    int col = t - p;
                    for (int j = 0; j < n; j++) {
                        gsl_matrix_set(Y, j, col, usedata[t][j]);
                    }
                    int row = 0;
                    for (int lag = 1; lag <= p; lag++) {
                        for (int j = 0; j < n; j++) {
                            gsl_matrix_set(X, row++, col, usedata[t - lag][j]);
                        }
                    }
                    gsl_matrix_set(X, row, col, 1.0);
                }

                gsl_matrix *XX = gsl_matrix_alloc(nreg, nreg);
                gsl_matrix *XY = gsl_matrix_alloc(nreg, n);
                gsl_blas_dgemm(CblasNoTrans, CblasTrans, 1.0, X, X, 0.0, XX);
                gsl_blas_dgemm(CblasNoTrans, CblasTrans, 1.0, X, Y, 0.0, XY);

                gsl_matrix *XX_copy = gsl_matrix_alloc(nreg, nreg);
                gsl_matrix_memcpy(XX_copy, XX);
                gsl_permutation *perm = gsl_permutation_alloc(nreg);
                int signum;
                gsl_linalg_LU_decomp(XX_copy, perm, &signum);

                gsl_matrix *beta = gsl_matrix_alloc(nreg, n);
                for (int eq = 0; eq < n; eq++) {
                    gsl_vector_view xy_col = gsl_matrix_column(XY, eq);
                    gsl_vector_view beta_col = gsl_matrix_column(beta, eq);
                    gsl_linalg_LU_solve(XX_copy, perm, &xy_col.vector, &beta_col.vector);
                }

                gsl_matrix *resid = gsl_matrix_alloc(n, nobs);
                gsl_matrix_memcpy(resid, Y);
                gsl_matrix *pred = gsl_matrix_alloc(n, nobs);
                gsl_blas_dgemm(CblasTrans, CblasNoTrans, 1.0, beta, X, 0.0, pred);
                gsl_matrix_sub(resid, pred);

                gsl_matrix *cov = gsl_matrix_alloc(n, n);
                gsl_blas_dgemm(CblasNoTrans, CblasTrans, 1.0/nobs, resid, resid, 0.0, cov);

                gsl_matrix *cov_copy = gsl_matrix_alloc(n, n);
                gsl_matrix_memcpy(cov_copy, cov);
                gsl_permutation *perm2 = gsl_permutation_alloc(n);
                gsl_linalg_LU_decomp(cov_copy, perm2, &signum);
                double logdet = gsl_linalg_LU_lndet(cov_copy);

                int k = nreg * n;
                aic[p] = logdet + 2.0 * k / nobs;
                bic[p] = logdet + log(nobs) * k / nobs;
                hq[p]  = logdet + 2.0 * log(log(nobs)) * k / nobs;

                gsl_matrix_free(Y); gsl_matrix_free(X); gsl_matrix_free(XX);
                gsl_matrix_free(XY); gsl_matrix_free(XX_copy); gsl_matrix_free(beta);
                gsl_matrix_free(resid); gsl_matrix_free(pred); gsl_matrix_free(cov);
                gsl_matrix_free(cov_copy);
                gsl_permutation_free(perm); gsl_permutation_free(perm2);
            }
        }

        int best_aic = 0, best_bic = 0, best_hq = 0;
        for (int p = 1; p <= maxlag; p++) {
            if (aic[p] < aic[best_aic]) best_aic = p;
            if (bic[p] < bic[best_bic]) best_bic = p;
            if (hq[p] < hq[best_hq]) best_hq = p;
        }

        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
        gtk_text_buffer_set_text(buffer, "", -1);
        append_to_view("VAR Order Selection\n");
        append_to_view("===================\n");
        append_to_view("Data: %s\n", use_diff ? "differenced" : "raw levels");
        append_to_view("Maximum lag considered: %d\n", maxlag);
        append_to_view("\n");
        append_to_view("p\tAIC\t\tBIC\t\tHQ\n");
        for (int p = 0; p <= maxlag; p++) {
            if (aic[p] < 1e9) {
                append_to_view("%d\t%.4f\t%.4f\t%.4f\n", p, aic[p], bic[p], hq[p]);
            } else {
                append_to_view("%d\t(insufficient data)\n", p);
            }
        }
        append_to_view("\nOptimal order (AIC): %d\n", best_aic);
        append_to_view("Optimal order (BIC): %d\n", best_bic);
        append_to_view("Optimal order (HQ): %d\n", best_hq);

        g_free(aic); g_free(bic); g_free(hq);
        for (int i = 0; i < nobs_eff; i++) g_free(usedata[i]);
        g_free(usedata);

        gtk_label_set_text(GTK_LABEL(status_label), "VAR order selection completed.");
    } else {
        gtk_widget_destroy(dialog);
    }
}

/* ---------- Data properties dialog (unchanged) ---------- */
typedef struct {
    GtkWidget *freq_combo;
    GtkWidget *start_year_spin;
    GtkWidget *start_sub_spin;
    GtkWidget *sub_label;
    GtkWidget *end_label;
} PropWidgets;

static void on_freq_changed(GtkComboBox *combo, gpointer user_data) {
    PropWidgets *w = (PropWidgets *)user_data;
    int freq = 1;
    const char *active_text = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo));
    if (active_text) {
        if (strstr(active_text, "Quarterly")) freq = 4;
        else if (strstr(active_text, "Monthly")) freq = 12;
        g_free((gchar*)active_text);
    }

    gtk_spin_button_set_range(GTK_SPIN_BUTTON(w->start_sub_spin), 1, freq);

    if (freq == 1) {
        gtk_widget_hide(w->sub_label);
        gtk_widget_hide(w->start_sub_spin);
    } else {
        gtk_widget_show(w->sub_label);
        gtk_widget_show(w->start_sub_spin);
    }

    int start_year = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w->start_year_spin));
    int start_sub = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w->start_sub_spin));
    int nobs = data.n_obs;
    int end_year, end_sub;

    if (freq == 1) {
        end_year = start_year + nobs - 1;
        end_sub = 1;
    } else {
        int total_periods = (nobs - 1) + start_sub;
        end_year = start_year + (total_periods - 1) / freq;
        end_sub = ((total_periods - 1) % freq) + 1;
    }

    char buf[128];
    if (freq == 1)
        snprintf(buf, sizeof(buf), "End date: %d", end_year);
    else if (freq == 4)
        snprintf(buf, sizeof(buf), "End date: Q%d %d", end_sub, end_year);
    else
        snprintf(buf, sizeof(buf), "End date: %d/%d", end_sub, end_year);

    gtk_label_set_text(GTK_LABEL(w->end_label), buf);
}

static void on_start_year_changed(GtkSpinButton *spin, gpointer user_data) {
    PropWidgets *w = (PropWidgets *)user_data;
    on_freq_changed(GTK_COMBO_BOX(w->freq_combo), w);
}

static void on_start_sub_changed(GtkSpinButton *spin, gpointer user_data) {
    PropWidgets *w = (PropWidgets *)user_data;
    on_freq_changed(GTK_COMBO_BOX(w->freq_combo), w);
}

static void show_data_properties_dialog(GtkWidget *parent) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Data Properties",
                                                    GTK_WINDOW(parent),
                                                    GTK_DIALOG_MODAL,
                                                    "_OK", GTK_RESPONSE_OK,
                                                    "_Cancel", GTK_RESPONSE_CANCEL,
                                                    NULL);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 10);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_container_add(GTK_CONTAINER(content), grid);

    int row = 0;

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Frequency:"), 0, row, 1, 1);
    GtkWidget *freq_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(freq_combo), NULL, "Annual (1)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(freq_combo), NULL, "Quarterly (4)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(freq_combo), NULL, "Monthly (12)");
    gtk_combo_box_set_active(GTK_COMBO_BOX(freq_combo), 2);
    gtk_grid_attach(GTK_GRID(grid), freq_combo, 1, row, 1, 1);
    row++;

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Start year:"), 0, row, 1, 1);
    GtkWidget *start_year_spin = gtk_spin_button_new_with_range(1000, 3000, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(start_year_spin), 2000);
    gtk_grid_attach(GTK_GRID(grid), start_year_spin, 1, row, 1, 1);
    row++;

    GtkWidget *sub_label = gtk_label_new("Start month/quarter:");
    gtk_grid_attach(GTK_GRID(grid), sub_label, 0, row, 1, 1);
    GtkWidget *sub_spin = gtk_spin_button_new_with_range(1, 12, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(sub_spin), 1);
    gtk_grid_attach(GTK_GRID(grid), sub_spin, 1, row, 1, 1);
    row++;

    GtkWidget *end_label = gtk_label_new("End date: (not computed)");
    gtk_grid_attach(GTK_GRID(grid), end_label, 0, row, 2, 1);
    row++;

    PropWidgets *widgets = g_new(PropWidgets, 1);
    widgets->freq_combo = freq_combo;
    widgets->start_year_spin = start_year_spin;
    widgets->start_sub_spin = sub_spin;
    widgets->sub_label = sub_label;
    widgets->end_label = end_label;

    g_signal_connect(freq_combo, "changed", G_CALLBACK(on_freq_changed), widgets);
    g_signal_connect(start_year_spin, "value-changed", G_CALLBACK(on_start_year_changed), widgets);
    g_signal_connect(sub_spin, "value-changed", G_CALLBACK(on_start_sub_changed), widgets);

    on_freq_changed(GTK_COMBO_BOX(freq_combo), widgets);

    gtk_widget_show_all(content);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
        int freq = 1;
        const char *active_text = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(freq_combo));
        if (active_text) {
            if (strstr(active_text, "Quarterly")) freq = 4;
            else if (strstr(active_text, "Monthly")) freq = 12;
            g_free((gchar*)active_text);
        }
        opts.freq = freq;
        update_freq_info_label();
        opts.start_year = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(start_year_spin));
        opts.start_sub = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(sub_spin));
        opts.props_set = 1;
        g_print("DEBUG: Properties set: freq=%d, start_year=%d, start_sub=%d\n",
                opts.freq, opts.start_year, opts.start_sub);


  //      gtk_widget_set_sensitive(btn_run, TRUE);
        
            if (btn_run) {
        gtk_widget_set_sensitive(btn_run, TRUE);
    }
        
        gtk_label_set_text(GTK_LABEL(status_label), "Data properties set. Ready.");
    }

    g_free(widgets);
    gtk_widget_destroy(dialog);
}

/* ---------- Callback to set drvarma executable path ---------- */
static void on_set_drv_path(GtkWidget *widget, gpointer user_data) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Select drvarma executable",
                                                    GTK_WINDOW(main_window),
                                                    GTK_FILE_CHOOSER_ACTION_OPEN,
                                                    "_Cancel", GTK_RESPONSE_CANCEL,
                                                    "_Open", GTK_RESPONSE_ACCEPT,
                                                    NULL);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        g_strlcpy(opts.drv_path, filename, sizeof(opts.drv_path));
        gtk_entry_set_text(GTK_ENTRY(drv_path_entry), filename);
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

/* ---------- Callbacks for main window ---------- */
static void on_load_series(GtkWidget *widget, gpointer user_data) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Open data file",
                                                    GTK_WINDOW(main_window),
                                                    GTK_FILE_CHOOSER_ACTION_OPEN,
                                                    "_Cancel", GTK_RESPONSE_CANCEL,
                                                    "_Open", GTK_RESPONSE_ACCEPT,
                                                    NULL);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (load_data_file(filename)) {
            gchar *msg = g_strdup_printf("Loaded %d series, %d observations", data.n_var, data.n_obs);
            gtk_label_set_text(GTK_LABEL(status_label), msg);
            g_free(msg);
            opts.props_set = 0;
            gtk_widget_set_sensitive(btn_run, FALSE);
     //       gtk_widget_set_sensitive(btn_run_forecast, TRUE);
                        if (btn_run_forecast) {
                gtk_widget_set_sensitive(btn_run_forecast, FALSE);
            }
            show_data_properties_dialog(main_window);
        } else {
            gtk_label_set_text(GTK_LABEL(status_label), "Error reading file. Need at least 2 rows, all numeric.");
        }
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

static void on_run(GtkWidget *widget, gpointer user_data) {
    if (!opts.props_set) {
        gtk_label_set_text(GTK_LABEL(status_label), "Please set data properties first.");
        return;
    }
    if (data.n_obs == 0) {
        gtk_label_set_text(GTK_LABEL(status_label), "No data loaded.");
        return;
    }
    if (!data.base_name) {
        gtk_label_set_text(GTK_LABEL(status_label), "Internal error: base_name is NULL");
        return;
    }

    char *dir = g_path_get_dirname(data.input_filename);
    char *inp_filename = g_strconcat(data.base_name, ".inp", NULL);
    char *inp_path = g_build_filename(dir, inp_filename, NULL);
    g_free(inp_filename);
    g_print("DEBUG: Generating .inp at %s\n", inp_path);

    if (!generate_inp_file(inp_path)) {
        g_free(dir); g_free(inp_path);
        return;
    }

    gchar *msg = g_strdup_printf("Created %s", inp_path);
    gtk_label_set_text(GTK_LABEL(status_label), msg);
    g_free(msg);

    #ifdef _WIN32
    if (!run_drvarma_win32(data.base_name, dir)) {
        // El error ya se mostró en la función
        g_free(dir);
        g_free(inp_path);
        return;
    }
    #else
        run_drvarma(data.base_name, dir);
    #endif

    g_free(dir);
    g_free(inp_path);
}

static void on_view_output(GtkWidget *widget, gpointer user_data) {
    if (!data.base_name || !data.input_filename) {
        gtk_label_set_text(GTK_LABEL(status_label), "No output file available.");
        return;
    }

    char *dir = g_path_get_dirname(data.input_filename);
    char *out_filename = g_strconcat(data.base_name, ".out", NULL);
    char *out_path = g_build_filename(dir, out_filename, NULL);
    g_free(out_filename);
    g_free(dir);

    g_print("DEBUG: Viewing output %s\n", out_path);
    display_file_in_view(out_path);
    g_free(out_path);
    gtk_label_set_text(GTK_LABEL(status_label), "Output displayed.");
}

static void on_view_inp(GtkWidget *widget, gpointer user_data) {
    if (!data.base_name || !data.input_filename) {
        gtk_label_set_text(GTK_LABEL(status_label), "No input file generated yet.");
        return;
    }

    char *dir = g_path_get_dirname(data.input_filename);
    char *inp_filename = g_strconcat(data.base_name, ".inp", NULL);
    char *inp_path = g_build_filename(dir, inp_filename, NULL);
    g_free(inp_filename);
    g_free(dir);

    g_print("DEBUG: Viewing .inp %s\n", inp_path);
    if (g_file_test(inp_path, G_FILE_TEST_EXISTS)) {
        display_file_in_view(inp_path);
        gtk_label_set_text(GTK_LABEL(status_label), "Input file displayed.");
    } else {
        gchar *msg = g_strdup_printf("File %s does not exist. Run DRVARMA first.", inp_path);
        gtk_label_set_text(GTK_LABEL(status_label), msg);
        g_free(msg);
    }
    g_free(inp_path);
}

static void on_quit(GtkWidget *widget, gpointer user_data) {
    gtk_main_quit();
}

/* ---------- Callbacks for model option widgets ---------- */
static void on_p_value_changed(GtkSpinButton *btn, gpointer user_data) {
    opts.p = gtk_spin_button_get_value_as_int(btn);
}
static void on_q_value_changed(GtkSpinButton *btn, gpointer user_data) {
    opts.q = gtk_spin_button_get_value_as_int(btn);
}
static void on_mean_toggled(GtkToggleButton *btn, gpointer user_data) {
    opts.include_mean = gtk_toggle_button_get_active(btn) ? 1 : 0;
}
static void on_diagar_toggled(GtkToggleButton *btn, gpointer user_data) {
    opts.diag_ar = gtk_toggle_button_get_active(btn) ? 1 : 0;
}
static void on_diagma_toggled(GtkToggleButton *btn, gpointer user_data) {
    opts.diag_ma = gtk_toggle_button_get_active(btn) ? 1 : 0;
}
static void on_diagcov_toggled(GtkToggleButton *btn, gpointer user_data) {
    opts.diag_cov = gtk_toggle_button_get_active(btn) ? 1 : 0;
}
static void on_method_exact_toggled(GtkToggleButton *btn, gpointer user_data) {
    if (gtk_toggle_button_get_active(btn)) opts.method = 1;
}
static void on_method_approx_toggled(GtkToggleButton *btn, gpointer user_data) {
    if (gtk_toggle_button_get_active(btn)) opts.method = 2;
}
static void on_twostep_toggled(GtkToggleButton *btn, gpointer user_data) {
    opts.twostep = gtk_toggle_button_get_active(btn) ? 1 : 0;
}
static void on_exp_vol_toggled(GtkToggleButton *btn, gpointer user_data) {
    opts.use_exp_vol = gtk_toggle_button_get_active(btn) ? 1 : 0;
}
static void on_exp_alpha_changed(GtkSpinButton *btn, gpointer user_data) {
    opts.exp_alpha = gtk_spin_button_get_value(btn);
}
static void on_exp_window_changed(GtkSpinButton *btn, gpointer user_data) {
    opts.exp_window = gtk_spin_button_get_value_as_int(btn);
}
static void on_mov_vol_toggled(GtkToggleButton *btn, gpointer user_data) {
    opts.use_mov_vol = gtk_toggle_button_get_active(btn) ? 1 : 0;
}
static void on_mov_window_changed(GtkSpinButton *btn, gpointer user_data) {
    opts.mov_window = gtk_spin_button_get_value_as_int(btn);
}
static void on_logs_toggled(GtkToggleButton *btn, gpointer user_data) {
    opts.use_logs = gtk_toggle_button_get_active(btn) ? 1 : 0;
}
static void on_scale_factor_changed(GtkSpinButton *btn, gpointer user_data) {
    opts.scale_factor = gtk_spin_button_get_value(btn);
}
static void on_diff_order_changed(GtkSpinButton *btn, gpointer user_data) {
    opts.diff_order = gtk_spin_button_get_value_as_int(btn);
}
static void on_seasonal_diff_changed(GtkSpinButton *btn, gpointer user_data) {
    opts.seasonal_diff = gtk_spin_button_get_value_as_int(btn);
}

static void on_deseason_toggled(GtkToggleButton *btn, gpointer user_data) {
    opts.deseasonalize = gtk_toggle_button_get_active(btn) ? 1 : 0;
}

static void on_deseason_s_changed(GtkSpinButton *btn, gpointer user_data) {
    opts.deseasonalize_s = gtk_spin_button_get_value_as_int(btn);
}

static void on_forecast_toggled(GtkToggleButton *btn, gpointer user_data) {
    opts.do_forecast = gtk_toggle_button_get_active(btn) ? 1 : 0;
}

static void on_forecast_horizon_changed(GtkSpinButton *btn, gpointer user_data) {
    opts.forecast_horizon = gtk_spin_button_get_value_as_int(btn);
}

static void on_forecast_seasonal_changed(GtkSpinButton *btn, gpointer user_data) {
    opts.forecast_seasonal = gtk_spin_button_get_value_as_int(btn);
}

static void on_view_forecast(GtkWidget *widget, gpointer user_data) {
    if (!data.base_name || !data.input_filename) {
        gtk_label_set_text(GTK_LABEL(status_label), "No forecast file available.");
        return;
    }
    char *dir = g_path_get_dirname(data.input_filename);
    char *fcast_filename = g_strconcat(data.base_name, ".forecast", NULL);
    char *fcast_path = g_build_filename(dir, fcast_filename, NULL);
    g_free(fcast_filename);
    g_free(dir);
    if (g_file_test(fcast_path, G_FILE_TEST_EXISTS)) {
        display_file_in_view(fcast_path);
        gtk_label_set_text(GTK_LABEL(status_label), "Forecast displayed.");
    } else {
        gchar *msg = g_strdup_printf("Forecast file %s not found.", fcast_path);
        gtk_label_set_text(GTK_LABEL(status_label), msg);
        g_free(msg);
    }
    g_free(fcast_path);
}

static void on_run_and_forecast(GtkWidget *widget, gpointer user_data) {
    on_run(widget, user_data);
    if (opts.do_forecast) {
        while (gtk_events_pending()) gtk_main_iteration();
        on_view_forecast(widget, user_data);
    }
}


static void on_test_seasonality(GtkWidget *widget, gpointer user_data) {
    if (!opts.props_set || data.n_obs == 0) {
        gtk_label_set_text(GTK_LABEL(status_label), "No data or properties not set.");
        return;
    }

    GString *log = g_string_new("=== SEASONALITY TEST RESULTS ===\n");
    g_string_append_printf(log, "Period s=%d, differencing d=%d, significance 0.05\n\n",
                           opts.deseasonalize_s, opts.diff_order);

    // Transformación log/scale (sin desestacionalizar ni diferenciar)
    double **trans = transform_data_from_matrix(data.data, data.n_obs, data.n_var,
                                                opts.use_logs, opts.scale_factor);
    if (!trans) {
        g_string_append(log, "ERROR: Cannot apply log transform.\n");
        goto show_log;
    }

    int s = opts.deseasonalize_s;
    int d = opts.diff_order;

    for (int j = 0; j < data.n_var; j++) {
        double *series = extract_column_from_matrix(trans, data.n_obs, j);
        int n_diff;
        double *diff_series = apply_differences_to_series(series, data.n_obs, d, &n_diff);
        free(series);

        if (!diff_series || n_diff < 2 * s) {
            g_string_append_printf(log, "Series %d: insufficient data after differencing.\n", j+1);
            free(diff_series);
            continue;
        }

        int num_harm = s - 1;
        double *coeffs = malloc(num_harm * sizeof(double));
        double *std_err = malloc(num_harm * sizeof(double));
        gsl_matrix *hac_cov = NULL, *ols_cov = NULL;
        double intercept, intercept_se, f_stat, p_value, r_sq;

        int reg_ok = harmonic_regression_differenced_basis(diff_series, n_diff, d,
                                                           coeffs, std_err, &hac_cov, &ols_cov,
                                                           &intercept, &intercept_se,
                                                           &f_stat, &p_value, &r_sq,
                                                           s);
        free(diff_series);

        if (reg_ok) {
            double f_crit = f_distribution_critical_value(num_harm, n_diff - num_harm - 1, 0.05);
            int is_seasonal = (f_stat > f_crit);
            g_string_append_printf(log, "Series %d: F=%.3f (crit=%.3f), p=%.4f, R²=%.3f -> %s\n",
                                   j+1, f_stat, f_crit, p_value, r_sq,
                                   is_seasonal ? "SEASONAL" : "NOT SEASONAL");
        } else {
            g_string_append_printf(log, "Series %d: regression failed.\n", j+1);
        }

        free(coeffs); free(std_err);
        if (hac_cov) gsl_matrix_free(hac_cov);
        if (ols_cov) gsl_matrix_free(ols_cov);
    }

    for (int i = 0; i < data.n_obs; i++) free(trans[i]);
    free(trans);

show_log:
    {
        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
        gtk_text_buffer_set_text(buffer, log->str, -1);
    }
    g_string_free(log, TRUE);
    gtk_label_set_text(GTK_LABEL(status_label), "Seasonality test completed.");
}

/* ---------- Build the main UI with GtkNotebook ---------- */
static void create_ui(GtkApplication *app, gpointer user_data) {
    main_window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(main_window), "DRVARMA - VARMA Estimation");
    gtk_window_set_default_size(GTK_WINDOW(main_window), 1200, 800);
    g_signal_connect(main_window, "destroy", G_CALLBACK(on_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
    gtk_container_add(GTK_CONTAINER(main_window), vbox);

    /* ----- Top bar: file loading and drvarma path ----- */
    GtkWidget *hbox_file = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox_file, FALSE, FALSE, 0);

    GtkWidget *btn_load = gtk_button_new_with_label("Load Data File");
    g_signal_connect(btn_load, "clicked", G_CALLBACK(on_load_series), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_file), btn_load, FALSE, FALSE, 0);

    status_label = gtk_label_new("No data loaded.");
    gtk_box_pack_start(GTK_BOX(hbox_file), status_label, TRUE, TRUE, 0);

    /* drvarma path */
    GtkWidget *hbox_path = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox_path, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_path), gtk_label_new("drvarma executable:"), FALSE, FALSE, 0);
    drv_path_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(drv_path_entry), opts.drv_path);
    gtk_box_pack_start(GTK_BOX(hbox_path), drv_path_entry, TRUE, TRUE, 0);
    GtkWidget *btn_browse = gtk_button_new_with_label("Browse...");
    g_signal_connect(btn_browse, "clicked", G_CALLBACK(on_set_drv_path), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_path), btn_browse, FALSE, FALSE, 0);

    /* ----- Notebook ----- */
    GtkWidget *notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(vbox), notebook, FALSE, FALSE, 0);

    /* Page 1: Model */
    GtkWidget *grid_model = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid_model), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid_model), 15);
    gtk_container_set_border_width(GTK_CONTAINER(grid_model), 10);

    int r = 0;
    gtk_grid_attach(GTK_GRID(grid_model), gtk_label_new("AR order (p):"), 0, r, 1, 1);
    GtkWidget *spin_p = gtk_spin_button_new_with_range(0, 10, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_p), opts.p);
    g_signal_connect(spin_p, "value-changed", G_CALLBACK(on_p_value_changed), NULL);
    gtk_grid_attach(GTK_GRID(grid_model), spin_p, 1, r, 1, 1);
    r++;

    gtk_grid_attach(GTK_GRID(grid_model), gtk_label_new("MA order (q):"), 0, r, 1, 1);
    GtkWidget *spin_q = gtk_spin_button_new_with_range(0, 10, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_q), opts.q);
    g_signal_connect(spin_q, "value-changed", G_CALLBACK(on_q_value_changed), NULL);
    gtk_grid_attach(GTK_GRID(grid_model), spin_q, 1, r, 1, 1);
    r++;

    GtkWidget *check_mean = gtk_check_button_new_with_label("Include mean (-mean)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_mean), opts.include_mean);
    g_signal_connect(check_mean, "toggled", G_CALLBACK(on_mean_toggled), NULL);
    gtk_grid_attach(GTK_GRID(grid_model), check_mean, 0, r, 2, 1);
    r++;

    GtkWidget *check_twostep = gtk_check_button_new_with_label("Two-step initialisation (-twostep)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_twostep), opts.twostep);
    g_signal_connect(check_twostep, "toggled", G_CALLBACK(on_twostep_toggled), NULL);
    gtk_grid_attach(GTK_GRID(grid_model), check_twostep, 0, r, 2, 1);
    r++;

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), grid_model, gtk_label_new("Model"));

    /* Page 2: Matrix Structure */
    GtkWidget *grid_matrix = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid_matrix), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid_matrix), 15);
    gtk_container_set_border_width(GTK_CONTAINER(grid_matrix), 10);

    r = 0;
    GtkWidget *check_diagar = gtk_check_button_new_with_label("Diagonal AR (-diagar)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_diagar), opts.diag_ar);
    g_signal_connect(check_diagar, "toggled", G_CALLBACK(on_diagar_toggled), NULL);
    gtk_grid_attach(GTK_GRID(grid_matrix), check_diagar, 0, r++, 2, 1);

    GtkWidget *check_diagma = gtk_check_button_new_with_label("Diagonal MA (-diagma)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_diagma), opts.diag_ma);
    g_signal_connect(check_diagma, "toggled", G_CALLBACK(on_diagma_toggled), NULL);
    gtk_grid_attach(GTK_GRID(grid_matrix), check_diagma, 0, r++, 2, 1);

    GtkWidget *check_diagcov = gtk_check_button_new_with_label("Diagonal covariance (-diagcov)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_diagcov), opts.diag_cov);
    g_signal_connect(check_diagcov, "toggled", G_CALLBACK(on_diagcov_toggled), NULL);
    gtk_grid_attach(GTK_GRID(grid_matrix), check_diagcov, 0, r++, 2, 1);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), grid_matrix, gtk_label_new("Matrix Structure"));

    /* Page 3: Estimation */
    GtkWidget *grid_est = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid_est), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid_est), 15);
    gtk_container_set_border_width(GTK_CONTAINER(grid_est), 10);

    r = 0;
    gtk_grid_attach(GTK_GRID(grid_est), gtk_label_new("Method:"), 0, r, 1, 1);
    GtkWidget *radio_exact = gtk_radio_button_new_with_label(NULL, "Exact (1)");
    g_signal_connect(radio_exact, "toggled", G_CALLBACK(on_method_exact_toggled), NULL);
    gtk_grid_attach(GTK_GRID(grid_est), radio_exact, 1, r, 1, 1);
    GtkWidget *radio_approx = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio_exact), "Approximate (2)");
    g_signal_connect(radio_approx, "toggled", G_CALLBACK(on_method_approx_toggled), NULL);
    gtk_grid_attach(GTK_GRID(grid_est), radio_approx, 2, r, 1, 1);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_exact), opts.method == 1);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_approx), opts.method == 2);
    r++;

    gtk_grid_attach(GTK_GRID(grid_est), gtk_label_new("(Executable path set above)"), 0, r++, 3, 1);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), grid_est, gtk_label_new("Estimation"));

    /* Page 4: Volatility */
    GtkWidget *grid_vol = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid_vol), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid_vol), 15);
    gtk_container_set_border_width(GTK_CONTAINER(grid_vol), 10);

    r = 0;
    GtkWidget *exp_check = gtk_check_button_new_with_label("Exponential volatility (-volexp)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(exp_check), opts.use_exp_vol);
    g_signal_connect(exp_check, "toggled", G_CALLBACK(on_exp_vol_toggled), NULL);
    gtk_grid_attach(GTK_GRID(grid_vol), exp_check, 0, r, 3, 1);
    r++;

    gtk_grid_attach(GTK_GRID(grid_vol), gtk_label_new("  Alpha:"), 0, r, 1, 1);
    GtkWidget *exp_alpha_spin = gtk_spin_button_new_with_range(0.001, 0.5, 0.01);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(exp_alpha_spin), opts.exp_alpha);
    g_signal_connect(exp_alpha_spin, "value-changed", G_CALLBACK(on_exp_alpha_changed), NULL);
    gtk_grid_attach(GTK_GRID(grid_vol), exp_alpha_spin, 1, r, 1, 1);
    r++;

    gtk_grid_attach(GTK_GRID(grid_vol), gtk_label_new("  Window:"), 0, r, 1, 1);
    GtkWidget *exp_win_spin = gtk_spin_button_new_with_range(1, 100, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(exp_win_spin), opts.exp_window);
    g_signal_connect(exp_win_spin, "value-changed", G_CALLBACK(on_exp_window_changed), NULL);
    gtk_grid_attach(GTK_GRID(grid_vol), exp_win_spin, 1, r, 1, 1);
    r++;

    GtkWidget *mov_check = gtk_check_button_new_with_label("Moving volatility (-volmov)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mov_check), opts.use_mov_vol);
    g_signal_connect(mov_check, "toggled", G_CALLBACK(on_mov_vol_toggled), NULL);
    gtk_grid_attach(GTK_GRID(grid_vol), mov_check, 0, r, 3, 1);
    r++;

    gtk_grid_attach(GTK_GRID(grid_vol), gtk_label_new("  Window:"), 0, r, 1, 1);
    GtkWidget *mov_win_spin = gtk_spin_button_new_with_range(1, 100, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(mov_win_spin), opts.mov_window);
    g_signal_connect(mov_win_spin, "value-changed", G_CALLBACK(on_mov_window_changed), NULL);
    gtk_grid_attach(GTK_GRID(grid_vol), mov_win_spin, 1, r, 1, 1);
    r++;

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), grid_vol, gtk_label_new("Volatility"));

    /* Page 5: Data Transformation */
    GtkWidget *grid_trans = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid_trans), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid_trans), 15);
    gtk_container_set_border_width(GTK_CONTAINER(grid_trans), 10);

    r = 0;
    GtkWidget *check_logs = gtk_check_button_new_with_label("Apply log transform");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_logs), opts.use_logs);
    g_signal_connect(check_logs, "toggled", G_CALLBACK(on_logs_toggled), NULL);
    gtk_grid_attach(GTK_GRID(grid_trans), check_logs, 0, r++, 2, 1);

    gtk_grid_attach(GTK_GRID(grid_trans), gtk_label_new("Scale factor:"), 0, r, 1, 1);
    GtkWidget *scale_spin = gtk_spin_button_new_with_range(0.001, 1e6, 1.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(scale_spin), opts.scale_factor);
    g_signal_connect(scale_spin, "value-changed", G_CALLBACK(on_scale_factor_changed), NULL);
    gtk_grid_attach(GTK_GRID(grid_trans), scale_spin, 1, r, 1, 1);
    r++;

    gtk_grid_attach(GTK_GRID(grid_trans), gtk_label_new("Regular differencing (d):"), 0, r, 1, 1);
    GtkWidget *diff_spin = gtk_spin_button_new_with_range(0, 2, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(diff_spin), opts.diff_order);
    g_signal_connect(diff_spin, "value-changed", G_CALLBACK(on_diff_order_changed), NULL);
    gtk_grid_attach(GTK_GRID(grid_trans), diff_spin, 1, r, 1, 1);
    r++;

    gtk_grid_attach(GTK_GRID(grid_trans),
                    gtk_label_new("Seasonal differencing (D, lag=freq):"), 0, r, 1, 1);
    GtkWidget *sdiff_spin = gtk_spin_button_new_with_range(0, 2, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(sdiff_spin), opts.seasonal_diff);
    g_signal_connect(sdiff_spin, "value-changed", G_CALLBACK(on_seasonal_diff_changed), NULL);
    gtk_grid_attach(GTK_GRID(grid_trans), sdiff_spin, 1, r, 1, 1);
    r++;

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), grid_trans, gtk_label_new("Data Transformation"));


    /* ----- Página 6: Seasonal Adjustment ----- */
    GtkWidget *grid_seasonal = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid_seasonal), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid_seasonal), 15);
    gtk_container_set_border_width(GTK_CONTAINER(grid_seasonal), 10);

    int r_seas = 0;

    // Información sobre la frecuencia de los datos (se actualiza después)
    char freq_info[128];
    snprintf(freq_info, sizeof(freq_info), "(Data frequency: %d period(s) per year)", opts.freq);
    label_freq_info_seasonal = gtk_label_new(freq_info);
    gtk_widget_set_halign(label_freq_info_seasonal, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid_seasonal), label_freq_info_seasonal, 0, r_seas++, 2, 1);

    // Checkbox para activar la desestacionalización
   // Checkbox principal
    GtkWidget *check_deseason = gtk_check_button_new_with_label("Apply seasonal adjustment");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_deseason), opts.deseasonalize);
    g_signal_connect(check_deseason, "toggled", G_CALLBACK(on_deseason_toggled), NULL);
    gtk_grid_attach(GTK_GRID(grid_seasonal), check_deseason, 0, r_seas++, 2, 1);

    // Radio buttons para modo
    GtkWidget *radio_force = gtk_radio_button_new_with_label(NULL, "Force all series");
    GtkWidget *radio_auto  = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio_force),
                                                                     "Only if significant (p < 0.05)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_auto), opts.deseasonalize_auto);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_force), !opts.deseasonalize_auto);

    g_signal_connect(radio_force, "toggled", G_CALLBACK(on_deseason_force_toggled), NULL);
    g_signal_connect(radio_auto,  "toggled", G_CALLBACK(on_deseason_auto_toggled), NULL);

    gtk_grid_attach(GTK_GRID(grid_seasonal), radio_force, 1, r_seas++, 1, 1);
    gtk_grid_attach(GTK_GRID(grid_seasonal), radio_auto,  1, r_seas++, 1, 1);

    // Hacer que los radios se habiliten/deshabiliten con el checkbox
    g_object_bind_property(check_deseason, "active", radio_force, "sensitive", G_BINDING_SYNC_CREATE);
    g_object_bind_property(check_deseason, "active", radio_auto,  "sensitive", G_BINDING_SYNC_CREATE);

    // Botón para test independiente
    GtkWidget *btn_test_seasonal = gtk_button_new_with_label("Test Seasonality");
    g_signal_connect(btn_test_seasonal, "clicked", G_CALLBACK(on_test_seasonality), NULL);
    gtk_grid_attach(GTK_GRID(grid_seasonal), btn_test_seasonal, 0, r_seas++, 2, 1);

    // Etiqueta y spin para el período estacional
    gtk_grid_attach(GTK_GRID(grid_seasonal), gtk_label_new("Seasonal period (s):"), 0, r_seas, 1, 1);
    GtkWidget *spin_deseason_s = gtk_spin_button_new_with_range(2, 12, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_deseason_s), opts.deseasonalize_s);
    g_signal_connect(spin_deseason_s, "value-changed", G_CALLBACK(on_deseason_s_changed), NULL);
    gtk_grid_attach(GTK_GRID(grid_seasonal), spin_deseason_s, 1, r_seas++, 1, 1);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), grid_seasonal, gtk_label_new("Seasonal Adjustment"));

        /* ----- Página 7: Forecast ----- */
    GtkWidget *grid_forecast = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid_forecast), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid_forecast), 15);
    gtk_container_set_border_width(GTK_CONTAINER(grid_forecast), 10);

    int r_fc = 0;
    GtkWidget *check_forecast = gtk_check_button_new_with_label("Enable forecast (-forecast)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_forecast), opts.do_forecast);
    g_signal_connect(check_forecast, "toggled", G_CALLBACK(on_forecast_toggled), NULL);
    gtk_grid_attach(GTK_GRID(grid_forecast), check_forecast, 0, r_fc++, 2, 1);

    gtk_grid_attach(GTK_GRID(grid_forecast), gtk_label_new("Horizon (L):"), 0, r_fc, 1, 1);
    GtkWidget *spin_horizon = gtk_spin_button_new_with_range(1, 100, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_horizon), opts.forecast_horizon);
    g_signal_connect(spin_horizon, "value-changed", G_CALLBACK(on_forecast_horizon_changed), NULL);
    gtk_grid_attach(GTK_GRID(grid_forecast), spin_horizon, 1, r_fc++, 1, 1);

    gtk_grid_attach(GTK_GRID(grid_forecast), gtk_label_new("Seasonal period (s):"), 0, r_fc, 1, 1);
    GtkWidget *spin_fc_seasonal = gtk_spin_button_new_with_range(1, 52, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_fc_seasonal), opts.forecast_seasonal);
    g_signal_connect(spin_fc_seasonal, "value-changed", G_CALLBACK(on_forecast_seasonal_changed), NULL);
    gtk_grid_attach(GTK_GRID(grid_forecast), spin_fc_seasonal, 1, r_fc++, 1, 1);

        // Botón "Run & Forecast"
    GtkWidget *btn_run_forecast = gtk_button_new_with_label("Run & Forecast");
    gtk_widget_set_sensitive(btn_run_forecast, opts.props_set);
    g_signal_connect(btn_run_forecast, "clicked", G_CALLBACK(on_run_and_forecast), NULL);
    gtk_grid_attach(GTK_GRID(grid_forecast), btn_run_forecast, 0, r_fc++, 2, 1);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), grid_forecast, gtk_label_new("Forecast"));


    /* ----- Action buttons (single row) ----- */
    GtkWidget *hbox_actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox_actions, FALSE, FALSE, 0);

    btn_run = gtk_button_new_with_label("Run DRVARMA");
    g_signal_connect(btn_run, "clicked", G_CALLBACK(on_run), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_actions), btn_run, FALSE, FALSE, 0);
    gtk_widget_set_sensitive(btn_run, FALSE);

    GtkWidget *btn_view_out = gtk_button_new_with_label("View Output");
    g_signal_connect(btn_view_out, "clicked", G_CALLBACK(on_view_output), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_actions), btn_view_out, FALSE, FALSE, 0);

    GtkWidget *btn_view_inp = gtk_button_new_with_label("View .inp");
    g_signal_connect(btn_view_inp, "clicked", G_CALLBACK(on_view_inp), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_actions), btn_view_inp, FALSE, FALSE, 0);

    GtkWidget *btn_johansen = gtk_button_new_with_label("Johansen Test");
    g_signal_connect(btn_johansen, "clicked", G_CALLBACK(on_johansen_test), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_actions), btn_johansen, FALSE, FALSE, 0);

    GtkWidget *btn_vecm = gtk_button_new_with_label("Estimate VECM");
    g_signal_connect(btn_vecm, "clicked", G_CALLBACK(on_vecm_estimate), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_actions), btn_vecm, FALSE, FALSE, 0);

    GtkWidget *btn_varorder = gtk_button_new_with_label("Select VAR Order");
    g_signal_connect(btn_varorder, "clicked", G_CALLBACK(on_select_var_order), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_actions), btn_varorder, FALSE, FALSE, 0);

    GtkWidget *btn_quit = gtk_button_new_with_label("Quit");
    g_signal_connect(btn_quit, "clicked", G_CALLBACK(on_quit), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_actions), btn_quit, FALSE, FALSE, 0);

    /* ----- Output viewer (large) ----- */
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);

     text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_NONE);  // no wrapping

    // Set monospace font via CSS
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        "textview { font-family: monospace; }", -1, NULL);
    gtk_style_context_add_provider(gtk_widget_get_style_context(text_view),
                                   GTK_STYLE_PROVIDER(provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);

    gtk_container_add(GTK_CONTAINER(scrolled), text_view);

    gtk_widget_show_all(main_window);
}

/* ---------- main ---------- */
int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "C");

#ifdef _WIN32
    // Obtener la ruta del directorio que contiene el ejecutable
    wchar_t wpath[MAX_PATH];
    GetModuleFileNameW(NULL, wpath, MAX_PATH);
    char *exe_path = g_utf16_to_utf8(wpath, -1, NULL, NULL, NULL);
    char *exe_dir = g_path_get_dirname(exe_path);
    g_free(exe_path);

    // Construir rutas absolutas para los recursos
    char *data_dir = g_build_filename(exe_dir, "share", NULL);
    char *pixbuf_cache = g_build_filename(exe_dir, "lib", "gdk-pixbuf-2.0", "2.10.0", "loaders.cache", NULL);
    char *schema_dir = g_build_filename(exe_dir, "share", "glib-2.0", "schemas", NULL);

    // Establecer variables de entorno
    g_setenv("XDG_DATA_DIRS", data_dir, TRUE);
    g_setenv("GDK_PIXBUF_MODULE_FILE", pixbuf_cache, TRUE);
    g_setenv("GSETTINGS_SCHEMA_DIR", schema_dir, TRUE);
    // Opcional: usar backend memory si los esquemas no son críticos
    // g_setenv("GSETTINGS_BACKEND", "memory", TRUE);
    g_setenv("GTK_THEME", "Windows", TRUE);  // Tema básico

    g_free(data_dir);
    g_free(pixbuf_cache);
    g_free(schema_dir);
    g_free(exe_dir);
#endif


    GtkApplication *app = gtk_application_new("org.example.drvarmagui", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(create_ui), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    /* Clean up data */
    if (data.data) {
        for (int i = 0; i < data.n_obs; i++) g_free(data.data[i]);
        g_free(data.data);
    }
    if (data.input_filename) g_free(data.input_filename);
    if (data.base_name) g_free(data.base_name);
    return status;
}