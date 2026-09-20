/*
 * plotshim.c -- las tres funciones que lib/fugplot pide y drtran no tenia.
 *
 * Ver gui/drtran/include/plothost.h para el porque. No hay calculo nuevo:
 * fp_acf_ser es la Acf del motor con la firma que fugplot espera, y las otras
 * dos son las de fue, copiadas porque son dos lineas cada una y arrastrar
 * diagnose.c de otro motor para obtenerlas seria mucho peor.
 */

#include <math.h>
#include "plothost.h"

/* La ACF de fugplot. El calculo es el de diagnose.c del motor --se comprueba
 * linea por linea-- y lo unico que cambia es que la media y la varianza van
 * dentro de la serie en vez de en la llamada.                            */
void fp_acf_ser( struct Tseries *ser, int lags, real *corr )
{
#undef Acf
    Acf( ser->data, ser->nobs, lags, corr, ser->mean, ser->var );
}

real JarqueBera( real skew, real kurt, int nobs )
{
    return nobs / 6.0 * ( skew * skew + kurt * kurt / 4.0 );
}

int iround( double num )
{
    return (int) ( num < 0.0 ? num - 0.5 : num + 0.5 );
}
