/*****************************************************************************/
/*  drvarma.c -- part of drvarma (multivariate VARMA modelling).
 *
 *  Copyright (C) 1995-2026 A.B. Treadway, J.A. Mauricio & D.E. Guerrero.
 *
 *  This program is free software: you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.  Distributed WITHOUT ANY WARRANTY; see the
 *  GNU General Public License (file COPYING) for details.
 *****************************************************************************/

/*****************************************************************************/
/*  DRVARMA.C                                                               */
/*  Exact maximum likelihood estimation of general VARMA(p,q) models.      */
/*  Refactored from original DRV.C to support flexible model specification. */
/*  Usage: drvarma input p q [-mean] [-diagar] [-diagma] [-diagcov] [-m mtd]*/
/*         [-twostep]                                                        */
/*         input : data file name (without .inp)                            */
/*         p     : AR order                                                  */
/*         q     : MA order                                                  */
/*         -mean       : include mean vector (default: no)                  */
/*         -diagar     : diagonal AR matrices (default: full)               */
/*         -diagma     : diagonal MA matrices (default: full)               */
/*         -diagcov    : diagonal covariance matrix (default: full)         */
/*         -m method   : 1 = exact, xi truncated at 1e-3 (default); 2 = exact, */
/*                       no truncation (the labels were swapped until 5.0)    */
/*         -twostep    : use two-step initialization (diagonal then full)   */
/*****************************************************************************/

#include "main.h"
#include "forecast.h"
#include "transform.h"
#include "deseason.h"
#include "escalera.h"
#include "inpread.h"
#include "version.h"
#include <getopt.h>  /* optional, can be replaced by manual parsing */


real macheps;
FILE *outputv;
int quiet_mode = 0;   /* default: not quiet */

real **datamat;        /* stationary data used for estimation (nobs x nser) */
int nser, nobs;        /* nser series, nobs = effective (post-diff) length  */

/* New .inp metadata (fue-style header) */
int data_freq      = 1;    /* 1=A, 4=Q, 12=M */
int data_start_sub = 1;    /* starting subperiod (1..freq) */
int data_start_year = 1;   /* starting year */
char **series_names = NULL; /* nser series names (1-based) */

/* Global Box-Cox / differencing spec (applied to every series) */
real trans_lambda = 1.0;   /* Box-Cox lambda (0 = log, 1 = none) */
real trans_scale  = 100.0; /* rescale factor applied after Box-Cox (-scale).
                              Default 100: rescaling improves the conditioning of
                              the convergence criteria (per J.A. Mauricio); the
                              optimum is unchanged and forecasts are inverted. */
int  trans_d      = 0;     /* number of regular differences */
int  trans_D      = 0;     /* number of seasonal differences (lag = data_freq) */

/* Box-Cox series before differencing + its length, kept to invert forecasts */
real **bc_series  = NULL;
int  nobs_raw     = 0;

/* CLI harmonic seasonal adjustment (-deseason) */
int  do_deseason     = 0;      /* 1 if -deseason requested */
int  deseason_mode   = 0;      /* 0 = auto (p<0.05), 1 = force all */
real **seasonal_dummies = NULL;/* 1..nser x 0..freq-1 (kept for re-seasonalization) */

/* Global model configuration */
int global_p, global_q;
int global_include_mean = 0;
int global_diag_ar = 0;
int global_diag_ma = 0;
int global_diag_cov = 0;
int met = 1;   /* 1: exact, xi truncated at 1e-3 (xitol > 0); 2: exact, untruncated */
int global_twostep = 0;  /* two-step initialization */

/* Fixed-parameter recursive forecasting: estimate once on the first g_estwin
   effective observations, then forecast from every origin with FIXED params.
   g_in_est is 1 only while the optimizer runs, so shootx restricts the
   likelihood to the estimation window without shrinking the allocated buffers. */
int g_estwin = 0;
int g_in_est = 0;

int do_exp_vol = 0;          // flag para -volexp
real exp_alpha = 0.05;        // nivel de significancia (default)
int exp_window = 20;          // ventana para la suma ponderada (default)

int do_mov_vol = 0;           // flag para -volmov
int mov_window = 20;           // ventana móvil (default)


/*****************************************************************************/
/*  Local function prototypes                                                */
/*****************************************************************************/
static void shootx(real *x, struct Tvarma *armax, int *ifaultx, int firstx, int lastx);
static int calc_nparametrs(void);
static void init_varma(real *x, int npar);
static void init_diag_varma(real *x, int npar);
static void hannan_rissanen_diag(real *x, int npar);  /* new: HR diagonal init */
static void combine_vectors(real *x_full, real *x_diag, int npar_full, int npar_diag);
static void print_parameters(real *x, real *dev, real **cov, int npar, struct Tvarma *varma);

//static real normal_cdf(real x);
static void sig_code(real p, char *sig);
void print_matrices(struct Tvarma *varma);
void print_roots(struct Tvarma *varma);

static void debug_print_vector(const char *label, real *v, int n);

