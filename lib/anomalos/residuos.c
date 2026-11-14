/*
 * residuos.c -- ver residuos.h. Sacado de la ventana de anomalos de la madre
 * (an_anomalos.c) para que atsw-cli anomalos calcule lo mismo.
 */
#include <math.h>
#include <string.h>

#include "residuos.h"

#include "lags.h"                /* lib/fugplot: la regla del motor */

/* LA FRECUENCIA, DE LAS FECHAS ANTES QUE DEL MODELO. El s del .out es el
   periodo estacional del MODELO, y un mensual sin parte estacional puede
   traerlo a 1; las fechas, en cambio, son las de la serie.              */
static int frecuencia( const FueOut *o )
{
    int i, maxp = 1, anno, per;

    if ( o->nres < 1 ) return 1;
    if ( strchr( o->res_fecha[0], '/' ) == NULL ) return 1;
    for ( i = 0; i < o->nres; i++ )
        {
        if ( fo_fecha_parte( o->res_fecha[i], &per, &anno ) != 0 ) { per = 1; anno = 0; }
        if ( per > maxp ) maxp = per;
        }
    if ( o->s > 1 ) return o->s;
    if ( maxp > 4 ) return 12;
    if ( maxp > 1 ) return 4;
    return 12;                     /* con barra y un solo periodo: mensual */
}

int an_residuos( const FueOut *o, AnResiduos *r )
{
    double sd = 0.0;
    int    i;

    memset( r, 0, sizeof *r );
    if ( o == NULL || o->nres < 1 ) return 1;
    r->n = o->nres;
    /* LOS EXTREMOS, EN EPISODIOS. Un suceso de tres periodos no son tres
       atipicos sueltos con su forma decidida por adyacencia.          */
    r->umbral = an_umbral( o->nres );
    for ( i = 0; i < o->nres; i++ ) sd += o->res[i] * o->res[i];
    sd = sqrt( sd / o->nres );
    for ( i = 0; i < o->nres; i++ ) r->z[i] = ( sd > 0.0 ) ? o->res[i] / sd : 0.0;
    /* SE PUNTUA EL TRAMO, NO LOS PUNTOS (anomalos.h). */
    r->nep = an_episodios( r->z, o->nres, r->umbral, r->ep, AN_MAX_EP );

    r->freq = frecuencia( o );
    r->lags = fp_lags( o->nres, r->freq );
    if ( r->lags > AN_MAX_LAG ) r->lags = AN_MAX_LAG;
    if ( r->lags > o->nres - 2 ) r->lags = o->nres - 2;
    return 0;
}

int an_omite( const AnResiduos *r, const int *marcado, int *omit, int max )
{
    int i, t, n = 0;

    for ( i = 0; i < r->nep; i++ )
        {
        if ( marcado && !marcado[i] ) continue;
        for ( t = r->ep[i].desde; t <= r->ep[i].hasta; t++ )
            if ( n < max ) omit[n++] = t;
        }
    return n;
}
