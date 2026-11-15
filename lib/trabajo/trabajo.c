/* trabajo.c -- un trabajo aparte, y lo que se enseña de el. Ver trabajo.h. */

#include "trabajo.h"
#include "sitio.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <glib.h>
#include <glib/gstdio.h>

int trabajo_escribe( const char *ruta, const Trabajo *t )
{
    gchar *txt = g_strdup_printf( "que %s\nobjeto %s\npid %ld\ninicio %ld\npor %s\ntotal %d\n"
                                  "progreso %s\nresultado %s\n", t->que, t->objeto, t->pid, t->inicio,
                                  t->por[0] ? t->por : "analista", t->total, t->progreso, t->resultado );
    gboolean ok = g_file_set_contents( ruta, txt, -1, NULL );   /* aparte y renombra */

    g_free( txt );
    return ok ? 0 : 1;
}

int trabajo_lee( const char *ruta, Trabajo *t )
{
    gchar  *txt = NULL, **l;
    int     i, hay = 0;

    memset( t, 0, sizeof *t );
    if ( !g_file_get_contents( ruta, &txt, NULL, NULL ) ) return 1;
    l = g_strsplit( txt, "\n", -1 );
    for ( i = 0; l[i]; i++ )
        {
        char *v = strchr( l[i], ' ' );

        if ( !v ) continue;
        *v++ = '\0';
        g_strchomp( v );
        if ( !strcmp( l[i], "que" ) )            g_strlcpy( t->que, v, sizeof t->que );
        else if ( !strcmp( l[i], "objeto" ) )    g_strlcpy( t->objeto, v, sizeof t->objeto );
        else if ( !strcmp( l[i], "pid" ) )       { t->pid = atol( v ); hay |= 1; }
        else if ( !strcmp( l[i], "inicio" ) )    { t->inicio = atol( v ); hay |= 2; }
        else if ( !strcmp( l[i], "por" ) )       g_strlcpy( t->por, v, sizeof t->por );
        else if ( !strcmp( l[i], "total" ) )     t->total = atoi( v );
        else if ( !strcmp( l[i], "progreso" ) )  g_strlcpy( t->progreso, v, sizeof t->progreso );
        else if ( !strcmp( l[i], "resultado" ) ) g_strlcpy( t->resultado, v, sizeof t->resultado );
        }
    g_strfreev( l );
    g_free( txt );
    return hay == 3 ? 0 : 1;
}

static gchar *junto( const char *ruta, const char *rel )
{
    gchar *dir, *r;

    if ( !rel || !rel[0] ) return NULL;
    if ( g_path_is_absolute( rel ) ) return g_strdup( rel );
    dir = g_path_get_dirname( ruta );
    r = g_build_filename( dir, rel, NULL );
    g_free( dir );
    return r;
}

int trabajo_hechos( const char *ruta, const Trabajo *t )
{
    gchar *p = junto( ruta, t->progreso );
    FILE  *f = p ? g_fopen( p, "r" ) : NULL;
    char   l[512];
    int    n = 0;

    g_free( p );
    if ( !f ) return -1;
    while ( fgets( l, sizeof l, f ) )
        if ( isdigit( (unsigned char) l[0] ) ) n++;
    fclose( f );
    return n;
}

TrEstado trabajo_estado( const char *ruta, const Trabajo *t )
{
    gchar   *r;
    GStatBuf st;
    TrEstado e;

    if ( sitio_vive( t->pid ) ) return TR_EN_MARCHA;
    r = junto( ruta, t->resultado );
    e = ( r && g_stat( r, &st ) == 0 && (long) st.st_mtime >= t->inicio ) ? TR_TERMINADO : TR_CAIDO;
    g_free( r );
    return e;
}

char *trabajo_linea( const char *ruta, const Trabajo *t, long ahora, char *out, size_t n, double *fraccion )
{
    TrEstado e = trabajo_estado( ruta, t );
    int      h = trabajo_hechos( ruta, t );
    long     ya = ahora - t->inicio;

    if ( fraccion ) *fraccion = ( t->total > 0 && h >= 0 ) ? (double) h / t->total : -1.0;
    if ( e == TR_TERMINADO )
        { g_snprintf( out, n, "%s · %s · terminado", t->objeto, t->que ); if ( fraccion ) *fraccion = 1.0; }
    else if ( e == TR_CAIDO )
        g_snprintf( out, n, "%s · %s · se paró sin terminar", t->objeto, t->que );
    else if ( h > 0 && t->total > 0 )
        {
        long q = (long) ( (double) ya / h * ( t->total - h ) );
        if ( q < 0 ) q = 0;
        if ( q >= 120 ) g_snprintf( out, n, "%s · %s · %d/%d · quedan ~%ld min · %s", t->objeto, t->que,
                                    h, t->total, ( q + 30 ) / 60, t->por );
        else g_snprintf( out, n, "%s · %s · %d/%d · quedan ~%ld s · %s", t->objeto, t->que, h, t->total, q, t->por );
        }
    else
        g_snprintf( out, n, "%s · %s · empezando (%ld s) · %s", t->objeto, t->que, ya, t->por );
    return out;
}

static int mira( const char *dir, char rutas[][TR_RUTA], int max, int n )
{
    GDir       *d = g_dir_open( dir, 0, NULL );
    const char *e;

    if ( !d ) return n;
    while ( n < max && ( e = g_dir_read_name( d ) ) )
        if ( g_str_has_suffix( e, ".trabajo" ) )
            {
            gchar *r = g_build_filename( dir, e, NULL );
            g_strlcpy( rutas[n++], r, TR_RUTA );
            g_free( r );
            }
    g_dir_close( d );
    return n;
}

int trabajo_lista( const char *raiz, char rutas[][TR_RUTA], int max )
{
    GDir       *d;
    const char *e;
    int         n = 0;
    gchar      *casos = g_build_filename( raiz, "_casos", NULL );

    if ( ( d = g_dir_open( casos, 0, NULL ) ) )
        {
        while ( ( e = g_dir_read_name( d ) ) )
            { gchar *w = g_build_filename( casos, e, "work", NULL ); n = mira( w, rutas, max, n ); g_free( w ); }
        g_dir_close( d );
        }
    g_free( casos );
    if ( ( d = g_dir_open( raiz, 0, NULL ) ) )
        {
        while ( ( e = g_dir_read_name( d ) ) )
            if ( e[0] != '_' && e[0] != '.' )
                { gchar *w = g_build_filename( raiz, e, "work", NULL ); n = mira( w, rutas, max, n ); g_free( w ); }
        g_dir_close( d );
        }
    return n;
}
