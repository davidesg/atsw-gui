/*****************************************************************************/
/*  split.c -- part of drvarma (multivariate VARMA modelling).
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
/*  drvarma -split FILE[.inp] [-mean] [-harmonics] [-ar P] [-ma Q]           */
/*                            [-scale F] [-dir DIR]                          */
/*                                                                           */
/*  The migration out of the multivariate .inp (deprecated since 5.0): one   */
/*  univariate .inp of fue per series, which is the format the rest of the   */
/*  ecosystem reads and writes, and the input of the ladder.                 */
/*                                                                           */
/*  What the multivariate file says goes into every one of them: frequency,  */
/*  dates, the series, Box-Cox lambda, d and D. What was a run option of the */
/*  .inp path becomes part of each specification:                            */
/*    -mean       an estimated mean              (the .inp path's -mean)     */
/*    -scale F    the rescaling factor, 100 by default (the .inp path's too) */
/*    -harmonics  the seasonal harmonics as deterministic terms, estimated   */
/*                (what -deseason computed outside the model)                */
/*    -ar P -ma Q a free regular AR(P) / MA(Q): with them and cross orders   */
/*                p = P, q = Q, the ladder is the .inp path's full VARMA(P,Q)*/
/*                                                                           */
/*  The files are SPECIFICATIONS, not optima: the values are seeds. They are */
/*  meant for fue or art, where the univariate model is built; the ladder    */
/*  takes them as they are too (with -redet if they carry harmonics).        */
/*****************************************************************************/

#include "main.h"
#include "inpread.h"
#include "version.h"
#include <string.h>

static void usage(void)
{
    printf("Usage: drvarma -split FILE[.inp] [-mean] [-harmonics] [-ar P] [-ma Q]\n"
           "                        [-scale F] [-dir DIR]\n"
           "  Converts a multivariate drvarma .inp (deprecated since 5.0) into one\n"
           "  univariate .inp of fue per series, DIR/<series name>.inp.\n"
           "  -mean       estimated mean               -scale F  rescaling (100)\n"
           "  -harmonics  seasonal harmonics, estimated -ar P -ma Q free regular ARMA\n"
           "  Existing files are never overwritten.\n");
}

static void write_coefs(FILE *f, const char *title, int order)
{
    int k;
    fprintf(f, "** %s:\n", title);
    if (order <= 0) { fprintf(f, "0\n"); return; }
    fprintf(f, "1 %d\n**\n", order);
    for (k = 1; k <= order; k++) fprintf(f, "0.000000  1\n");
}

