/*****************************************************************************/
/*  inpread.c -- part of drvarma (multivariate VARMA modelling).
 *
 *  Copyright (C) 1995-2026 A.B. Treadway, J.A. Mauricio & D.E. Guerrero.
 *
 *  This program is free software: you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.  Distributed WITHOUT ANY WARRANTY; see the
 *  GNU General Public License (file COPYING) for details.
 *****************************************************************************/

/* The reader of the multivariate .inp (deprecated since 5.0; see inpread.h).
   Moved verbatim out of main() in drvarma.c, so that -split reads the file
   with the same code as the .inp path; the error messages are the same.   */

#include "inpread.h"
#include <string.h>

/*****************************************************************************/
/*  Tokenizer for the fue-style .inp format.                                 */
/*  Lines whose first non-blank character is '*' are comments/section        */
/*  markers and are skipped.  All other lines provide whitespace-delimited   */
/*  tokens consumed sequentially.                                            */
/*****************************************************************************/
typedef struct { FILE *fp; char line[8192]; char *pos; } InpReader;

static int inp_token(InpReader *R, char *out, int n)
{
    for (;;) {
        if (R->pos == NULL || *R->pos == '\0') {
            if (!fgets(R->line, sizeof(R->line), R->fp)) return 0;  /* EOF */
            char *p = R->line;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '*' || *p == '\0' || *p == '\n' || *p == '\r') {
                R->pos = NULL;          /* comment / blank line: skip */
                continue;
            }
            R->pos = R->line;
        }
        while (*R->pos == ' ' || *R->pos == '\t' ||
               *R->pos == '\n' || *R->pos == '\r') R->pos++;
        if (*R->pos == '\0') { R->pos = NULL; continue; }
        int k = 0;
        while (*R->pos && *R->pos != ' ' && *R->pos != '\t' &&
               *R->pos != '\n' && *R->pos != '\r') {
            if (k < n - 1) out[k++] = *R->pos;
            R->pos++;
        }
        out[k] = '\0';
        return 1;
    }
}

/* Read one integer / real token, aborting with a message on failure. */
static int inp_int(InpReader *R, const char *what, const char *fname)
{
    char tok[256];
    if (!inp_token(R, tok, sizeof tok)) {
        printf("ERROR: %s: unexpected end of file while reading %s\n", fname, what);
        exit(1);
    }
    return atoi(tok);
}
static real inp_real(InpReader *R, const char *what, const char *fname)
{
    char tok[256];
    if (!inp_token(R, tok, sizeof tok)) {
        printf("ERROR: %s: unexpected end of file while reading %s\n", fname, what);
        exit(1);
    }
    return atof(tok);
}

void mvinp_read(const char *inputf, MvInp *in)
{
    FILE *inputv;
    int   i, j;

    if (NULL == (inputv = fopen(inputf, "r"))) {
        printf("ERROR: cannot open %s\n", inputf);
        exit(1);
    }
    if (NULL == (inputv = fopen(inputf, "r"))) {
        printf("ERROR: cannot open %s\n", inputf);
        exit(1);
    }
    {
        InpReader R; R.fp = inputv; R.line[0] = '\0'; R.pos = NULL;

        /* ** Frequency: */
        in->freq = inp_int(&R, "frequency", inputf);
        /* ** Series, observations, start (subperiod year): */
        in->nser        = inp_int(&R, "number of series", inputf);
        in->nobs        = inp_int(&R, "number of observations", inputf);
        in->start_sub   = inp_int(&R, "starting subperiod", inputf);
        in->start_year  = inp_int(&R, "starting year", inputf);

        if (in->freq < 1 || in->nser < 1 || in->nobs < 1) {
            printf("ERROR: invalid header in %s (freq=%d, nser=%d, nobs=%d)\n",
                   inputf, in->freq, in->nser, in->nobs);
            fclose(inputv);
            exit(1);
        }

        /* ** Series names: */
        in->names = (char **) malloc((in->nser + 1) * sizeof(char *));
        for (j = 1; j <= in->nser; j++) {
            char tok[256];
            if (!inp_token(&R, tok, sizeof tok)) {
                printf("ERROR: %s: missing series name %d of %d\n", inputf, j, in->nser);
                fclose(inputv);
                exit(1);
            }
            in->names[j] = strdup(tok);
        }

        /* ** Box-Cox lambda, regular differences, annual differences: */
        in->lambda = inp_real(&R, "Box-Cox lambda", inputf);
        in->d      = inp_int (&R, "regular differences", inputf);
        in->D      = inp_int (&R, "seasonal differences", inputf);
        if (in->d < 0 || in->D < 0) {
            printf("ERROR: %s: differences must be >= 0 (d=%d, D=%d)\n",
                   inputf, in->d, in->D);
            fclose(inputv);
            exit(1);
        }

        /* ** Data: nobs_raw rows of nser raw level values */
        real **raw = matrix(1, in->nobs, 1, in->nser);
        for (i = 1; i <= in->nobs; i++) {
            for (j = 1; j <= in->nser; j++) {
                char tok[256];
                if (!inp_token(&R, tok, sizeof tok)) {
                    printf("ERROR: not enough data in %s (failed at obs %d, series %d; "
                           "expected %d x %d values)\n",
                           inputf, i, j, in->nobs, in->nser);
                    free_matrix(raw, 1, in->nobs, 1, in->nser);
                    fclose(inputv);
                    exit(1);
                }
                raw[i][j] = atof(tok);
            }
        }
        fclose(inputv);
        in->raw = raw;
    }
}
