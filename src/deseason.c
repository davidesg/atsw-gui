/*****************************************************************************/
/*  deseason.c -- part of drvarma (multivariate VARMA modelling).
 *
 *  Copyright (C) 1995-2026 A.B. Treadway, J.A. Mauricio & D.E. Guerrero.
 *
 *  This program is free software: you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.  Distributed WITHOUT ANY WARRANTY; see the
 *  GNU General Public License (file COPYING) for details.
 *****************************************************************************/

/***************************************************************************/
/*  DESEASON.C                                                            */
/*  CLI harmonic seasonal adjustment, faithful to the GUI's proven path   */
/*  (preprocess_series_levels / get_seasonal_dummies in drvarma_gui.c):   */
/*    - estimate the seasonal harmonics on the first differences (d=1),   */
/*    - recover the LEVEL seasonal dummies via the A0 transform,          */
/*    - subtract them from the level series.                              */
/*  Reuses the tested routines declared in seasonal_detection.h.          */
/***************************************************************************/

#include "deseason.h"
#include "seasonal_detection.h"

/* seasonal_detection.c contains detect_seasonality_harmonic_regression(), a
   file-based entry point that calls load_data() (defined in the GUI).  The CLI
   never uses that path, but the symbol must resolve at link time: provide a
   stub.  It is never executed by the engine. */
int load_data(const char *filename, double **data, int *n)
{
    (void) filename; (void) data; (void) n;
    return 0;
}

void deseasonalize_raw(real **raw, int nobs, int m, int s, int start_sub,
                       int mode, real **dummies, FILE *logf)
{
    int d = 1;                       /* estimation differencing (stationary) */
    int num_harm = s - 1;
    int n_diff = nobs - d;

    for (int j = 1; j <= m; j++) {
        for (int p = 0; p < s; p++) dummies[j][p] = 0.0;

        if (n_diff <= num_harm + 1) {
            if (logf) fprintf(logf, "  Series %d: insufficient data for seasonal test "
                                    "(n_diff=%d, params=%d), kept as is\n",
                              j, n_diff, num_harm + 1);
            continue;
        }

        /* First differences of series j (0-based, length n_diff). */
        double *diff = (double *) malloc((size_t) n_diff * sizeof(double));
        for (int i = 0; i < n_diff; i++)
            diff[i] = (double) (raw[i + 2][j] - raw[i + 1][j]);

        /* Harmonic regression on the differenced basis (tested routine). */
        double *coeffs = (double *) malloc(num_harm * sizeof(double));
        double *serr   = (double *) malloc(num_harm * sizeof(double));
        gsl_matrix *hac = NULL, *ols = NULL;
        double intercept, intercept_se, f_stat, p_value, r_squared;

        int ok = harmonic_regression_differenced_basis(
                     diff, n_diff, d, coeffs, serr, &hac, &ols,
                     &intercept, &intercept_se, &f_stat, &p_value, &r_squared, s);

        int is_seasonal = 0, do_des = 0;
        if (ok) {
            double f_crit = f_distribution_critical_value(num_harm,
                                                          n_diff - num_harm - 1, 0.05);
            is_seasonal = (f_stat > f_crit);
            do_des = (mode == 1) ? 1 : is_seasonal;   /* force vs auto */
        }

        if (logf)
            fprintf(logf, "  Series %d: F=%.3f, p=%.4f, R2=%.3f -> %s%s\n",
                    j, f_stat, p_value, r_squared,
                    is_seasonal ? "SEASONAL" : "NOT SEASONAL",
                    do_des ? " (adjusted)" : " (kept as is)");

        if (ok && do_des) {
            /* ------------------------------------------------------------------
             * BUG (CRITICAL, NOT fixed here) — wrong phase when start_sub != 1.
             * `harmonic_regression_differenced_basis` above takes no start_sub:
             * its design matrix is built from t = i + d + 1, so `level` below is
             * indexed by OFFSET FROM THE START of the series. It is then applied
             * in ABSOLUTE subperiod phase, `(i + start_sub - 1) % s`. The two
             * agree only when start_sub == 1; otherwise the pattern is subtracted
             * shifted by start_sub-1 months and the adjustment ADDS seasonal
             * variance instead of removing it. Entirely silent.
             * See BUGS.md for root cause, reproduction and the one-loop rotation
             * that fixes it. Already fixed in the Python port (deseason.py).
             * ------------------------------------------------------------------ */
            /* Level seasonal dummies via the A0 transform (sum-to-zero). */
            double *level = transform_harmonics_to_dummies_general(coeffs, NULL, s);
            for (int p = 0; p < s; p++) dummies[j][p] = level[p];
            for (int i = 0; i < nobs; i++) {
                int period = (i + start_sub - 1) % s;
                raw[i + 1][j] -= level[period];
            }
            free(level);
        }

        if (hac) gsl_matrix_free(hac);
        if (ols) gsl_matrix_free(ols);
        free(serr);
        free(coeffs);
        free(diff);
    }
}
