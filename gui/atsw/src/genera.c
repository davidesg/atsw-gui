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
 * comienzo declarado.
 *
 *   ""   la muestra entera. Es una respuesta, no una falta.
 *   -1   LA FECHA NO SE ENTIENDE. Y esto es lo importante: antes devolvia
 *        nobs, asi que escribir "2019-12" en vez de "12/2019" daba LA
 *        MUESTRA ENTERA en silencio -- se declaraba una ventana hasta 2019
 *        y se estimaba sobre todo, sin que nada lo dijera. Tres cosas
 *        distintas --sin ventana, ilegible, fuera de rango-- estaban
 *        contestando lo mismo.
 *
 * Se cuenta POR FECHAS y no por posicion porque una ventana se dice en
 * fechas --"hasta 12/2019"-- y traducirla a un numero de observaciones aqui
 * es lo unico que hay que hacer bien. Una fecha que se pasa del final SI se
 * recorta: pedir mas de lo que hay es querer todo lo que hay.          */
int atsw_hasta_n( int freq, int anio, int per, int nobs, const char *hasta )
{
    int p = 1, y, i;
    char sobra;

    if ( hasta == NULL || !*hasta ) return nobs;
    if ( freq <= 0 || anio <= 0 )   return nobs;   /* sin fechas no hay corte */

    /* El %c de mas es para cazar la basura del final: sscanf da por bueno
       "12/2019xyz" si no se le pregunta si quedaba algo.               */
    if ( freq == 1 )
        { if ( sscanf( hasta, "%d %c", &y, &sobra ) != 1 ) return -1; }
    else if ( sscanf( hasta, "%d/%d %c", &p, &y, &sobra ) != 2 )
        return -1;

    if ( p < 1 || p > freq ) return -1;     /* "13/2019" no es un mes */
    if ( y < 1 )             return -1;

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
    if ( nobs < 0 )
        {
        /* NO SE ADIVINA. Antes esto era la muestra entera y nadie se
           enteraba de que la ventana no se habia aplicado.           */
        if ( why ) g_snprintf( why, n, "«%s» no es una fecha de este "
                               "proyecto. Se escriben como en los ficheros "
                               "del motor: %s.", hasta,
                               d->freq == 1 ? "«2019»" : "«12/2019»" );
        g_free( d ); return 1;
        }
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
    if ( pr_ruta( p, serie, "", NULL, NULL, dir, sizeof dir ) != 0 ) return 1;
    g_snprintf( out, n, "%s/datos.csv", dir );
    return 0;
}

/* ------------------------------------------------------------------------ */
/* EL TRAMO DEL PROYECTO: LA UNION DE LAS SERIES                             */
/*                                                                           */
/* Las muestras son DEL PROYECTO -- si fueran de cada serie, la fila de       */
/* pestañas bailaria al cambiar de serie -- pero el tramo de datos es DE CADA */
/* SERIE. Asi que "el principio y el final de la muestra completa" no es una  */
/* cosa sola, y lo que se ofrece es la UNION: la mas temprana y la mas        */
/* tardia. Una serie mas corta que la ventana da lo que tiene, que ya es la   */
/* regla declarada.                                                          */
/*                                                                           */
/* Y LA FRECUENCIA ES UNA SOLA. Un proyecto de frecuencias mezcladas no se    */
/* puede alimentar a drtran ni a drvarma sin sembrar bugs, asi que aqui no se */
/* arregla: se DETECTA y se dice. Si t->mezcla, el que llame sabe que no      */
/* puede ofrecer un selector de periodos porque no hay un rango que valga.    */
/* ------------------------------------------------------------------------ */

int atsw_tramo( const Proyecto *p, AtTramo *t )
{
    int i;

    if ( t == NULL ) return 1;
    memset( t, 0, sizeof *t );
    if ( p == NULL ) return 1;

    for ( i = 0; i < p->ns; i++ )
        {
        DtDatos *d;
        DtError  e;
        char     csv[PR_RUTA];
        int      fin;

        if ( atsw_csv_de( p, p->s[i].id, csv, sizeof csv ) != 0 ) continue;
        if ( !g_file_test( csv, G_FILE_TEST_EXISTS ) ) continue;

        d = g_new0( DtDatos, 1 );
        if ( dt_leer( csv, d, &e ) != 0 || d->nobs < 1 )
            { g_free( d ); continue; }

        if ( t->nseries == 0 )
            {
            t->freq = d->freq;
            t->anio = d->anio;
            t->per  = d->per > 0 ? d->per : 1;
            t->fin_anio = 0;
            }
        else if ( d->freq != t->freq )
            t->mezcla = TRUE;

        /* El comienzo mas TEMPRANO */
        if ( d->anio > 0 &&
             ( t->anio == 0 ||
               d->anio * 1000 + ( d->per > 0 ? d->per : 1 ) <
               t->anio * 1000 + t->per ) )
            { t->anio = d->anio; t->per = d->per > 0 ? d->per : 1; }

        /* y el final mas TARDIO */
        if ( d->freq > 0 && d->anio > 0 )
            {
            int k = ( ( d->per > 0 ? d->per : 1 ) - 1 ) + d->nobs - 1;

            fin = d->anio + k / d->freq;
            if ( fin * 1000 + ( k % d->freq ) + 1 >
                 t->fin_anio * 1000 + t->fin_per )
                { t->fin_anio = fin; t->fin_per = ( k % d->freq ) + 1; }
            }

        if ( d->nobs > t->nobs ) t->nobs = d->nobs;
        t->nseries++;
        g_free( d );
        }
    return t->nseries ? 0 : 1;
}