/*****************************************************************************/
/*  Main function                                                            */
/*****************************************************************************/
int main(int argc, char *argv[])
{
    STRING inputf, outputf, base_name;
    real *x, *dev, **cov, gradtol, steptol;
    int npar, maxits, nrits, ifault, i, j, k;
    int do_forecast = 0;
    int forecast_horizon = 0;
    int seasonal_period = 1;
    int estwin_raw = 0;       /* -estwin N: raw obs for fixed-param estimation */

    struct Tvarma varma1;

    /* [0] The ladder: .pre input (drvarma 5.0). Everything below is the
       .inp path, unchanged since 0.4.1 (tests/banco).                     */
    if (argc > 1 && strcmp(argv[1], "-version") == 0) {
        printf("drvarma %s\n", DRVARMA_VERSION_FULL);
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "-split") == 0)
        return split_main(argc, argv);
    if (escalera_requested(argc, argv))
        return escalera_main(argc, argv);

    /* [1] Process command line arguments */
    if (argc < 4) {
        printf("drvarma %s\n", DRVARMA_VERSION_FULL);
        printf("Usage: %s file p q [-mean] [-diagar] [-diagma] [-diagcov] [-m method] [-twostep]\n", argv[0]);
        printf("       [-volexp [alpha window]] [-volmov [window]]\n");
        printf("  method: 1 = exact, xi truncated at 1e-3 (default); 2 = exact, no truncation\n");
        printf("  -twostep: use two-step initialization (diagonal then full)\n");
        printf("  -lik elf|shea|both: exact likelihood by Mauricio's AS 311 (default), by\n"
               "       Shea's AS 242 (the independent benchmark), or elf checked against\n"
               "       Shea at every point the optimizer visits\n");
        printf("  -hessian fd|bfgs: standard errors from fdhess at the optimum (default)\n"
               "       or from the BFGS Hessian of the search\n");
        printf("  -volexp [alpha window]: compute exponential volatility (alpha default 0.05, window default 20)\n");
        printf("  -volmov [window]: compute moving-window volatility (window default 20)\n");
        printf("  -deseason [auto|force]: harmonic seasonal adjustment of RAW series\n");
        printf("       auto = only series with significant seasonality (default); force = all\n");
        printf("  -scale factor: rescale the series (multiply after Box-Cox); forecasts inverted (default 100)\n");
        printf("  -estwin N: estimate params on first N raw obs, then write <base>.recursive\n");
        printf("       with fixed-parameter forecasts from every origin (needs -forecast H)\n");
        escalera_usage(argv[0]);
        printf("       %s -split FILE[.inp] [-mean] [-harmonics] [-ar P] [-ma Q] [-scale F] [-dir DIR]\n", argv[0]);
        printf("  The multivariate .inp (the first form) is DEPRECATED since 5.0: -split\n");
        printf("       converts it into one univariate .inp of fue per series, for the ladder.\n");
        printf("  -version: print the version and exit\n");
            exit(1);
    }

    /* The multivariate .inp is deprecated since 5.0: one line, on stderr. */
    fprintf(stderr, "Note: the multivariate .inp is deprecated since drvarma 5.0; "
                    "convert it with: drvarma -split %s\n", argv[1]);

    inputf = NEW_STR(80);
    outputf = NEW_STR(80);
    base_name = NEW_STR(80);
    strcpy(inputf, argv[1]);
    strcpy(base_name, argv[1]);

    global_p = atoi(argv[2]);
    global_q = atoi(argv[3]);

    /* Parse additional options */
    est_fdhess = 1;     /* fdhess by default: drvarma-python docs/STUDY-standard-errors.md */
    for (i = 4; i < argc; i++) {
        if (strcmp(argv[i], "-mean") == 0)
        global_include_mean = 1;
        else if (strcmp(argv[i], "-diagar") == 0)
        global_diag_ar = 1;
        else if (strcmp(argv[i], "-diagma") == 0)
        global_diag_ma = 1;
        else if (strcmp(argv[i], "-diagcov") == 0)
        global_diag_cov = 1;
        else if (strcmp(argv[i], "-m") == 0 && i+1 < argc)
        met = atoi(argv[++i]);
        else if (strcmp(argv[i], "-twostep") == 0)
        global_twostep = 1;
        else if (strcmp(argv[i], "-lik") == 0) {
        if (i+1 < argc && (strcmp(argv[i+1], "elf") == 0 || strcmp(argv[i+1], "shea") == 0
                           || strcmp(argv[i+1], "both") == 0)) {
            i++;
            est_lik = strcmp(argv[i], "shea") == 0 ? LIK_SHEA
                    : strcmp(argv[i], "both") == 0 ? LIK_BOTH : LIK_ELF;
            }
        else {
            printf("ERROR: -lik takes elf, shea or both\n");
            exit(1);
            }
        }
        else if (strcmp(argv[i], "-hessian") == 0) {
        if (i+1 < argc && (strcmp(argv[i+1], "fd") == 0 || strcmp(argv[i+1], "bfgs") == 0))
            est_fdhess = (strcmp(argv[++i], "fd") == 0);
        else {
            printf("ERROR: -hessian takes fd or bfgs\n");
            exit(1);
            }
        }
    /* Nuevas opciones de volatilidad */
        else if (strcmp(argv[i], "-volexp") == 0) {
        do_exp_vol = 1;
        if (i+1 < argc && sscanf(argv[i+1], "%lf", &exp_alpha) == 1) {
            i++;
            if (i+1 < argc && sscanf(argv[i+1], "%d", &exp_window) == 1)
                i++;
            }
        }
        else if (strcmp(argv[i], "-volmov") == 0) {
        do_mov_vol = 1;
            if (i+1 < argc && sscanf(argv[i+1], "%d", &mov_window) == 1)
            i++;
            }
        else if (strcmp(argv[i], "-forecast") == 0 && i+1 < argc) {
        do_forecast = 1;
        forecast_horizon = atoi(argv[++i]);
            }
        else if (strcmp(argv[i], "-estwin") == 0 && i+1 < argc) {
        estwin_raw = atoi(argv[++i]);
            }

        else if (strcmp(argv[i], "-seasonal") == 0 && i+1 < argc) {
        seasonal_period = atoi(argv[++i]);
            }
        else if (strcmp(argv[i], "-scale") == 0 && i+1 < argc) {
        trans_scale = atof(argv[++i]);
        if (trans_scale == 0.0) {
            printf("ERROR: -scale factor must be non-zero\n");
            exit(1);
            }
        }
        else if (strcmp(argv[i], "-deseason") == 0) {
        do_deseason = 1;
        deseason_mode = 0;                 /* default: auto (p<0.05) */
        if (i+1 < argc) {
            if (strcmp(argv[i+1], "force") == 0) { deseason_mode = 1; i++; }
            else if (strcmp(argv[i+1], "auto") == 0) { deseason_mode = 0; i++; }
            }
        }
        }

    strcpy(outputf, inputf);
    strcat(outputf, ".out");
    strcat(inputf, ".inp");

    printf("\nInput Data File  : %s\n", inputf);
    printf("Output File      : %s\n", outputf);
    printf("Model: VARMA(%d,%d)\n", global_p, global_q);
    printf("Include mean     : %s\n", global_include_mean ? "yes" : "no");
    printf("Diagonal AR      : %s\n", global_diag_ar ? "yes" : "no");
    printf("Diagonal MA      : %s\n", global_diag_ma ? "yes" : "no");
    printf("Diagonal Cov     : %s\n", global_diag_cov ? "yes" : "no");
    printf("Estimation method: %d\n", met);
    printf("Two-step init    : %s\n", global_twostep ? "yes" : "no");

    /* [2] Read data (fue-style header + raw level data).  The reader is in
       inpread.c, shared with -split; this format is deprecated since 5.0. */
    {
        MvInp in;
        mvinp_read(inputf, &in);
        data_freq = in.freq;  nser = in.nser;  nobs_raw = in.nobs;
        data_start_sub = in.start_sub;  data_start_year = in.start_year;
        series_names = in.names;
        trans_lambda = in.lambda;  trans_d = in.d;  trans_D = in.D;
        real **raw = in.raw;

        /* Optional harmonic seasonal adjustment on the RAW levels (-deseason).
           Same tested algorithm as the GUI; estimated on first differences,
           level dummies subtracted from the levels.  Dummies are kept in
           seasonal_dummies for later re-seasonalization of the forecasts. */
        if (do_deseason) {
            seasonal_dummies = (real **) malloc((nser + 1) * sizeof(real *));
            for (j = 1; j <= nser; j++)
                seasonal_dummies[j] = (real *) malloc(data_freq * sizeof(real));
            /* For fixed-parameter recursive forecasting the seasonal dummies must
               also be estimated ONLY on the training window (-estwin), then
               subtracted from the held-out tail — otherwise the deseasonalized
               series leaks future seasonality into the "fixed" model. */
            int des_nobs = (estwin_raw > 0 && estwin_raw < nobs_raw)
                           ? estwin_raw : nobs_raw;
            printf("Seasonal adjustment (harmonic, s=%d, mode=%s, window=%d obs):\n",
                   data_freq, deseason_mode ? "force all" : "auto (p<0.05)", des_nobs);
            deseasonalize_raw(raw, des_nobs, nser, data_freq, data_start_sub,
                              deseason_mode, seasonal_dummies, stdout);
            for (int t = des_nobs + 1; t <= nobs_raw; t++)
                for (j = 1; j <= nser; j++) {
                    int period = (t + data_start_sub - 2) % data_freq;
                    raw[t][j] -= seasonal_dummies[j][period];
                }
        }

        /* Apply Box-Cox + regular/seasonal differencing -> stationary datamat */
        int ifault_tr = 0, neff = 0;
        datamat = transform_series(raw, nobs_raw, nser,
                                   trans_lambda, trans_scale, trans_d, trans_D, data_freq,
                                   &neff, &bc_series, &ifault_tr);
        free_matrix(raw, 1, nobs_raw, 1, nser);
        if (datamat == NULL) {
            if (ifault_tr == 1)
                printf("ERROR: %s: not enough observations for d=%d, D=%d (freq=%d)\n",
                       inputf, trans_d, trans_D, data_freq);
            else if (ifault_tr == 2)
                printf("ERROR: %s: non-positive value incompatible with Box-Cox "
                       "lambda=%g (use lambda=1 for untransformed data)\n",
                       inputf, trans_lambda);
            else
                printf("ERROR: %s: data transformation failed (code %d)\n",
                       inputf, ifault_tr);
            exit(1);
        }
        nobs = neff;   /* effective length after differencing */
    }
    printf("Frequency        : %d\n", data_freq);
    printf("Start            : %d %d\n", data_start_sub, data_start_year);
    printf("Box-Cox lambda   : %g\n", trans_lambda);
    printf("Rescale factor   : %g\n", trans_scale);
    printf("Differences      : d=%d, D=%d (seasonal lag=%d)\n",
           trans_d, trans_D, data_freq);
    printf("Deseasonalize    : %s\n", do_deseason
           ? (deseason_mode ? "harmonic (force all)" : "harmonic (auto)") : "no");
    printf("Series: %d, Observations: %d (raw %d)\n", nser, nobs, nobs_raw);

    /* Estimation window for fixed-parameter recursive forecasting. The user
       gives raw obs; convert to effective (post-differencing) length. */
    if (estwin_raw > 0) {
        g_estwin = estwin_raw - trans_d - trans_D * data_freq;
        if (g_estwin < global_p + 1 || g_estwin > nobs) {
            printf("WARNING: -estwin %d invalid (effective %d, nobs %d); disabled.\n",
                   estwin_raw, g_estwin, nobs);
            g_estwin = 0;
        } else {
            printf("Estimation window: first %d raw obs (%d effective) -> params "
                   "fixed; recursive forecast from each origin to the data end.\n",
                   estwin_raw, g_estwin);
        }
    }

    /* [3] Open output file */
    if (NULL == (outputv = fopen(outputf, "w"))) {
        printf("ERROR: cannot create %s\n", outputf);
        exit(1);
    }
    fprintf(outputv, "Program          : drvarma %s\n", DRVARMA_VERSION_FULL);
    fprintf(outputv, "Input Data File  : %s\n", inputf);
    fprintf(outputv, "Output File      : %s\n", outputf);
    fprintf(outputv, "Model: VARMA(%d,%d)\n", global_p, global_q);
    fprintf(outputv, "Include mean     : %s\n", global_include_mean ? "yes" : "no");
    fprintf(outputv, "Diagonal AR      : %s\n", global_diag_ar ? "yes" : "no");
    fprintf(outputv, "Diagonal MA      : %s\n", global_diag_ma ? "yes" : "no");
    fprintf(outputv, "Diagonal Cov     : %s\n", global_diag_cov ? "yes" : "no");
    fprintf(outputv, "Estimation method: %d\n", met);
    fprintf(outputv, "Likelihood       : %s\n", lik_label());
    fprintf(outputv, "Two-step init    : %s\n", global_twostep ? "yes" : "no");
    fprintf(outputv, "Frequency        : %d\n", data_freq);
    fprintf(outputv, "Start            : %d %d\n", data_start_sub, data_start_year);
    fprintf(outputv, "Box-Cox lambda   : %g\n", trans_lambda);
    fprintf(outputv, "Rescale factor   : %g\n", trans_scale);
    fprintf(outputv, "Differences      : d=%d, D=%d (seasonal lag=%d)\n",
            trans_d, trans_D, data_freq);
    fprintf(outputv, "Deseasonalize    : %s\n", do_deseason
            ? (deseason_mode ? "harmonic (force all)" : "harmonic (auto)") : "no");
    fprintf(outputv, "Series names     :");
    for (j = 1; j <= nser; j++) fprintf(outputv, " %s", series_names[j]);
    fprintf(outputv, "\n");
    fprintf(outputv, "Observations     : %d (raw %d)\n\n", nobs, nobs_raw);

    /* [4] Determine number of parameters and allocate memory */
    npar = calc_nparametrs();
    x   = vector(1, npar);
    dev = vector(1, npar);
    cov = matrix(1, npar, 1, npar);

    printf("Number of parameters: %d\n", npar);
    fprintf(outputv, "Number of parameters: %d\n", npar);

    /* [5] Initialize parameters (pre-estimation) */
    init_varma(x, npar);
    debug_print_vector("Initial x (before two-step)", x, npar);

    /* [5b] Two-step initialization if requested and model not fully diagonal.
       Only meaningful when q > 0: the two-step (Hannan-Rissanen) exists to seed
       the MA part; with no MA the AR is already well initialized by OLS in
       init_varma, and the diagonal HR path would produce a degenerate start. */
    if (global_twostep && global_q == 0)
        printf("Note: -twostep ignored for q=0 (no MA part to initialize).\n");

    if (global_twostep && global_q > 0 &&
        (global_diag_ar == 0 || global_diag_ma == 0 || global_diag_cov == 0)) {
        printf("Performing two-step initialization (Hannan-Rissanen)...\n");
        /* Save current flags */
        int save_diag_ar = global_diag_ar;
        int save_diag_ma = global_diag_ma;
        int save_diag_cov = global_diag_cov;
        /* Force diagonal */
        global_diag_ar = global_diag_ma = global_diag_cov = 1;
        int npar_diag = calc_nparametrs();
        real *x_diag = vector(1, npar_diag);

        /* Initialize diagonal model using Hannan-Rissanen */
        hannan_rissanen_diag(x_diag, npar_diag);
        debug_print_vector("Diagonal HR estimates", x_diag, npar_diag);

        /* Restore original flags BEFORE combining */
        global_diag_ar = save_diag_ar;
        global_diag_ma = save_diag_ma;
        global_diag_cov = save_diag_cov;

        /* Combine vectors: replace diagonal elements with HR estimates */
        combine_vectors(x, x_diag, npar, npar_diag);
        printf("Two-step initialization completed.\n");
        debug_print_vector("Combined x (after two-step)", x, npar);

        /* Free temporary memory */
        free_vector(x_diag, 1, npar_diag);
    }

    /* [6] Optimizer parameters */
    maxits  = 500;
    nrits   = 10;
    gradtol = 1.0e-7;
    steptol = 1.0e-7;
    ifault  = 0;
    macheps = cmacheps();

    varma1.xitol = (met == 2) ? -1.0e-3 : 1.0e-3;

    debug_print_vector("Final x (before estimation)", x, npar);
    /* [7] Estimate */
    shootx(x, &varma1, &ifault, 1, 0);   /* allocate memory (full nobs) */
    g_in_est = 1;                        /* restrict likelihood to estimation window */
    /* Q is estimated unnormalised, and the likelihood concentrates sigma2, so
       Q -> cQ is a flat direction: fdhess holds qq[1,1], the first element of
       Q, which is the first of the last n_cov parameters.                    */
    est_fixed = npar - (global_diag_cov ? nser : nser * (nser + 1) / 2) + 1;
    est(&shootx, npar, x, dev, cov, maxits, nrits, gradtol, steptol,
        varma1.xitol, varma1.a, &varma1.sigma2, &varma1.logelf, &ifault);

    /* Preserve the estimation status: the retrieval shootx() below resets
       *ifaultx to 0, which would otherwise hide a failed estimation. */
    int est_fault = ifault;
    if (est_fault) {
        const char *msg;
        switch (est_fault) {
            case 1: msg = "Q matrix not positive definite"; break;
            case 2: msg = "unit root in AR";                break;
            case 3: msg = "AR nonstationary";               break;
            case 4: msg = "MA noninvertible";               break;
            case 5: msg = "numerical problem";              break;
            case 6: msg = "error in shootx";                break;
            default: msg = "unknown";                       break;
        }
        printf("Estimation error: %s (code %d)\n", msg, est_fault);
        fprintf(outputv, "\n**** ESTIMATION FAILED: %s (code %d)\n", msg, est_fault);
        fprintf(outputv, "No results are produced; check the model "
                         "specification / starting values.\n");
    }

    /* [8] Retrieve final model */
    shootx(x, &varma1, &ifault, 0, 0);

  if (est_fault == 0) {
    /* [9] Write results */
    print_parameters(x, dev, cov, npar, &varma1);

    all_hypothesis_tests(x, cov, npar, nser, global_p, global_q,
                         global_include_mean,
                         global_diag_ar, global_diag_ma, global_diag_cov,
                         outputv);
    int irf_horizon = 20;
    if (nobs < 40) irf_horizon = 10;
    impulse_response(&varma1, irf_horizon, outputv);
    variance_decomposition(&varma1, irf_horizon, outputv);

    multivariate_diagnostics(varma1.a, varma1.n, varma1.m, outputv);
    print_matrices(&varma1);
    /* The number every comparison between models needs, and the .inp path
       never printed it (the ladder does). Added with -lik (2026-09-28).     */
    fprintf(outputv, "Exact log-likelihood: %.8f   (%s)\n", varma1.logelf, lik_label());
    lik_check_report(outputv);
    fprintf(outputv, "\n");
    print_roots(&varma1);
    diagnose(&varma1);
    if (ifault == 0) {
     if (do_exp_vol) {
        compute_exponential_volatility(&varma1, exp_alpha, exp_window, base_name);
     }
     if (do_mov_vol) {
        compute_moving_window_volatility(&varma1, mov_window, base_name);
     }
    }
    /* [9.1] Write also forecast if the flag is active*/
    if (ifault == 0 && do_forecast) {
    int L = forecast_horizon;
    real **f_level = matrix(1, nser, 1, L);
    real ***v_level = tensor(1, L, 1, nser, 1, nser);
    real ***v_diff = tensor(1, L, 1, nser, 1, nser);
    real ***v_seas = tensor(1, L, 1, nser, 1, nser);

    // Los datos centrados w ya están en varma1.w
    // Los residuos a están en varma1.a
    // La matriz de covarianza sigma = sigma2 * qq
    real **sigma = matrix(1, nser, 1, nser);
    for (i = 1; i <= nser; i++)
        for (j = 1; j <= nser; j++)
            sigma[i][j] = varma1.sigma2 * varma1.qq[i][j];

    /* Forecast the stationary (Box-Cox + differenced) series modelled by the
       engine. f_level[k][l] is the forecast of that series; v_level its
       forecast-error variance (on the modelled scale). */
    forecast_model(nser, nobs, global_p, global_q, varma1.mu,
                   varma1.phi, varma1.theta, sigma,
                   varma1.w, varma1.a,
                   f_level, v_level, v_diff, v_seas,
                   0, L, seasonal_period, datamat);

    /* Integrate the forecasts back to ORIGINAL levels: undo regular/seasonal
       differencing and invert the Box-Cox transform (per series). */
    real **wf        = matrix(1, L, 1, nser);   /* wf[l][i] */
    real **level_fc  = matrix(1, nser, 1, L);   /* level_fc[i][l] */
    for (i = 1; i <= nser; i++)
        for (int l = 1; l <= L; l++)
            wf[l][i] = f_level[i][l];
    integrate_forecast(bc_series, nobs_raw, nser, wf, L,
                       trans_lambda, trans_scale, trans_d, trans_D, data_freq, level_fc);

    /* Forecast-error variances of the level (v_lvl) and of the monthly (1-B)
       and annual (1-B^s) variation, integrating the model's psi-weights
       through the differencing operator. All on the scale*Box-Cox scale. */
    real ***v_lvl = tensor(1, L, 1, nser, 1, nser);
    real ***v_mon = tensor(1, L, 1, nser, 1, nser);
    real ***v_ann = tensor(1, L, 1, nser, 1, nser);
    forecast_level_variances(nser, global_p, global_q, varma1.phi, varma1.theta,
                             sigma, L, trans_d, trans_D, data_freq,
                             v_lvl, v_mon, v_ann);

    /* Re-seasonalized level forecast (RAW units), 95% level bands, and the
       point forecast on the scale*Box-Cox scale (cf), used for the variation
       rates.  cf matches bc_series, so rates compare against the in-sample
       history bc_series. */
    real **level_raw = matrix(1, nser, 1, L);
    real **low_raw   = matrix(1, nser, 1, L);
    real **high_raw  = matrix(1, nser, 1, L);
    real **cf        = matrix(1, nser, 1, L);
    for (i = 1; i <= nser; i++)
        for (int l = 1; l <= L; l++) {
            real dseas = 0.0;
            if (do_deseason && seasonal_dummies) {
                int period = (nobs_raw + l + data_start_sub - 2) % data_freq;
                dseas = seasonal_dummies[i][period];
            }
            real sd_y = sqrt(v_lvl[l][i][i]);                 /* sd on scale*BC */
            cf[i][l] = trans_scale * boxcox_fwd(level_fc[i][l], trans_lambda);
            level_raw[i][l] = level_fc[i][l] + dseas;
            low_raw[i][l]  = boxcox_inv((cf[i][l] - 1.96 * sd_y) / trans_scale,
                                        trans_lambda) + dseas;
            high_raw[i][l] = boxcox_inv((cf[i][l] + 1.96 * sd_y) / trans_scale,
                                        trans_lambda) + dseas;
        }

    // Escribir resultados en un archivo .forecast
    char fname[256];
    strcpy(fname, base_name);
    strcat(fname, ".forecast");
    int sfreq = data_freq;
    FILE *ffore = fopen(fname, "w");
    if (ffore) {
        int per, sub;
        fprintf(ffore, "Forecasts from VARMA(%d,%d) model\n", global_p, global_q);
        fprintf(ffore, "lambda=%g, scale=%g, d=%d, D=%d, freq=%d, horizon=%d, deseason=%s\n",
                trans_lambda, trans_scale, trans_d, trans_D, data_freq, L,
                do_deseason ? (deseason_mode ? "force" : "auto") : "no");
        fprintf(ffore, "Level/Low95/High95 in original units%s. "
                       "mon%%/ann%% = monthly/annual variation rate and std "
                       "(100*delta/scale; %% for lambda=0).\n\n",
                do_deseason ? " (re-seasonalized)" : "");
        for (int k = 1; k <= nser; k++) {
            fprintf(ffore, "Series %d (%s):\n", k, series_names[k]);
            fprintf(ffore, "  date   %10s %10s %10s %8s %7s %8s %7s\n",
                    "Level", "Low95", "High95", "mon%", "std", "ann%", "std");
            for (int l = 1; l <= L; l++) {
                ObsToDate(data_start_year, data_start_sub, nobs_raw + l, data_freq,
                          &per, &sub);
                /* point variation on the scale*Box-Cox scale */
                real g2 = (l == 1) ? cf[k][1] - bc_series[nobs_raw][k]
                                   : cf[k][l] - cf[k][l-1];
                real g3;
                if (l <= sfreq)
                    g3 = (nobs_raw - sfreq + l >= 1)
                         ? cf[k][l] - bc_series[nobs_raw - sfreq + l][k] : 0.0;
                else
                    g3 = cf[k][l] - cf[k][l-sfreq];
                real sc = 100.0 / trans_scale;
                fprintf(ffore, "%3d/%4d %10.4f %10.4f %10.4f %8.4f %7.4f %8.4f %7.4f\n",
                        sub, per, level_raw[k][l], low_raw[k][l], high_raw[k][l],
                        sc * g2, sc * sqrt(v_mon[l][k][k]),
                        sc * g3, sc * sqrt(v_ann[l][k][k]));
            }
            fprintf(ffore, "\n");
        }
        fclose(ffore);
        printf("Forecasts written to %s\n", fname);
    }
    free_matrix(cf, 1, nser, 1, L);
    free_matrix(high_raw, 1, nser, 1, L);
    free_matrix(low_raw, 1, nser, 1, L);
    free_matrix(level_raw, 1, nser, 1, L);
    free_tensor(v_ann, 1, L, 1, nser, 1, nser);
    free_tensor(v_mon, 1, L, 1, nser, 1, nser);
    free_tensor(v_lvl, 1, L, 1, nser, 1, nser);

    free_matrix(level_fc, 1, nser, 1, L);
    free_matrix(wf, 1, L, 1, nser);
    free_tensor(v_seas, 1, L, 1, nser, 1, nser);
    free_tensor(v_diff, 1, L, 1, nser, 1, nser);
    free_tensor(v_level, 1, L, 1, nser, 1, nser);
    /* [9.2] Fixed-parameter RECURSIVE forecasts from multiple origins.
       Params are frozen at the estimation window; for each origin e in
       [g_estwin, nobs] forecast H steps using only data up to e (offset
       b = nobs - e), integrate anchored at the origin and re-seasonalize.
       Writes <base>.recursive for out-of-sample evaluation at several origins. */
    if (g_estwin > 0) {
        int H   = L;
        int off = trans_d + trans_D * data_freq;     /* raw index = effective + off */
        char rname[300];
        strcpy(rname, base_name); strcat(rname, ".recursive");
        FILE *frec = fopen(rname, "w");
        if (frec) {
            fprintf(frec, "Recursive fixed-parameter forecasts VARMA(%d,%d)\n",
                    global_p, global_q);
            fprintf(frec, "estwin_eff=%d nobs_eff=%d horizon=%d deseason=%s\n",
                    g_estwin, nobs, H,
                    do_deseason ? (deseason_mode ? "force" : "auto") : "no");
            fprintf(frec, "origin series horizon level\n");
            real **f_e   = matrix(1, nser, 1, H);
            real ***ve1  = tensor(1, H, 1, nser, 1, nser);
            real ***ve2  = tensor(1, H, 1, nser, 1, nser);
            real ***ve3  = tensor(1, H, 1, nser, 1, nser);
            real **wfe   = matrix(1, H, 1, nser);
            real **lvle  = matrix(1, nser, 1, H);
            for (int e = g_estwin; e <= nobs; e++) {
                int b = nobs - e;
                int r = e + off;                     /* raw index of the origin */
                forecast_model(nser, nobs, global_p, global_q, varma1.mu,
                               varma1.phi, varma1.theta, sigma,
                               varma1.w, varma1.a,
                               f_e, ve1, ve2, ve3,
                               b, H, seasonal_period, datamat);
                for (i = 1; i <= nser; i++)
                    for (int l = 1; l <= H; l++) wfe[l][i] = f_e[i][l];
                integrate_forecast(bc_series, r, nser, wfe, H,
                                   trans_lambda, trans_scale, trans_d, trans_D,
                                   data_freq, lvle);
                int oper, osub;
                ObsToDate(data_start_year, data_start_sub, r, data_freq, &oper, &osub);
                for (int kk = 1; kk <= nser; kk++)
                    for (int l = 1; l <= H; l++) {
                        real dseas = 0.0;
                        if (do_deseason && seasonal_dummies) {
                            int period = (r + l + data_start_sub - 2) % data_freq;
                            dseas = seasonal_dummies[kk][period];
                        }
                        fprintf(frec, "%d/%d %s %d %.6f\n",
                                osub, oper, series_names[kk], l, lvle[kk][l] + dseas);
                    }
            }
            free_matrix(lvle, 1, nser, 1, H);
            free_matrix(wfe, 1, H, 1, nser);
            free_tensor(ve3, 1, H, 1, nser, 1, nser);
            free_tensor(ve2, 1, H, 1, nser, 1, nser);
            free_tensor(ve1, 1, H, 1, nser, 1, nser);
            free_matrix(f_e, 1, nser, 1, H);
            fclose(frec);
            printf("Recursive forecasts written to %s\n", rname);
        }
    }

    free_matrix(f_level, 1, nser, 1, L);
    free_matrix(sigma, 1, nser, 1, nser);
    }
  }  /* end if (est_fault == 0): results block */

    /* [10] Free memory and close */
    g_in_est = 0;                        /* restore full nobs for correct frees */
    shootx(x, &varma1, &ifault, 0, 1);   /* deallocate model memory (full nobs) */
    free_matrix(cov, 1, npar, 1, npar);
    free_vector(dev, 1, npar);
    free_vector(x, 1, npar);
    free_matrix(datamat, 1, nobs, 1, nser);
    free_matrix(bc_series, 1, nobs_raw, 1, nser);
    for (j = 1; j <= nser; j++) free(series_names[j]);
    free(series_names);
    if (seasonal_dummies) {
        for (j = 1; j <= nser; j++) free(seasonal_dummies[j]);
        free(seasonal_dummies);
    }
    FREE_STR(outputf);
    FREE_STR(inputf);
    FREE_STR(base_name);
    fclose(outputv);
    return 0;
}

