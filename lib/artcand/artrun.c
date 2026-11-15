/*
 * artrun.c -- ver artrun.h. Es lo que hacia an_identifica.c (corre_art,
 * corre_art_una, escribe_datos), sin la ventana.
 */
#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "artrun.h"
#include "engine.h"

int ac_escribe_datos( const char *ruta, const double *x, int n,
                      int freq, int per, int anio )
{
    FILE *f = g_fopen( ruta, "w" );
    int   i;

    if ( !f ) return 1;
    fprintf( f, "# freq %d\n", freq > 0 ? freq : 1 );
    if ( anio > 0 ) fprintf( f, "# start %d/%d\n", per > 0 ? per : 1, anio );
    for ( i = 0; i < n; i++ ) fprintf( f, "%.10g\n", x[i] );
    fclose( f );
    return 0;
}

/* art una vez; 0 y c lleno, o 1 con el motivo. */
static int una( const char *exe, const char *datos, int freq, double lam, int d, int D,
                int residuos, int off, ArtCand *c, char *why, size_t n )
{
    gchar       *dir = g_path_get_dirname( datos );
    char         s[16], sd[16], sD[16], cand[1100];
    EngineResult r;
    int          line = 0, rc;

    g_snprintf( s, sizeof s, "%d", freq > 0 ? freq : 1 );
    g_snprintf( sd, sizeof sd, "%d", d );
    g_snprintf( sD, sizeof sD, "%d", D );
    if ( residuos )
        r = engine_run( dir, exe, datos, "-s", s, "--harmonics", "off", "--no-tests", NULL );
    else if ( lam == 0.0 )
        r = off ? engine_run( dir, exe, datos, "-s", s, "-l", "-d", sd, "-D", sD,
                              "--harmonics", "off", NULL )
                : engine_run( dir, exe, datos, "-s", s, "-l", "-d", sd, "-D", sD, NULL );
    else
        r = off ? engine_run( dir, exe, datos, "-s", s, "-d", sd, "-D", sD,
                              "--harmonics", "off", NULL )
                : engine_run( dir, exe, datos, "-s", s, "-d", sd, "-D", sD, NULL );
    g_free( dir );
    if ( r.status != 0 )
        {
        g_snprintf( why, n, "art no identificó: %s", r.message ? r.message : "?" );
        engine_result_clear( &r );
        return 1;
        }
    engine_result_clear( &r );
    {
    gchar *base = g_strndup( datos, strlen( datos ) - ( g_str_has_suffix( datos, ".txt" ) ? 4 : 0 ) );
    g_snprintf( cand, sizeof cand, "%s_art.cand", base );
    g_free( base );
    }
    rc = ac_leer( cand, c, &line );
    if ( rc != AC_OK )
        {
        g_snprintf( why, n, rc == AC_ENOFILE ? "art no dejó %s." :
                            rc == AC_ETRUNC ? "%s está cortado." :
                            "%s no se entiende (línea %d).", cand, line );
        return 1;
        }
    if ( c->ncand == 0 ) { g_snprintf( why, n, "art no propuso ningún candidato." ); return 1; }
    return 0;
}

int ac_corre( const char *exe, const char *datos, int freq, double lam, int d, int D,
              int residuos, ArtCand *c, int *sin_armonicos, char *why, size_t n )
{
    if ( sin_armonicos ) *sin_armonicos = 0;
    if ( !exe ) { g_snprintf( why, n, "No encuentro el motor art (engines/art/bin/art)." ); return 1; }
    if ( una( exe, datos, freq, lam, d, D, residuos, 0, c, why, n ) != 0 ) return 1;
    if ( residuos || D != 0 || freq <= 1 || !c->has_seasonal || c->seasonal_detected )
        return 0;
    if ( una( exe, datos, freq, lam, d, D, residuos, 1, c, why, n ) != 0 ) return 1;
    if ( sin_armonicos ) *sin_armonicos = 1;
    return 0;
}
