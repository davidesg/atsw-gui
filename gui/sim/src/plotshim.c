/*
 * plotshim.c -- las tres funciones que lib/fugplot pide al programa
 * (include/plothost.h). La ACF es la de drvarma (diagnose.c: Acf), escrita
 * aqui para no arrastrar el diagnose del motor, que vive de sus globales.
 */
#include <math.h>
#include <stdio.h>
#include "plothost.h"

void fp_acf_ser( struct Tseries *ser, int lags, real *corr )
{
    int i, j;
    for (j = 1; j <= lags; j++) {
        corr[j] = 0.0;
        for (i = 1; i <= ser->nobs - j; i++)
            corr[j] += ( ser->data[i] - ser->mean ) * ( ser->data[i + j] - ser->mean );
        corr[j] /= ser->nobs * ser->var;
    }
}

real JarqueBera( real skew, real kurt, int nobs )
{
    return nobs / 6.0 * ( skew * skew + kurt * kurt / 4.0 );
}

int iround( double num )
{
    return (int) ( num < 0.0 ? num - 0.5 : num + 0.5 );
}

/* LAS ESTADISTICAS DEL GRAFICO (Mean, Stdev, Skew, Kurt, Pacf, ChiTest) son
 * las del motor: se enlaza engines/drvarma/src/diagnose.c tal cual, como
 * drtran_gui enlaza el de drtran. Ese fichero declara los globales del
 * informe de drvarma; las funciones que usa fugplot no los tocan, y aqui se
 * definen vacios para que enlace (como macheps en series.c).           */
FILE  *outputv = NULL;
int    data_freq = 0, data_start_year = 0, data_start_sub = 0;
int    trans_d = 0, trans_D = 0;
char **series_names = NULL;