/*****************************************************************************/
/*  shootx: maps the parameter vector to the Tvarma structure               */
/*  (Unchanged from original, but with added comments in English)           */
/*****************************************************************************/
static void shootx(real *x, struct Tvarma *armax, int *ifaultx, int firstx, int lastx)
{
    int i, j, k, idx = 1;
    int m = nser;        /* system dimension */
    int p = global_p;
    int q = global_q;

    *ifaultx = 0;

    /* [1] Set dimensions.  During estimation (g_in_est) the likelihood is
       restricted to the first g_estwin observations; buffers stay full size. */
    armax->m = m;
    armax->n = (g_in_est && g_estwin > 0) ? g_estwin : nobs;
    armax->p = p;
    armax->q = q;

    /* [2] Allocate memory on first call */
    if (firstx) {
        armax->mu    = vector(1, m);
        armax->phi   = tensor(0, p, 1, m, 1, m);
        armax->theta = tensor(0, q, 1, m, 1, m);
        armax->qq    = matrix(1, m, 1, m);
        armax->w     = matrix(1, nobs, 1, m);
        armax->a     = matrix(1, nobs, 1, m);

        /* Initialize to zero */
        for (i = 1; i <= m; i++) {
            armax->mu[i] = 0.0;
            for (j = 1; j <= m; j++) {
                for (k = 0; k <= p; k++) armax->phi[k][i][j] = 0.0;
                for (k = 0; k <= q; k++) armax->theta[k][i][j] = 0.0;
                armax->qq[i][j] = 0.0;
            }
            for (j = 1; j <= nobs; j++) {
                armax->w[j][i] = 0.0;
                armax->a[j][i] = 0.0;
            }
        }
        /* phi[0] and theta[0] = identity */
        for (i = 1; i <= m; i++) {
            armax->phi[0][i][i] = 1.0;
            armax->theta[0][i][i] = 1.0;
        }
    }

    /* [3] Build unnormalized model (phi1, theta1, qq1, mu1) */
    /* Allocate temporary memory */
    real *mu1    = vector(1, m);
    real ***phi1   = tensor(0, p, 1, m, 1, m);
    real ***theta1 = tensor(0, q, 1, m, 1, m);
    real **qq1    = matrix(1, m, 1, m);

    /* Initialize to zero */
    for (i = 1; i <= m; i++) {
        mu1[i] = 0.0;
        for (j = 1; j <= m; j++) {
            for (k = 0; k <= p; k++) phi1[k][i][j] = 0.0;
            for (k = 0; k <= q; k++) theta1[k][i][j] = 0.0;
            qq1[i][j] = 0.0;
        }
        phi1[0][i][i] = 1.0;
        theta1[0][i][i] = 1.0;
    }

    /* [3a] Means (if included) */
    if (global_include_mean) {
        for (i = 1; i <= m; i++)
            mu1[i] = x[idx++];
    }

    /* [3b] AR parameters */
    for (k = 1; k <= p; k++) {
        if (global_diag_ar) {
            for (i = 1; i <= m; i++) {
                phi1[k][i][i] = x[idx++];
            }
        } else {
            for (i = 1; i <= m; i++) {
                for (j = 1; j <= m; j++) {
                    phi1[k][i][j] = x[idx++];
                }
            }
        }
    }

    /* [3c] MA parameters */
    for (k = 1; k <= q; k++) {
        if (global_diag_ma) {
            for (i = 1; i <= m; i++) {
                theta1[k][i][i] = x[idx++];
            }
        } else {
            for (i = 1; i <= m; i++) {
                for (j = 1; j <= m; j++) {
                    theta1[k][i][j] = x[idx++];
                }
            }
        }
    }

    /* [3d] Covariance matrix Q (lower triangular) */
    if (global_diag_cov) {
        for (i = 1; i <= m; i++) {
            qq1[i][i] = x[idx++];
        }
    } else {
        for (i = 1; i <= m; i++) {
            for (j = 1; j <= i; j++) {
                qq1[i][j] = x[idx++];
                qq1[j][i] = qq1[i][j];   /* symmetry */
            }
        }
    }

    /* [4] Normalization (as in original, using phi1[0] and theta1[0]) */
    /* Assumes functions ludcp, lusol, etc. are available */
    real **mtmp1 = matrix(1, m, 1, m);
    real **mtmp2 = matrix(1, m, 1, m);
    real **mtmp0 = matrix(1, m, 1, m);
    real *vtmp0  = vector(1, m);
    int *index   = ivector(1, m);

    /* mtmp0 = phi1[0] */
    for (i = 1; i <= m; i++)
        for (j = 1; j <= m; j++)
            mtmp0[i][j] = phi1[0][i][j];
    ludcp(mtmp0, m, index);
    for (j = 1; j <= m; j++) {
        for (i = 1; i <= m; i++) vtmp0[i] = 0.0;
        vtmp0[j] = 1.0;
        lusol(mtmp0, vtmp0, m, index);
        for (i = 1; i <= m; i++) mtmp1[i][j] = vtmp0[i];
    }

    /* mtmp0 = theta1[0] */
    for (i = 1; i <= m; i++)
        for (j = 1; j <= m; j++)
            mtmp0[i][j] = theta1[0][i][j];
    ludcp(mtmp0, m, index);
    for (j = 1; j <= m; j++) {
        for (i = 1; i <= m; i++) vtmp0[i] = 0.0;
        vtmp0[j] = 1.0;
        lusol(mtmp0, vtmp0, m, index);
        for (i = 1; i <= m; i++) mtmp2[i][j] = vtmp0[i];
    }

    free_ivector(index, 1, m);
    free_vector(vtmp0, 1, m);
    free_matrix(mtmp0, 1, m, 1, m);

    /* Normalized AR */
    for (k = 1; k <= p; k++)
        for (i = 1; i <= m; i++)
            for (j = 1; j <= m; j++) {
                armax->phi[k][i][j] = 0.0;
                for (int k1 = 1; k1 <= m; k1++)
                    armax->phi[k][i][j] += mtmp1[i][k1] * phi1[k][k1][j];
            }

    real **mtmp3 = matrix(1, m, 1, m);
    real **mtmp4 = matrix(1, m, 1, m);

    /* mtmp3 = theta1[0]^{-1} * phi1[0] */
    for (i = 1; i <= m; i++)
        for (j = 1; j <= m; j++) {
            mtmp3[i][j] = 0.0;
            for (int k1 = 1; k1 <= m; k1++)
                mtmp3[i][j] += mtmp2[i][k1] * phi1[0][k1][j];
        }

    /* Normalized MA */
    for (k = 1; k <= q; k++) {
        /* mtmp4 = phi1[0]^{-1} * theta1[k] */
        for (i = 1; i <= m; i++)
            for (j = 1; j <= m; j++) {
                mtmp4[i][j] = 0.0;
                for (int k1 = 1; k1 <= m; k1++)
                    mtmp4[i][j] += mtmp1[i][k1] * theta1[k][k1][j];
            }
        for (i = 1; i <= m; i++)
            for (j = 1; j <= m; j++) {
                armax->theta[k][i][j] = 0.0;
                for (int k1 = 1; k1 <= m; k1++)
                    armax->theta[k][i][j] += mtmp4[i][k1] * mtmp3[k1][j];
            }
    }

    /* mtmp3 = phi1[0]^{-1} * theta1[0] */
    for (i = 1; i <= m; i++)
        for (j = 1; j <= m; j++) {
            mtmp3[i][j] = 0.0;
            for (int k1 = 1; k1 <= m; k1++)
                mtmp3[i][j] += mtmp1[i][k1] * theta1[0][k1][j];
        }

    /* Make qq1 symmetric (just in case) */
    for (i = 1; i <= m; i++)
        for (j = i+1; j <= m; j++)
            qq1[i][j] = qq1[j][i];

    /* mtmp4 = phi1[0]^{-1} * theta1[0] * qq1 */
    for (i = 1; i <= m; i++)
        for (j = 1; j <= m; j++) {
            mtmp4[i][j] = 0.0;
            for (int k1 = 1; k1 <= m; k1++)
                mtmp4[i][j] += mtmp3[i][k1] * qq1[k1][j];
        }

    /* Normalized covariance: armax->qq = mtmp4 * mtmp3' */
    for (i = 1; i <= m; i++)
        for (j = 1; j <= m; j++) {
            armax->qq[i][j] = 0.0;
            for (int k1 = 1; k1 <= m; k1++)
                armax->qq[i][j] += mtmp4[i][k1] * mtmp3[j][k1];
        }

    /* Mean */
    for (i = 1; i <= m; i++)
        armax->mu[i] = mu1[i];

    /* Free temporary memory */
    free_matrix(mtmp4, 1, m, 1, m);
    free_matrix(mtmp3, 1, m, 1, m);
    free_matrix(mtmp2, 1, m, 1, m);
    free_matrix(mtmp1, 1, m, 1, m);
    free_matrix(qq1, 1, m, 1, m);
    free_tensor(theta1, 0, q, 1, m, 1, m);
    free_tensor(phi1, 0, p, 1, m, 1, m);
    free_vector(mu1, 1, m);

    /* [5] Data: copy datamat to armax->w */
    for (i = 1; i <= nobs; i++)
        for (j = 1; j <= m; j++)
            armax->w[i][j] = datamat[i][j];

    /* [6] Deallocate model memory if lastx is active */
    if (lastx) {
        free_matrix(armax->a, 1, armax->n, 1, armax->m);
        free_matrix(armax->w, 1, armax->n, 1, armax->m);
        free_matrix(armax->qq, 1, armax->m, 1, armax->m);
        free_tensor(armax->theta, 0, armax->q, 1, armax->m, 1, armax->m);
        free_tensor(armax->phi, 0, armax->p, 1, armax->m, 1, armax->m);
        free_vector(armax->mu, 1, armax->m);
    }
}