static int write_one(const MvInp *in, int j, const char *path, const char *src,
                     int mean, int harm, int ar, int ma, real scale)
{
    FILE *f;
    int   i, k, nd = 0, s = in->freq;
    char  spec[64][16];

    if ((f = fopen(path, "r")) != NULL) {
        fclose(f);
        printf("ERROR: %s exists; it is not overwritten\n", path);
        return 1;
    }
    if (harm && s > 1) {
        for (k = 1; 2 * k < s; k++) {
            snprintf(spec[nd++], sizeof spec[0], "cos %d", k);
            snprintf(spec[nd++], sizeof spec[0], "sin %d", k);
        }
        if (s % 2 == 0) snprintf(spec[nd++], sizeof spec[0], "alter");
    }
    if ((f = fopen(path, "w")) == NULL) {
        printf("ERROR: cannot create %s\n", path);
        return 1;
    }
    fprintf(f, "************************************************\n");
    fprintf(f, "* Input file for program FUE                   *\n");
    fprintf(f, "************************************************\n");
    fprintf(f, "* Split from %s by drvarma %s\n\n", src, DRVARMA_VERSION_FULL);
    fprintf(f, "** Frequency of time series: either 1(A), 4(Q) or 12(M):\n %d\n", s);
    fprintf(f, "** Number of observations and starting date of time series:\n");
    fprintf(f, " %d  %d %d %s\n", in->nobs, s > 1 ? in->start_sub : 1, in->start_year,
            in->names[j]);
    fprintf(f, "** Number of deterministic variables (including seasonal components):\n%d\n", nd);
    if (nd > 0) {
        fprintf(f, "**\n");
        for (k = 0; k < nd; k++) fprintf(f, "%s\n", spec[k]);
        fprintf(f, "**\n");
        for (k = 0; k < nd; k++) fprintf(f, "0%s", k + 1 < nd ? " " : "\n");
        for (k = 0; k < nd; k++) fprintf(f, "**\n0.000000  1\n");
        fprintf(f, "**\n");
        for (k = 0; k < nd; k++) fprintf(f, "0%s", k + 1 < nd ? " " : "\n");
    }
    write_coefs(f, "Number and orders of regular AR operators", ar);
    write_coefs(f, "Number and orders of annual AR operators", 0);
    write_coefs(f, "Number and orders of regular MA operators", ma);
    write_coefs(f, "Number and orders of anual MA operators", 0);
    fprintf(f, "** Number and frequencies of regular AR(2) operators with fixed frequency:\n0\n");
    fprintf(f, "** Number and frequencies of regular MA(2) operators with fixed frequency:\n0\n");
    fprintf(f, "** Mean parameter (mu):\n%s\n", mean ? "0.000000 1" : "0");
    fprintf(f, "** Box-Cox lambda, regular differences and complete annual differences:\n");
    fprintf(f, " %g  %d  %d\n", in->lambda, in->d, in->D);
    fprintf(f, "** Individual factors of the annual difference (starting at freq 0.0):\n");
    if (s > 1) {
        for (k = 0; k <= s / 2; k++) fprintf(f, "0%s", k < s / 2 ? " " : "\n");
    } else
        fprintf(f, " 0\n");
    fprintf(f, "** ACF/PACF bands (0 Automatic) and reescaling factor:\n 0 %g\n", scale);
    fprintf(f, "** Time series (stochastic and non-standard deterministic variables):\n");
    for (i = 1; i <= in->nobs; i++) fprintf(f, "%.15g\n", in->raw[i][j]);
    fclose(f);
    return 0;
}

int split_main(int argc, char *argv[])
{
    const char *dir = ".";
    char  src[600], path[900];
    int   mean = 0, harm = 0, ar = 0, ma = 0, i, j, rc = 0;
    real  scale = 100.0;
    MvInp in;
    FILE *f;

    if (argc < 3) { usage(); return 1; }
    for (i = 3; i < argc; i++) {
        if      (strcmp(argv[i], "-mean") == 0)      mean = 1;
        else if (strcmp(argv[i], "-harmonics") == 0) harm = 1;
        else if (strcmp(argv[i], "-ar") == 0 && i + 1 < argc)    ar = atoi(argv[++i]);
        else if (strcmp(argv[i], "-ma") == 0 && i + 1 < argc)    ma = atoi(argv[++i]);
        else if (strcmp(argv[i], "-scale") == 0 && i + 1 < argc) scale = atof(argv[++i]);
        else if (strcmp(argv[i], "-dir") == 0 && i + 1 < argc)   dir = argv[++i];
        else { printf("ERROR: unknown option %s\n", argv[i]); usage(); return 1; }
    }
    if (ar < 0 || ma < 0 || scale == 0.0) { usage(); return 1; }

    /* The name as given, or with .inp added, as the .inp path takes it. */
    snprintf(src, sizeof src, "%s", argv[2]);
    if ((f = fopen(src, "r")) == NULL) snprintf(src, sizeof src, "%s.inp", argv[2]);
    else fclose(f);

    mvinp_read(src, &in);
    for (j = 1; j <= in.nser; j++) {
        snprintf(path, sizeof path, "%s/%s.inp", dir, in.names[j]);
        if (write_one(&in, j, path, src, mean, harm, ar, ma, scale) != 0) { rc = 2; continue; }
        printf("%s\n", path);
    }
    free_matrix(in.raw, 1, in.nobs, 1, in.nser);
    for (j = 1; j <= in.nser; j++) free(in.names[j]);
    free(in.names);
    return rc;
}
