/*
 * genera.c -- el .inp de una serie, GENERADO del .csv.
 *
 * EL DATO ES EL .csv; LOS .inp SE DERIVAN DE EL. No hay dos dueños: hay un
 * dueño y una derivacion, igual que .pre -> .inp.
 *
 * Antes los numeros solo existian dentro de m00.inp, en el formato del motor,
 * y eso costaba tres cosas: nadie mas los podia leer, no se podia alargar la
 * muestra sin reimportar, y --la que bloquea de verdad-- no se podia generar
 * el .inp de una ventana, porque para eso hacen falta los numeros de esa
 * ventana.
 *
 * LA VENTANA ES UN RECORTE, NO UNA AMPUTACION. El .csv guarda siempre la
 * muestra total; lo que se recorta es el .inp que se genera. Si se recortara
 * el .csv se perderia la unica copia de lo que habia.
 *
 * Y NO PONE MODELO: el .inp que sale de aqui es la serie y nada mas, que es
 * lo que fug identifica y de lo que el analista parte. La especificacion la
 * escribe encima quien la decida.
 */

#include <string.h>

#include "datos.h"
#include "inpfile.h"

#include "atsw.h"


/* Cuantas observaciones caben hasta <hasta> inclusive, contando desde el
 * comienzo declarado. "" = hasta el final.
 *
 * Se cuenta POR FECHAS y no por posicion porque una ventana se dice en
 * fechas --"hasta 12/2019"-- y traducirla a un numero de observaciones aqui
 * es lo unico que hay que hacer bien. Si la fecha no cae en la serie, se
 * corta en la ultima que si.                                           */
int atsw_hasta_n( int freq, int anio, int per, int nobs, const char *hasta )
{
    int p, y, i;

    if ( hasta == NULL || !*hasta ) return nobs;
    if ( freq <= 0 || anio <= 0 )   return nobs;   /* sin fechas no hay corte */

    if ( freq == 1 )
        { if ( sscanf( hasta, "%d", &y ) != 1 ) return nobs; p = 1; }
    else if ( sscanf( hasta, "%d/%d", &p, &y ) != 2 )
        return nobs;

    if ( per <= 0 ) per = 1;
    /* periodos entre el comienzo y <hasta>, ambos inclusive */
    i = ( y - anio ) * freq + ( p - per ) + 1;

    if ( i < 1 )    return 0;      /* la ventana acaba antes de empezar */
    if ( i > nobs ) return nobs;   /* y si se pasa, es que hay menos    */
    return i;
}


/* Genera el .inp de <serie> en <destino>, con los datos de <csv> recortados
 * hasta <hasta> ("" = todo). 0 si pudo.                                */
int atsw_genera_inp( const char *csv, const char *destino, const char *serie,
                     const char *hasta, char *why, size_t n )
{
    DtDatos *d;
    DtError  e;
    InpFile  inp;
    int      i, nobs, rc;

    if ( why && n ) why[0] = '\0';
    if ( csv == NULL || destino == NULL ) return 1;

    d = g_new0( DtDatos, 1 );
    if ( dt_leer( csv, d, &e ) != 0 )
        { if ( why ) dt_error_es( &e, why, n ); g_free( d ); return 1; }

    if ( d->ncol < 1 || d->nobs < 1 )
        { if ( why ) g_snprintf( why, n, "%s no tiene datos.", csv );
          g_free( d ); return 1; }

    nobs = atsw_hasta_n( d->freq, d->anio, d->per, d->nobs, hasta );
    if ( nobs < 1 )
        {
        /* SE DICE QUE LA VENTANA NO PILLA NADA. Un .inp de cero
           observaciones lo rechazaria el motor mucho mas lejos de aqui. */
        if ( why ) g_snprintf( why, n, "La muestra que pides acaba antes de "
                               "que empiece «%s».", serie );
        g_free( d ); return 1;
        }

    memset( &inp, 0, sizeof inp );
    inp.fue      = 1;
    inp.model    = 0;          /* SIN MODELO: identificar es antes        */
    inp.freq     = d->freq > 0 ? d->freq : 1;
    inp.nobs     = nobs;
    inp.begtime  = d->per  > 0 ? d->per  : 1;
    inp.begyear  = d->anio > 0 ? d->anio : 1;
    inp.boxlam   = 1.0;        /* sin transformar: la decide el analista  */
    inp.refactor = 1.0;
    snprintf( inp.name, sizeof inp.name, "%s", serie ? serie : "" );

    inp.data = g_new( double, nobs );
    for ( i = 0; i < nobs; i++ ) inp.data[i] = d->v[0][i];

    rc = inp_write_bare( destino, &inp );
    g_free( inp.data );
    g_free( d );

    if ( rc != 0 )
        { if ( why ) g_snprintf( why, n, "No pude escribir %s.", destino );
          return 1; }
    return 0;
}

/* <raiz>/<serie>/datos.csv -- FUERA de work/.
 *
 * work/ es donde se corre: se llena de .out, .eps y .tex, y es lo que se
 * limpia. El dato no puede vivir en lo que se limpia.                  */
int atsw_csv_de( const Proyecto *p, const char *serie, char *out, size_t n )
{
    char dir[PR_RUTA];

    if ( out == NULL || n == 0 ) return 1;
    out[0] = '\0';
    if ( pr_ruta( p, serie, NULL, NULL, dir, sizeof dir ) != 0 ) return 1;
    g_snprintf( out, n, "%s/datos.csv", dir );
    return 0;
}