/*****************************************************************************/
/*  calc_nparametrs: calculates the number of parameters according to flags */
/*  (Unchanged)                                                              */
/*****************************************************************************/
static int calc_nparametrs(void)
{
    int npar = 0;
    int m = nser;

    if (global_include_mean)
        npar += m;

    int n_ar = global_diag_ar ? m : m * m;
    npar += n_ar * global_p;

    int n_ma = global_diag_ma ? m : m * m;
    npar += n_ma * global_q;

    int n_cov = global_diag_cov ? m : m * (m + 1) / 2;
    npar += n_cov;

    return npar;
}

/*****************************************************************************/
/*  init_varma: initial values for the full VARMA model                     */
/*  (Original method: OLS for AR, zero for MA, scaled covariance)           */
/*****************************************************************************/
static void init_varma(real *x, int npar)
{
    int m = nser;
    int i, j, k, t, idx = 1;
    real *mean_est = NULL;

    /* 1. Medias muestrales (si se incluyen) */
    if (global_include_mean) {
        mean_est = vector(1, m);
        for (j = 1; j <= m; j++) {
            real sum = 0.0;
            for (t = 1; t <= nobs; t++) sum += datamat[t][j];
            mean_est[j] = sum / nobs;
            x[idx++] = mean_est[j];
        }
    }

    /* Datos centrados */
    real **datac = matrix(1, nobs, 1, m);
    for (t = 1; t <= nobs; t++)
        for (j = 1; j <= m; j++)
            datac[t][j] = datamat[t][j] - (global_include_mean ? mean_est[j] : 0.0);

    /* Matrices para covarianza y correlación de residuos */
    real **resid_cov = matrix(1, m, 1, m);
    real **resid_corr = matrix(1, m, 1, m);
    for (i = 1; i <= m; i++)
        for (j = 1; j <= m; j++)
            resid_cov[i][j] = 0.0;

    /* 2. Parámetros AR y cálculo de residuos */
    if (global_p > 0) {
        int T = nobs - global_p;
        real **Z = matrix(1, T, 1, m * global_p);
        real **y = matrix(1, T, 1, m);

        /* Construir matriz de regresores y vector respuesta */
        for (t = global_p + 1; t <= nobs; t++) {
            int row = t - global_p;
            int col = 1;
            for (k = 1; k <= global_p; k++) {
                for (j = 1; j <= m; j++) {
                    Z[row][col++] = datac[t - k][j];
                }
            }
            for (j = 1; j <= m; j++)
                y[row][j] = datac[t][j];
        }

        /* Z'Z y factorización LU */
        real **ZtZ = matrix(1, m * global_p, 1, m * global_p);
        for (i = 1; i <= m * global_p; i++) {
            for (j = 1; j <= m * global_p; j++) {
                real sum = 0.0;
                for (t = 1; t <= T; t++) sum += Z[t][i] * Z[t][j];
                ZtZ[i][j] = sum;
            }
        }
        int *indx = ivector(1, m * global_p);
        ludcp(ZtZ, m * global_p, indx);

        /* Coeficientes por ecuación */
        real **coef = matrix(1, m, 1, m * global_p);
        for (int eq = 1; eq <= m; eq++) {
            real *Zty = vector(1, m * global_p);
            for (i = 1; i <= m * global_p; i++) {
                Zty[i] = 0.0;
                for (t = 1; t <= T; t++)
                    Zty[i] += Z[t][i] * y[t][eq];
            }
            lusol(ZtZ, Zty, m * global_p, indx);
            for (i = 1; i <= m * global_p; i++)
                coef[eq][i] = Zty[i];
            free_vector(Zty, 1, m * global_p);
        }

        /* Almacenar coeficientes AR en x */
        for (k = 1; k <= global_p; k++) {
            for (i = 1; i <= m; i++) {
                for (j = 1; j <= m; j++) {
                    if (global_diag_ar && i != j) continue;
                    /* Posicion dentro del bloque AR de x, segun el layout que
                       espera shootx: diagonal -> m valores por retardo (i=1..m);
                       completo -> m*m valores por retardo (fila i, columna j). */
                    int pos = global_diag_ar
                              ? (k - 1) * m + i
                              : (k - 1) * m * m + (i - 1) * m + j;
                    int coef_idx = (k - 1) * m + j;
                    x[idx + pos - 1] = coef[i][coef_idx];
                }
            }
        }
        idx += (global_diag_ar ? m : m * m) * global_p;

        /* Calcular residuos del AR y su matriz de covarianza */
        real **resid = matrix(1, nobs, 1, m);
        for (t = 1; t <= nobs; t++) {
            for (i = 1; i <= m; i++) {
                real pred = 0.0;
                for (k = 1; k <= global_p; k++) {
                    if (t > k) {
                        for (j = 1; j <= m; j++) {
                            pred += coef[i][(k-1)*m + j] * datac[t - k][j];
                        }
                    }
                }
                resid[t][i] = datac[t][i] - pred;
            }
        }
        for (i = 1; i <= m; i++) {
            for (j = 1; j <= m; j++) {
                real sum = 0.0;
                for (t = 1; t <= nobs; t++)
                    sum += resid[t][i] * resid[t][j];
                resid_cov[i][j] = sum / nobs;
            }
        }
        free_matrix(resid, 1, nobs, 1, m);
        free_matrix(Z, 1, T, 1, m * global_p);
        free_matrix(y, 1, T, 1, m);
        free_matrix(ZtZ, 1, m * global_p, 1, m * global_p);
        free_ivector(indx, 1, m * global_p);
        free_matrix(coef, 1, m, 1, m * global_p);
    }
    else {
        /* p == 0: usar covarianza muestral de los datos centrados */
        for (i = 1; i <= m; i++) {
            for (j = 1; j <= m; j++) {
                real sum = 0.0;
                for (t = 1; t <= nobs; t++)
                    sum += datac[t][i] * datac[t][j];
                resid_cov[i][j] = sum / nobs;
            }
        }
    }

    /* Convertir a matriz de correlación */
    for (i = 1; i <= m; i++) {
        real sd_i = sqrt(resid_cov[i][i]);
        if (sd_i < 1e-12) sd_i = 1.0;
        for (j = 1; j <= m; j++) {
            real sd_j = sqrt(resid_cov[j][j]);
            if (sd_j < 1e-12) sd_j = 1.0;
            resid_corr[i][j] = resid_cov[i][j] / (sd_i * sd_j);
        }
    }
    /* Regularización: diagonal = 1 y garantía de definida positiva */
    for (i = 1; i <= m; i++) {
        resid_corr[i][i] = 1.0;
        /* Añadir una pequeña constante si la matriz es casi singular */
        for (j = 1; j <= m; j++) {
            if (i == j) continue;
            if (fabs(resid_corr[i][j]) > 0.9999)
                resid_corr[i][j] *= 0.9999;
        }
    }

    /* 3. Parámetros MA (cero) */
    if (global_q > 0) {
        int n_ma_est = (global_diag_ma ? m : m * m) * global_q;
        for (i = 0; i < n_ma_est; i++)
            x[idx++] = 0.0;
    }

    /* 4. Parámetros de covarianza (basados en la matriz de correlación) */
    if (global_diag_cov) {
        for (i = 1; i <= m; i++)
            x[idx++] = 1.0;          /* varianza inicial = 1 */
    } else {
        for (i = 1; i <= m; i++) {
            for (j = 1; j <= i; j++) {
                x[idx++] = resid_corr[i][j];
            }
        }
    }

    /* Liberar memoria */
    free_matrix(resid_corr, 1, m, 1, m);
    free_matrix(resid_cov, 1, m, 1, m);
    free_matrix(datac, 1, nobs, 1, m);
    if (global_include_mean) free_vector(mean_est, 1, m);
}

