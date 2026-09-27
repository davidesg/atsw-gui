/*
 * inpread.h -- the multivariate .inp of drvarma (DEPRECATED since 5.0).
 *
 * One file, m columns: frequency; series, observations and start; the
 * series names; one Box-Cox lambda and one d, D for all; the data. It is a
 * second dialect called .inp next to fue's univariate one, which is what
 * the rest of the ecosystem reads and writes. drvarma 5.0 takes fue's files
 * (the ladder) and converts this one with `drvarma -split`; the .inp path
 * still reads it, unchanged, until the GUI is migrated.
 */
#ifndef DRVARMA_INPREAD_H
#define DRVARMA_INPREAD_H

#include "main.h"

typedef struct {
    int    freq, nser, nobs, start_sub, start_year;
    char **names;              /* 1..nser, strdup'ed                    */
    real   lambda;
    int    d, D;
    real **raw;                /* matrix(1, nobs, 1, nser): the levels  */
} MvInp;

/* Reads path. On any error it prints the reason and exits(1): the .inp
   path always behaved like this, and its messages are kept verbatim.   */
void mvinp_read(const char *path, MvInp *in);

/* drvarma -split: one univariate .inp of fue per series (split.c). */
int split_main(int argc, char *argv[]);

#endif