/*****************************************************************************/
/*  init_diag_varma: initial values for diagonal model                      */
/*  Estimates univariate AR for each series, sets MA to zero,               */
/*  and scales residual variances to have average 1.                        */
/*****************************************************************************/
static void init_diag_varma(real *x, int npar)
{
    int m = nser;
    int p = global_p;
    int q = global_q;
    int i, j, k, t, idx = 1;
    real *mean_est = NULL;

    /* Sample means (if included) */
    if (global_include_mean) {
        mean_est = vector(1, m);
        for (j = 1; j <= m; j++) {
            real sum = 0.0;
            for (t = 1; t <= nobs; t++) sum += datamat[t][j];
            mean_est[j] = sum / nobs;
            x[idx++] = mean_est[j];
        }
    }

    /* Center data */
    real **datac = matrix(1, nobs, 1, m);
    for (t = 1; t <= nobs; t++)
        for (j = 1; j <= m; j++)
            datac[t][j] = datamat[t][j] - (global_include_mean ? mean_est[j] : 0.0);

    /* Arrays for diagonal AR coefficients and residuals */
    real **phi_diag = matrix(1, p, 1, m);  /* phi_diag[k][i] */
    real **resid = matrix(1, nobs, 1, m);

    for (i = 1; i <= m; i++) {
        int T = nobs - p;
        if (T <= 0) {
            for (k = 1; k <= p; k++) phi_diag[k][i] = 0.0;
            for (t = 1; t <= nobs; t++) resid[t][i] = datac[t][i];
            continue;
        }
        /* Build regressor matrix Z (p lags) and vector y */
        real **Z = matrix(1, T, 1, p);
        real *y = vector(1, T);
        for (t = p+1; t <= nobs; t++) {
            int row = t - p;
            for (k = 1; k <= p; k++)
                Z[row][k] = datac[t - k][i];
            y[row] = datac[t][i];
        }

        /* Z'Z */
        real **ZtZ = matrix(1, p, 1, p);
        for (k = 1; k <= p; k++) {
            for (j = 1; j <= p; j++) {
                real sum = 0.0;
                for (t = 1; t <= T; t++) sum += Z[t][k] * Z[t][j];
                ZtZ[k][j] = sum;
            }
        }
        int *indx = ivector(1, p);
        ludcp(ZtZ, p, indx);
        real *Zty = vector(1, p);
        for (k = 1; k <= p; k++) {
            Zty[k] = 0.0;
            for (t = 1; t <= T; t++) Zty[k] += Z[t][k] * y[t];
        }
        lusol(ZtZ, Zty, p, indx);

        for (k = 1; k <= p; k++) phi_diag[k][i] = Zty[k];

        /* Compute residuals for all observations */
        for (t = 1; t <= nobs; t++) {
            if (t <= p) {
                resid[t][i] = datac[t][i];
            } else {
                real pred = 0.0;
                for (k = 1; k <= p; k++) pred += phi_diag[k][i] * datac[t - k][i];
                resid[t][i] = datac[t][i] - pred;
            }
        }

        free_matrix(Z, 1, T, 1, p);
        free_vector(y, 1, T);
        free_matrix(ZtZ, 1, p, 1, p);
        free_ivector(indx, 1, p);
        free_vector(Zty, 1, p);
    }

    /* Store AR coefficients */
    for (k = 1; k <= p; k++)
        for (i = 1; i <= m; i++)
            x[idx++] = phi_diag[k][i];

    /* MA parameters (all zero) */
    for (k = 1; k <= q; k++)
        for (i = 1; i <= m; i++)
            x[idx++] = 0.0;

    /* Compute residual variances and scale them to have average 1 */
    real *var_resid = vector(1, m);
    for (i = 1; i <= m; i++) {
        real sum = 0.0;
        for (t = 1; t <= nobs; t++) sum += resid[t][i] * resid[t][i];
        var_resid[i] = sum / nobs;
    }
    real scale_factor = 0.0;
    for (i = 1; i <= m; i++) scale_factor += var_resid[i];
    scale_factor /= m;
    if (scale_factor < 1e-12) scale_factor = 1.0;
    for (i = 1; i <= m; i++) var_resid[i] /= scale_factor;
    for (i = 1; i <= m; i++) x[idx++] = var_resid[i];
    free_vector(var_resid, 1, m);

    /* Free memory */
    free_matrix(phi_diag, 1, p, 1, m);
    free_matrix(resid, 1, nobs, 1, m);
    free_matrix(datac, 1, nobs, 1, m);
    if (global_include_mean) free_vector(mean_est, 1, m);
}

/*****************************************************************************/
/*  combine_vectors: combines the refined diagonal vector with the original */
/*  Diagonal elements of AR, MA, and covariance are taken from x_diag;      */
/*  non?diagonal elements are kept from x_full (original).                  */
/*****************************************************************************/
static void combine_vectors(real *x_full, real *x_diag, int npar_full, int npar_diag)
{
    int m = nser;
    int p = global_p;
    int q = global_q;
    int idx_full = 1, idx_diag = 1;

    /* Means */
    if (global_include_mean) {
        for (int i = 1; i <= m; i++)
            x_full[idx_full++] = x_diag[idx_diag++];
    }

    /* AR */
    for (int k = 1; k <= p; k++) {
        if (global_diag_ar) {
            for (int i = 1; i <= m; i++)
                x_full[idx_full++] = x_diag[idx_diag++];
        } else {
            for (int i = 1; i <= m; i++) {
                for (int j = 1; j <= m; j++) {
                    if (i == j)
                        x_full[idx_full] = x_diag[idx_diag++];
                    /* else keep original value (already in x_full) */
                    idx_full++;
                }
            }
        }
    }

    /* MA */
    for (int k = 1; k <= q; k++) {
        if (global_diag_ma) {
            for (int i = 1; i <= m; i++)
                x_full[idx_full++] = x_diag[idx_diag++];
        } else {
            for (int i = 1; i <= m; i++) {
                for (int j = 1; j <= m; j++) {
                    if (i == j)
                        x_full[idx_full] = x_diag[idx_diag++];
                    /* else keep original value (zero in init_varma) */
                    idx_full++;
                }
            }
        }
    }

    /* Covariance */
    if (global_diag_cov) {
        for (int i = 1; i <= m; i++)
            x_full[idx_full++] = x_diag[idx_diag++];
    } else {
        for (int i = 1; i <= m; i++) {
            for (int j = 1; j <= i; j++) {
                if (i == j)
                    x_full[idx_full] = x_diag[idx_diag++];
                /* else keep original covariance (already scaled) */
                idx_full++;
            }
        }
    }

    /* Consistency check */
    if (idx_full - 1 != npar_full || idx_diag - 1 != npar_diag) {
        printf("Error in vector combination: inconsistent indices (full:%d vs %d, diag:%d vs %d).\n",
               idx_full-1, npar_full, idx_diag-1, npar_diag);
        exit(1);
    }
}

/*****************************************************************************/
/*  print_parameters: prints estimated parameters and standard deviations   */
/*  (Unchanged, but comments in English)                                    */
/*****************************************************************************/

static void print_parameters(real *x, real *dev, real **cov, int npar, struct Tvarma *varma) {
    int i, j, k, idx = 1;
    int m = varma->m;
    real t_stat, p_val;
    char sig[5];



    fprintf(outputv, "\n\n");
    fprintf(outputv, "=============================================================\n");
    fprintf(outputv, "  Estimated Parameters and Standard Deviations               \n");
    fprintf(outputv, "=============================================================\n");
    fprintf(outputv, "\n%33s %12s %12s %8s %6s\n",
            "Parameter", "Estimate", "Std.Error", "t-stat", "p-val");
    fprintf(outputv, "%s\n",
            "--------------------------------------------------------------------");

    /* Means */
    if (global_include_mean) {
        for (i = 1; i <= m; i++) {
            if (isnan(dev[idx])) {      /* no standard errors (est_se_label) */
                fprintf(outputv, "mu[%d]               %12.6f\n", i, x[idx]);
                idx++;
                continue;
            }
            t_stat = x[idx] / dev[idx];
            p_val = 2.0 * (1.0 - normal_cdf(fabs(t_stat)));
            sig_code(p_val, sig);
            fprintf(outputv, "mu[%d]               %12.6f %12.6f %8.3f %6.4f %s\n",
                    i, x[idx], dev[idx], t_stat, p_val, sig);
            idx++;
        }
    }

    /* AR */
    for (k = 1; k <= global_p; k++) {
        for (i = 1; i <= m; i++) {
            for (j = 1; j <= m; j++) {
                if (global_diag_ar && i != j) {
                    fprintf(outputv, "phi[%d]_%d%d          (fixed 0.0)\n", k, i, j);
                } else {
                    if (isnan(dev[idx])) {  /* no standard errors */
                        fprintf(outputv, "phi[%d]_%d%d          %12.6f\n", k, i, j, x[idx]);
                        idx++;
                        continue;
                    }
                    t_stat = x[idx] / dev[idx];
                    p_val = 2.0 * (1.0 - normal_cdf(fabs(t_stat)));
                    sig_code(p_val, sig);
                    fprintf(outputv, "phi[%d]_%d%d          %12.6f %12.6f %8.3f %6.4f %s\n",
                            k, i, j, x[idx], dev[idx], t_stat, p_val, sig);
                    idx++;
                }
            }
        }
    }

    /* MA */
    for (k = 1; k <= global_q; k++) {
        for (i = 1; i <= m; i++) {
            for (j = 1; j <= m; j++) {
                if (global_diag_ma && i != j) {
                    fprintf(outputv, "theta[%d]_%d%d        (fixed 0.0)\n", k, i, j);
                } else {
                    if (isnan(dev[idx])) {  /* no standard errors */
                        fprintf(outputv, "theta[%d]_%d%d        %12.6f\n", k, i, j, x[idx]);
                        idx++;
                        continue;
                    }
                    t_stat = x[idx] / dev[idx];
                    p_val = 2.0 * (1.0 - normal_cdf(fabs(t_stat)));
                    sig_code(p_val, sig);
                    fprintf(outputv, "theta[%d]_%d%d        %12.6f %12.6f %8.3f %6.4f %s\n",
                            k, i, j, x[idx], dev[idx], t_stat, p_val, sig);
                    idx++;
                }
            }
        }
    }

    /* Covariance */
    if (global_diag_cov) {
        for (i = 1; i <= m; i++) {
            if (isnan(dev[idx])) {      /* held by fdhess: the flat direction */
                fprintf(outputv, "cov[%d,%d]           %12.6f %12s\n", i, i, x[idx],
                        est_se_how == EST_SE_FDHESS ? "(normalised)" : "");
                idx++;
                continue;
            }
            t_stat = x[idx] / dev[idx];
            p_val = 2.0 * (1.0 - normal_cdf(fabs(t_stat)));
            sig_code(p_val, sig);
            fprintf(outputv, "cov[%d,%d]           %12.6f %12.6f %8.3f %6.4f %s\n",
                    i, i, x[idx], dev[idx], t_stat, p_val, sig);
            idx++;
        }
    } else {
        for (i = 1; i <= m; i++) {
            for (j = 1; j <= i; j++) {
                if (isnan(dev[idx])) {  /* held by fdhess: the flat direction */
                    fprintf(outputv, "cov[%d,%d]           %12.6f %12s\n", i, j, x[idx],
                            est_se_how == EST_SE_FDHESS ? "(normalised)" : "");
                    idx++;
                    continue;
                }
                t_stat = x[idx] / dev[idx];
                p_val = 2.0 * (1.0 - normal_cdf(fabs(t_stat)));
                sig_code(p_val, sig);
                fprintf(outputv, "cov[%d,%d]           %12.6f %12.6f %8.3f %6.4f %s\n",
                        i, j, x[idx], dev[idx], t_stat, p_val, sig);
                idx++;
            }
        }
    }
    fprintf(outputv, "%s\n",
            "--------------------------------------------------------------------");
    fprintf(outputv, "Signif. codes:  0 '***' 0.001 '**' 0.01 '*' 0.05 '.' 0.1 ' ' 1\n");
    fprintf(outputv, "Standard errors: %s\n\n", est_se_label(est_se_how));
}

/*****************************************************************************/
/*  print_matrices: prints the normalized model matrices                    */
/*  (Unchanged, but comments in English)                                    */
/*****************************************************************************/
void print_matrices(struct Tvarma *varma)
{
    int i, j, k;
    int m = varma->m;
    fprintf(outputv, "\nNormalized model:\n");
    fprintf(outputv, "mu vector:\n");
    for (i = 1; i <= m; i++)
        fprintf(outputv, "  %12.6f\n", varma->mu[i]);

    for (k = 1; k <= varma->p; k++) {
        fprintf(outputv, "phi(%d) matrix:\n", k);
        for (i = 1; i <= m; i++) {
            for (j = 1; j <= m; j++)
                fprintf(outputv, "  %12.6f", varma->phi[k][i][j]);
            fprintf(outputv, "\n");
        }
    }
    for (k = 1; k <= varma->q; k++) {
        fprintf(outputv, "theta(%d) matrix:\n", k);
        for (i = 1; i <= m; i++) {
            for (j = 1; j <= m; j++)
                fprintf(outputv, "  %12.6f", varma->theta[k][i][j]);
            fprintf(outputv, "\n");
        }
    }
    fprintf(outputv, "Q matrix:\n");
    for (i = 1; i <= m; i++) {
        for (j = 1; j <= i; j++)
            fprintf(outputv, "  %12.6f", varma->qq[i][j]);
        fprintf(outputv, "\n");
    }
    fprintf(outputv, "Sigma = sigma2 * Q:\n");
    for (i = 1; i <= m; i++) {
        for (j = 1; j <= i; j++)
            fprintf(outputv, "  %12.6f", varma->sigma2 * varma->qq[i][j]);
        fprintf(outputv, "\n");
    }
    fprintf(outputv, "\n");
}

/*****************************************************************************/
/*  print_roots: prints the inverse roots of AR and MA polynomials          */
/*  (Unchanged, but comments in English)                                    */
/*****************************************************************************/
void print_roots(struct Tvarma *varma)
{
    int i;
    real *wr, *wi, *wmod;
    int ifault = 0;

    if (varma->p > 0) {
        wr = vector(1, varma->m * varma->p);
        wi = vector(1, varma->m * varma->p);
        wmod = vector(1, varma->m * varma->p);
        chekma(varma->m, varma->p, varma->phi, wr, wi, wmod, &ifault);
        fprintf(outputv, "Inverse roots of |phi(B)|=0:\n");
        for (i = 1; i <= varma->m * varma->p; i++)
            if (fabs(wmod[i]) >= 0.00005)
                fprintf(outputv, "  %12.6f  %12.6f i  (modulus %12.6f)\n", wr[i], wi[i], wmod[i]);
        free_vector(wmod, 1, varma->m * varma->p);
        free_vector(wi, 1, varma->m * varma->p);
        free_vector(wr, 1, varma->m * varma->p);
    }

    if (varma->q > 0) {
        wr = vector(1, varma->m * varma->q);
        wi = vector(1, varma->m * varma->q);
        wmod = vector(1, varma->m * varma->q);
        chekma(varma->m, varma->q, varma->theta, wr, wi, wmod, &ifault);
        fprintf(outputv, "Inverse roots of |theta(B)|=0:\n");
        for (i = 1; i <= varma->m * varma->q; i++)
            if (fabs(wmod[i]) >= 0.00005)
                fprintf(outputv, "  %12.6f  %12.6f i  (modulus %12.6f)\n", wr[i], wi[i], wmod[i]);
        free_vector(wmod, 1, varma->m * varma->q);
        free_vector(wi, 1, varma->m * varma->q);
        free_vector(wr, 1, varma->m * varma->q);
    }
    fprintf(outputv, "\n");
}

/*****************************************************************************/
/*  hannan_rissanen_diag: initial values for diagonal model using           */
/*  Hannan-Rissanen method (for series with MA parts).                       */
/*  For q=0 falls back to OLS AR.                                            */
/*****************************************************************************/
static void hannan_rissanen_diag(real *x, int npar)
{
    int m = nser;
    int p = global_p;
    int q = global_q;
    int i, j, k, t, idx = 1;
    real *mean_est = NULL;

    /* Sample means (if included) */
    if (global_include_mean) {
        mean_est = vector(1, m);
        for (j = 1; j <= m; j++) {
            real sum = 0.0;
            for (t = 1; t <= nobs; t++) sum += datamat[t][j];
            mean_est[j] = sum / nobs;
            x[idx++] = mean_est[j];
        }
    }

    /* Center data */
    real **datac = matrix(1, nobs, 1, m);
    for (t = 1; t <= nobs; t++)
        for (j = 1; j <= m; j++)
            datac[t][j] = datamat[t][j] - (global_include_mean ? mean_est[j] : 0.0);

    /* Storage for diagonal coefficients */
    real **phi_diag   = matrix(1, p, 1, m);   /* phi_diag[k][i] */
    real **theta_diag = matrix(1, q, 1, m);   /* theta_diag[k][i] */

    for (i = 1; i <= m; i++) {
        if (q == 0) {
            /* Pure AR: use OLS as before */
            int T = nobs - p;
            if (T <= 0) {
                for (k = 1; k <= p; k++) phi_diag[k][i] = 0.0;
                continue;
            }
            real **Z = matrix(1, T, 1, p);
            real  *y = vector(1, T);
            for (t = p+1; t <= nobs; t++) {
                int row = t - p;
                for (k = 1; k <= p; k++)
                    Z[row][k] = datac[t - k][i];
                y[row] = datac[t][i];
            }
            real **ZtZ = matrix(1, p, 1, p);
            for (k = 1; k <= p; k++) {
                for (j = 1; j <= p; j++) {
                    real sum = 0.0;
                    for (t = 1; t <= T; t++) sum += Z[t][k] * Z[t][j];
                    ZtZ[k][j] = sum;
                }
            }
            int *indx = ivector(1, p);
            ludcp(ZtZ, p, indx);
            real *Zty = vector(1, p);
            for (k = 1; k <= p; k++) {
                Zty[k] = 0.0;
                for (t = 1; t <= T; t++) Zty[k] += Z[t][k] * y[t];
            }
            lusol(ZtZ, Zty, p, indx);
            for (k = 1; k <= p; k++) phi_diag[k][i] = Zty[k];
            for (k = 1; k <= q; k++) theta_diag[k][i] = 0.0;

            free_matrix(Z, 1, T, 1, p);
            free_vector(y, 1, T);
            free_matrix(ZtZ, 1, p, 1, p);
            free_ivector(indx, 1, p);
            free_vector(Zty, 1, p);
        }
        else {
            /* q > 0: Hannan-Rissanen */
            int maxpq = (p > q ? p : q);
            int L = (int) floor(sqrt((double) nobs));
            if (L < p+q) L = p+q;
            if (L + maxpq >= nobs) L = nobs - maxpq - 1;
            if (L < 1) L = 1;

            /* Step 1: fit AR(L) by OLS to obtain residuals e_t */
            int T_ar = nobs - L;
            if (T_ar <= L) {   /* Not enough data ? fallback to zeros */
                for (k = 1; k <= p; k++) phi_diag[k][i] = 0.0;
                for (k = 1; k <= q; k++) theta_diag[k][i] = 0.0;
                continue;
            }
            real **Z_ar = matrix(1, T_ar, 1, L);
            real  *y_ar = vector(1, T_ar);
            for (t = L+1; t <= nobs; t++) {
                int row = t - L;
                for (k = 1; k <= L; k++)
                    Z_ar[row][k] = datac[t - k][i];
                y_ar[row] = datac[t][i];
            }
            real **ZtZ_ar = matrix(1, L, 1, L);
            for (k = 1; k <= L; k++) {
                for (j = 1; j <= L; j++) {
                    real sum = 0.0;
                    for (t = 1; t <= T_ar; t++) sum += Z_ar[t][k] * Z_ar[t][j];
                    ZtZ_ar[k][j] = sum;
                }
            }
            int *indx_ar = ivector(1, L);
            ludcp(ZtZ_ar, L, indx_ar);
            real *Zty_ar = vector(1, L);
            for (k = 1; k <= L; k++) {
                Zty_ar[k] = 0.0;
                for (t = 1; t <= T_ar; t++) Zty_ar[k] += Z_ar[t][k] * y_ar[t];
            }
            lusol(ZtZ_ar, Zty_ar, L, indx_ar);
            real *ar_coef = vector(1, L);
            for (k = 1; k <= L; k++) ar_coef[k] = Zty_ar[k];

            /* Compute residuals e_hat[t] for t = L+1 .. nobs */
            real *e_hat = vector(1, nobs);
            for (t = 1; t <= L; t++) e_hat[t] = 0.0;  /* pre-sample zeros */
            for (t = L+1; t <= nobs; t++) {
                real pred = 0.0;
                for (k = 1; k <= L; k++) pred += ar_coef[k] * datac[t - k][i];
                e_hat[t] = datac[t][i] - pred;
            }

            /* Step 2: regress y_t on y_{t-1}..y_{t-p} and e_{t-1}..e_{t-q} */
            int start = maxpq + 1;
            if (start < L+2) start = L+2;   /* need e_hat[t-1] available */
            if (start > nobs) {
                /* Not enough data ? fallback */
                for (k = 1; k <= p; k++) phi_diag[k][i] = 0.0;
                for (k = 1; k <= q; k++) theta_diag[k][i] = 0.0;
                free_vector(e_hat, 1, nobs);
                free_vector(ar_coef, 1, L);
                free_matrix(Z_ar, 1, T_ar, 1, L);
                free_vector(y_ar, 1, T_ar);
                free_matrix(ZtZ_ar, 1, L, 1, L);
                free_ivector(indx_ar, 1, L);
                free_vector(Zty_ar, 1, L);
                continue;
            }
            int T_reg = nobs - start + 1;
            real **X = matrix(1, T_reg, 1, p+q);
            real  *y_reg = vector(1, T_reg);
            for (t = start; t <= nobs; t++) {
                int row = t - start + 1;
                /* AR lags */
                for (k = 1; k <= p; k++) X[row][k] = datac[t - k][i];
                /* MA lags using e_hat */
                for (k = 1; k <= q; k++) X[row][p + k] = e_hat[t - k];
                y_reg[row] = datac[t][i];
            }
            /* X'X and X'y */
            real **XtX = matrix(1, p+q, 1, p+q);
            for (k = 1; k <= p+q; k++) {
                for (j = 1; j <= p+q; j++) {
                    real sum = 0.0;
                    for (t = 1; t <= T_reg; t++) sum += X[t][k] * X[t][j];
                    XtX[k][j] = sum;
                }
            }
            real *Xty = vector(1, p+q);
            for (k = 1; k <= p+q; k++) {
                Xty[k] = 0.0;
                for (t = 1; t <= T_reg; t++) Xty[k] += X[t][k] * y_reg[t];
            }
            int *indx_reg = ivector(1, p+q);
            ludcp(XtX, p+q, indx_reg);
            lusol(XtX, Xty, p+q, indx_reg);
            for (k = 1; k <= p; k++) phi_diag[k][i] = Xty[k];
            for (k = 1; k <= q; k++) theta_diag[k][i] = -Xty[p + k];

            /* Free memory for this series */
            free_ivector(indx_reg, 1, p+q);
            free_vector(Xty, 1, p+q);
            free_matrix(XtX, 1, p+q, 1, p+q);
            free_matrix(X, 1, T_reg, 1, p+q);
            free_vector(y_reg, 1, T_reg);
            free_vector(e_hat, 1, nobs);
            free_vector(ar_coef, 1, L);
            free_matrix(Z_ar, 1, T_ar, 1, L);
            free_vector(y_ar, 1, T_ar);
            free_matrix(ZtZ_ar, 1, L, 1, L);
            free_ivector(indx_ar, 1, L);
            free_vector(Zty_ar, 1, L);
        }
    } /* end loop over series */

    /* Compute residuals from the ARMA model for each series to estimate variance */
    real **resid = matrix(1, nobs, 1, m);
    for (i = 1; i <= m; i++) {
        int maxpq = (p > q ? p : q);
        /* Initialize pre-sample residuals to zero */
        for (t = 1; t <= maxpq; t++) resid[t][i] = 0.0;
        for (t = maxpq+1; t <= nobs; t++) {
            real pred = 0.0;
            for (k = 1; k <= p; k++) pred += phi_diag[k][i] * datac[t - k][i];
            for (k = 1; k <= q; k++) pred += theta_diag[k][i] * resid[t - k][i];
            resid[t][i] = datac[t][i] - pred;
        }
    }

    /* Compute variances from t = max(p,q)+1 .. nobs */
    real *var_resid = vector(1, m);
    for (i = 1; i <= m; i++) {
        int maxpq = (p > q ? p : q);
        real sum = 0.0;
        int cnt = 0;
        for (t = maxpq+1; t <= nobs; t++) {
            sum += resid[t][i] * resid[t][i];
            cnt++;
        }
        var_resid[i] = (cnt > 0) ? sum / cnt : 1.0;
    }

    /* Scale variances to have average 1 (as in original) */
    real scale_factor = 0.0;
    for (i = 1; i <= m; i++) scale_factor += var_resid[i];
    scale_factor /= m;
    if (scale_factor < 1e-12) scale_factor = 1.0;
    for (i = 1; i <= m; i++) var_resid[i] /= scale_factor;

    /* Store parameters in x[] in the correct order */
    /* AR */
    for (k = 1; k <= p; k++)
        for (i = 1; i <= m; i++)
            x[idx++] = phi_diag[k][i];
    /* MA */
    for (k = 1; k <= q; k++)
        for (i = 1; i <= m; i++)
            x[idx++] = theta_diag[k][i];
    /* Covariance (diagonal) */
    for (i = 1; i <= m; i++)
        x[idx++] = var_resid[i];

    /* Free remaining memory */
    free_vector(var_resid, 1, m);
    free_matrix(resid, 1, nobs, 1, m);
    free_matrix(phi_diag, 1, p, 1, m);
    free_matrix(theta_diag, 1, q, 1, m);
    free_matrix(datac, 1, nobs, 1, m);
    if (global_include_mean) free_vector(mean_est, 1, m);
}

/* Standard normal CDF approximation (Abramowitz & Stegun)
static real normal_cdf(real x) {
    real t, p;
    if (x < 0) return 1.0 - normal_cdf(-x);
    t = 1.0 / (1.0 + 0.2316419 * x);
    p = 1.0 - 0.3989423 * exp(-x * x / 2.0) * t *
        (0.3193815 + t * (-0.3565638 + t * (1.781478 + t * (-1.821256 + t * 1.330274))));
    return p;
}*/

static void sig_code(real p, char *sig) {
    if (p < 0.001) strcpy(sig, "***");
    else if (p < 0.01) strcpy(sig, "** ");
    else if (p < 0.05) strcpy(sig, "*  ");
    else if (p < 0.1) strcpy(sig, ".  ");
    else strcpy(sig, "   ");
}


static void debug_print_vector(const char *label, real *v, int n)
{
    printf("\n--- %s ---\n", label);
    for (int i = 1; i <= n; i++) {
        printf("  x[%d] = %g\n", i, v[i]);
        if (i % 10 == 0 && i < n) printf("\n"); // salto cada 10 elementos
    }
    printf("--- fin %s ---\n", label);
}
